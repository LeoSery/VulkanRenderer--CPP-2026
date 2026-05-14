#include "Core/graphics_context.h"
#include "Core/window.h"
#include "GPU/command_pool.h"
#include "GPU/command_buffer.h"
#include "GPU/image.h"
#include "Utils/VkCheck.h"
#include "GPU/shader.h"
#include "GPU/pipeline.h"
#include "GPU/sampler.h"
#include "GPU/descriptor_pool.h"
#include "GPU/descriptor_set.h"
#include "GPU/Buffer.h"
#include "GPU/persistent_staging_buffer.h"
#include "GPU/mesh.h"
#include "Loaders/obj_loader.h"
#include "Core/camera.h"
#include "Core/scene_data.h"
#include "Loaders/image_loader.h"

#include <vulkan/vulkan.h>
#include <VkBootstrap.h>
#include <vma/vk_mem_alloc.h>
#include <GLFW/glfw3.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <vector>
#include <stdexcept>
#include <memory>
#include <cassert>

// Anonymous namespace to keep data private outside of this file
namespace
{
	struct MVPData
	{
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 projection;
		glm::vec4 cameraPos;
	};
}

struct FrameData
{
	CommandBuffer* commandBuffer = nullptr;
	VkSemaphore isImageAvailable = VK_NULL_HANDLE;
};

struct GraphicsContext::Impl
{
	// Instance & device
	vkb::Instance vkbInstance;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkQueue graphicsQueue = VK_NULL_HANDLE;
	uint32_t graphicsQueueFamily = 0;
	VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

	// Memory
	VmaAllocator allocator = VK_NULL_HANDLE;

	// Swapchain
	vkb::Swapchain vkbSwapchain;
	std::vector<std::unique_ptr<Image>> swapchainImages;
	VkExtent2D swapchainExtent{};
	std::vector<VkSemaphore> renderFinishedSemaphores;

	// Render targets
	std::unique_ptr<Image> backbuffer;
	std::unique_ptr<Image> depthbuffer;
	VkFormat depthFormat = VK_FORMAT_UNDEFINED;

	// Pool and FrameData
	std::unique_ptr<CommandPool> commandPool;
	std::array<FrameData, FRAMES_IN_FLIGHT> frames;
	std::array<std::unique_ptr<CommandEncoder>, FRAMES_IN_FLIGHT> encoders;
	uint32_t currentFrame = 0;
	uint32_t swapchainImageIndex = 0;

	// Pipeline
	std::unique_ptr<Shader> vertexShader;
	std::unique_ptr<Shader> fragmentShader;
	std::unique_ptr<Pipeline> pipeline;

	// Texture
	std::unique_ptr<Image> texture;
	std::unique_ptr<Sampler> sampler;
	std::unique_ptr<DescriptorPool> descriptorPool;
	std::unique_ptr<DescriptorSet> descriptorSet;

	// Mesh
	std::unique_ptr<Mesh> mesh;

	// Buffer
	std::unique_ptr<PersistentStagingBuffer> stagingBuffer;

	// Camera
	std::unique_ptr<Camera> camera;

	// Lighting
	SceneData::LightData lightData{};
	std::unique_ptr<Buffer> lightUBO;

	// Time
	float lastTime = 0.0;

	// Window
	GLFWwindow* window = nullptr;
};

// Query the GPU for the best supported depth format
// We prefer D32_SFLOAT for maximum precision, with fallbacks for older hardware
static VkFormat FindCurrentDepthFormat(VkPhysicalDevice physicalDevice)
{
	// Candidate formats, in order of preference
	constexpr VkFormat candidatesFormat[] = {
		VK_FORMAT_D32_SFLOAT,         // Maximum precision, no stencil buffer
		VK_FORMAT_D32_SFLOAT_S8_UINT, // If we want a stencil buffer later on
		VK_FORMAT_D24_UNORM_S8_UINT   // Very common on desktops, slightly less precise
	};

	for (VkFormat format : candidatesFormat)
	{
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);

		if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			return format;
		}
	}

	throw std::runtime_error("GraphicsContext > FindDepthFormat(): No supported depth format found");
}
	
GraphicsContext::GraphicsContext(Window& window) : m_pImpl(std::make_unique<Impl>())
{
	InitInstance();
	InitSurface(window);
	InitDevice();
	InitAllocator();
	InitSwapchain(window);
	InitImages();
	InitFrameData();
	InitPersistentBuffer();
	InitTexture();
	InitMesh();
	InitPipeline();
	InitSceneObjects();

	m_pImpl->lastTime = glfwGetTime(); // Initialize lastTime so the first deltaTime is near zero instead of the full init duration
}

GraphicsContext::~GraphicsContext()
{
	vkDeviceWaitIdle(m_pImpl->device); // Wait for the GPU to finish all pending work before destroying anything

	m_pImpl->lightUBO.reset();
	m_pImpl->descriptorSet.reset();
	m_pImpl->descriptorPool.reset();
	m_pImpl->sampler.reset();
	m_pImpl->texture.reset();

	m_pImpl->pipeline.reset();
	m_pImpl->vertexShader.reset();
	m_pImpl->fragmentShader.reset();

	// Destroy in reverse order of construction :
	// Encoders > Semaphores > CommandPool > SwapchainImageViews > Swapchain > Allocator > Device > Surface > Instance

	// Encoders
	for (auto& encoder : m_pImpl->encoders)
	{
		encoder.reset();
	}

	// Semaphores
	for (auto& frame : m_pImpl->frames)
	{
		vkDestroySemaphore(m_pImpl->device, frame.isImageAvailable, nullptr);
	}

	for (auto& semaphore : m_pImpl->renderFinishedSemaphores)
	{
		vkDestroySemaphore(m_pImpl->device, semaphore, nullptr);
	}

	// CommandPool
	m_pImpl->commandPool.reset();

	// Image
	m_pImpl->mesh.reset();
	m_pImpl->backbuffer.reset();
	m_pImpl->depthbuffer.reset();
	m_pImpl->stagingBuffer.reset();

	// ImageViews
	m_pImpl->swapchainImages.clear();

	// Swapchain
	vkb::destroy_swapchain(m_pImpl->vkbSwapchain);

	// Allocator, Device, Surface and Instance
	vmaDestroyAllocator(m_pImpl->allocator);
	vkDestroyDevice(m_pImpl->device, nullptr);
	vkDestroySurfaceKHR(m_pImpl->vkbInstance.instance, m_pImpl->surface, nullptr);
	vkb::destroy_instance(m_pImpl->vkbInstance);
}

void GraphicsContext::BeginFrame()
{
	// DeltaTime
	double currentTime = glfwGetTime();
	float deltaTime = static_cast<float>(currentTime - m_pImpl->lastTime);
	m_pImpl->lastTime = currentTime;

	// Input
	m_pImpl->camera->ProcessInput(m_pImpl->window, deltaTime);

	auto& frame = m_pImpl->frames[m_pImpl->currentFrame];
	auto& encoder = m_pImpl->encoders[m_pImpl->currentFrame];

	// Wait for the GPU to finish the previous frame before starting a new one
	frame.commandBuffer->WaitForFence();
	frame.commandBuffer->ResetFence();

	// Request the next available image from the swapchain
	VK_CHECK(vkAcquireNextImageKHR(
		m_pImpl->device,
		m_pImpl->vkbSwapchain.swapchain,
		UINT64_MAX,
		frame.isImageAvailable,
		VK_NULL_HANDLE,
		&m_pImpl->swapchainImageIndex
	));

	// Begin recording > Writing all GPU commands into the command buffer for later submission
	encoder = frame.commandBuffer->BeginRecording();

	// Transition backbuffer layout to COLOR_ATTACHMENT - tells the GPU we are going to draw into it
	encoder->TransitionImageLayout(
		*m_pImpl->backbuffer,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	);

	// Transition depth buffer layout to DEPTH_ATTACHMENT so the GPU can read and write depth values during rendering
	encoder->TransitionImageLayout(
		*m_pImpl->depthbuffer,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
	);

	// Background clear color
	VkClearValue clearValue{};
	clearValue.color = { { 0.1f, 0.1f, 0.1f, 1.0f } };

	// Describe the color attachment > Which image to draw into, how to load and store it
	VkRenderingAttachmentInfo colorAttachementInfos{};
	colorAttachementInfos.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachementInfos.imageView = m_pImpl->backbuffer->GetVkImageView();
	colorAttachementInfos.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachementInfos.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachementInfos.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachementInfos.clearValue = clearValue;

	// Depth test configuration
	VkRenderingAttachmentInfo depthAttachementInfos{};
	depthAttachementInfos.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depthAttachementInfos.imageView = m_pImpl->depthbuffer->GetVkImageView();
	depthAttachementInfos.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
	depthAttachementInfos.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachementInfos.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachementInfos.clearValue.depthStencil = { 1.0f, 1 };

	// Describe the rendering pass > attachments, render area, and layer count
	VkRenderingInfo renderingInfos{};
	renderingInfos.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfos.renderArea.extent = m_pImpl->swapchainExtent;
	renderingInfos.layerCount = 1;
	renderingInfos.colorAttachmentCount = 1;
	renderingInfos.pColorAttachments = &colorAttachementInfos;
	renderingInfos.pDepthAttachment = &depthAttachementInfos;

	vkCmdBeginRendering(frame.commandBuffer->GetCmd(), &renderingInfos);

	// Viewport > transforms NDC coordinates (-1 to 1 from the shader) into screen pixels
	VkViewport viewportInfos{};
	viewportInfos.width = static_cast<float>(m_pImpl->swapchainExtent.width);
	viewportInfos.height = static_cast<float>(m_pImpl->swapchainExtent.height);
	viewportInfos.minDepth = 0.0f;
	viewportInfos.maxDepth = 1.0f;
	vkCmdSetViewport(frame.commandBuffer->GetCmd(), 0, 1, &viewportInfos);

	// Scissor > Discards fragments outside this pixel rectangle
	VkRect2D scissorInfo{};
	scissorInfo.extent = m_pImpl->swapchainExtent;
	vkCmdSetScissor(frame.commandBuffer->GetCmd(), 0, 1, &scissorInfo);

	// Bind the graphics pipeline > Defines shaders and render states for the following draw calls
	vkCmdBindPipeline(frame.commandBuffer->GetCmd(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pImpl->pipeline->GetPipeline());
	
	m_pImpl->lightUBO->Upload(&m_pImpl->lightData, sizeof(SceneData::LightData));

	VkDescriptorSet descriptorSet = m_pImpl->descriptorSet->GetVkDescriptorSet();
	vkCmdBindDescriptorSets(frame.commandBuffer->GetCmd(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pImpl->pipeline->GetLayout(), 0, 1, &descriptorSet, 0, nullptr);

	// Compute aspect ratio from swapchain extent to avoid distortion on non-square windows
	float aspectRatio = 0.0f;
	aspectRatio = (static_cast<float>(m_pImpl->swapchainExtent.width) / (static_cast<float>(m_pImpl->swapchainExtent.height)));

	// Set the MVP (Model/View /Projection) data
	// Build MVP matrices and push them to the vertex shader via push constants
	// model: object transform | view: camera | projection: perspective
	MVPData mvpData{};
	mvpData.model = m_pImpl->mesh->transform;
	mvpData.view = m_pImpl->camera->GetViewMatrix();
	mvpData.projection = m_pImpl->camera->GetProjectionMatrix(aspectRatio);
	mvpData.cameraPos = glm::vec4(m_pImpl->camera->GetPosition(), 0.0f);

	vkCmdPushConstants(frame.commandBuffer->GetCmd(), m_pImpl->pipeline->GetLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MVPData), &mvpData);
	
	m_pImpl->mesh->Draw(frame.commandBuffer->GetCmd());

	vkCmdEndRendering(frame.commandBuffer->GetCmd());
}

void GraphicsContext::EndFrame()
{
	FrameData& frame = m_pImpl->frames[m_pImpl->currentFrame];
	std::unique_ptr<CommandEncoder>& encoder = m_pImpl->encoders[m_pImpl->currentFrame];
	Image& swapchainImage = *m_pImpl->swapchainImages[m_pImpl->swapchainImageIndex];

	// Transition backbuffer layout to TransferSrc so it can be used as blit source
	encoder->TransitionImageLayout(
		*m_pImpl->backbuffer,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
	);

	// Transition swapchain image layout to TransferDst so it can receive the blit from the backbuffer
	encoder->TransitionImageLayout(
		swapchainImage,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	);

	// Blit (copy + format conversion) backbuffer HDR to swapchain SDR/LDR
	encoder->BlitImage(
		*m_pImpl->backbuffer,
		swapchainImage,
		m_pImpl->swapchainExtent,
		m_pImpl->swapchainExtent
	);

	// Transition swapchain image layout to PresentSrc so it can be presented to the screen
	encoder->TransitionImageLayout(
		swapchainImage,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	);

	encoder.reset();

	// Submit
	VkCommandBufferSubmitInfo cmdSubmitInfos{};
	cmdSubmitInfos.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmdSubmitInfos.commandBuffer = frame.commandBuffer->GetCmd();

	VkSemaphoreSubmitInfo waitInfos{};
	waitInfos.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	waitInfos.semaphore = frame.isImageAvailable;
	waitInfos.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

	VkSemaphoreSubmitInfo signalInfos{};
	signalInfos.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalInfos.semaphore = m_pImpl->renderFinishedSemaphores[m_pImpl->swapchainImageIndex];
	signalInfos.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;

	VkSubmitInfo2 submitInfos{};
	submitInfos.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfos.waitSemaphoreInfoCount = 1;
	submitInfos.pWaitSemaphoreInfos = &waitInfos;
	submitInfos.signalSemaphoreInfoCount = 1;
	submitInfos.pSignalSemaphoreInfos = &signalInfos;
	submitInfos.commandBufferInfoCount = 1;
	submitInfos.pCommandBufferInfos = &cmdSubmitInfos;

	VK_CHECK(vkQueueSubmit2(
		m_pImpl->graphicsQueue,
		1, 
		&submitInfos,
		frame.commandBuffer->GetFence()
	));

	VkPresentInfoKHR presentInfos{};
	presentInfos.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfos.waitSemaphoreCount = 1;
	presentInfos.pWaitSemaphores = &m_pImpl->renderFinishedSemaphores[m_pImpl->swapchainImageIndex];
	presentInfos.swapchainCount = 1;
	presentInfos.pSwapchains = &m_pImpl->vkbSwapchain.swapchain;
	presentInfos.pImageIndices = &m_pImpl->swapchainImageIndex;

	VK_CHECK(vkQueuePresentKHR(m_pImpl->graphicsQueue, &presentInfos));

	m_pImpl->currentFrame = (m_pImpl->currentFrame + 1) % FRAMES_IN_FLIGHT;
}

VkDevice GraphicsContext::GetDevice() const
{
	return m_pImpl->device;
}

VkPhysicalDevice GraphicsContext::GetPhysicalDevice() const
{
	return m_pImpl->physicalDevice;
}

VkPipeline GraphicsContext::GetPipeline() const
{
	return m_pImpl->pipeline->GetPipeline();
}

uint32_t GraphicsContext::GetGraphicsQueueFamily() const
{
	return m_pImpl->graphicsQueueFamily;
}

VmaAllocator GraphicsContext::GetAllocator() const
{
	return m_pImpl->allocator;
}

CommandPool& GraphicsContext::GetCommandPool() const
{
	return *m_pImpl->commandPool;
}

VkQueue GraphicsContext::GetGraphicsQueue() const
{
	return m_pImpl->graphicsQueue;
}

PersistentStagingBuffer& GraphicsContext::GetPersistentStagingBuffer() const
{
	return *m_pImpl->stagingBuffer;
}

void GraphicsContext::InitInstance()
{
	vkb::InstanceBuilder builder;

	auto result = builder
		.set_app_name("Vulkan Renderer")
		.request_validation_layers(true)
		.use_default_debug_messenger()
		.require_api_version(1, 3, 0)
		.build();

	if (!result)
	{
		throw std::runtime_error("GraphicsContext > Failed to create Vulkan instance");
	}

	m_pImpl->vkbInstance = result.value();
	m_pImpl->debugMessenger = m_pImpl->vkbInstance.debug_messenger;
}

void GraphicsContext::InitSurface(Window& window)
{
	m_pImpl->window = window.GetHandle();
	glfwSetInputMode(m_pImpl->window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	VK_CHECK(glfwCreateWindowSurface(
		m_pImpl->vkbInstance.instance,
		window.GetHandle(),
		nullptr,
		&m_pImpl->surface));
}

void GraphicsContext::InitDevice()
{
	// Features Vulkan
	VkPhysicalDeviceFeatures features10Infos{};
	features10Infos.samplerAnisotropy = true;

	// Features Vulkan 1.3
	VkPhysicalDeviceVulkan12Features features12Infos{};
	features12Infos.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	features12Infos.bufferDeviceAddress = true;
	features12Infos.descriptorIndexing = true;

	// Features Vulkan 1.3
	VkPhysicalDeviceVulkan13Features features13Infos{};
	features13Infos.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features13Infos.dynamicRendering = true;
	features13Infos.synchronization2 = true;

	vkb::PhysicalDeviceSelector selector{ m_pImpl->vkbInstance };
	auto physResult = selector
		.set_surface(m_pImpl->surface)
		.set_minimum_version(1, 3)
		.set_required_features(features10Infos)
		.set_required_features_12(features12Infos)
		.set_required_features_13(features13Infos)
		.prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
		.select();

	if (!physResult)
	{
		throw std::runtime_error("GraphicsContext > Failed to select physical device : " + physResult.error().message());
	}

	vkb::DeviceBuilder deviceBuilder{ physResult.value() };
	auto deviceResult = deviceBuilder.build();

	if (!deviceResult)
	{
		throw std::runtime_error("GraphicsContext > Failed to create logical device : " + deviceResult.error().message());
	}

	vkb::Device vkbDevice = deviceResult.value();

	m_pImpl->physicalDevice = physResult.value().physical_device;
	m_pImpl->device = vkbDevice.device;
	m_pImpl->graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	m_pImpl->graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	VkPhysicalDeviceProperties deviceProperties{};
	vkGetPhysicalDeviceProperties(m_pImpl->physicalDevice, &deviceProperties);

	// Ensure the GPU supports enough push constant space for our MVP matrices (3 x mat4 = 192 bytes)
	assert(deviceProperties.limits.maxPushConstantsSize >= sizeof(MVPData) && "GraphicsContext > InitDevice(): GPU does not support enough push constant space for MVPData");
}

void GraphicsContext::InitPersistentBuffer()
{
	m_pImpl->stagingBuffer = std::make_unique<PersistentStagingBuffer>(*this);
}

void GraphicsContext::InitAllocator()
{
	VmaAllocatorCreateInfo allocatorInfos{};
	allocatorInfos.physicalDevice = m_pImpl->physicalDevice;
	allocatorInfos.device = m_pImpl->device;
	allocatorInfos.instance = m_pImpl->vkbInstance.instance;
	allocatorInfos.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

	VK_CHECK(vmaCreateAllocator(&allocatorInfos, &m_pImpl->allocator));
}

void GraphicsContext::InitSwapchain(Window& window)
{
	vkb::SwapchainBuilder builder{
		m_pImpl->physicalDevice,
		m_pImpl->device,
		m_pImpl->surface
	};

	auto result = builder
	.set_desired_format(VkSurfaceFormatKHR{
		.format = VK_FORMAT_B8G8R8A8_UNORM,
		.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
		})
	.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
	.set_desired_extent(
		static_cast<uint32_t>(window.GetWidth()),
		static_cast<uint32_t>(window.GetHeight())
	)
	.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
	.build();

	if (!result)
	{
		throw std::runtime_error("GraphicsContext > Failed to create swapchain");
	}

	m_pImpl->vkbSwapchain = result.value();
	m_pImpl->swapchainExtent = m_pImpl->vkbSwapchain.extent;

	std::vector<VkImage> vkImages = m_pImpl->vkbSwapchain.get_images().value();
	std::vector<VkImageView> vkImageViews = m_pImpl->vkbSwapchain.get_image_views().value();
	VkFormat format = m_pImpl->vkbSwapchain.image_format;

	for (size_t i = 0; i < vkImages.size(); i++)
	{
		m_pImpl->swapchainImages.push_back(std::make_unique<Image>(*this, vkImages[i], vkImageViews[i], format, m_pImpl->swapchainExtent.width, m_pImpl->swapchainExtent.height));
	}
}

void GraphicsContext::InitImages()
{
	VkExtent2D extent = m_pImpl->swapchainExtent;

	m_pImpl->depthFormat = FindCurrentDepthFormat(m_pImpl->physicalDevice);

	// Backbuffer > Intermediate render target
	// We render into this image instead of directly into the swapchain.
	// Once rendering is done, we blit (Copy) it to the swapchain image for presentation (Show).
	// Using an intermediate buffer allows us to render at a different resolution than the swapchain, and to use HDR formats the swapchain may not support.
	Image::CreateInfos backBufferCreateInfos{};
	backBufferCreateInfos.width = extent.width;
	backBufferCreateInfos.height = extent.height;
	backBufferCreateInfos.format = BACKBUFFER_FORMAT;
	backBufferCreateInfos.usage = Image::E_Usage::ColorAttachment | Image::E_Usage::TransferSrc | Image::E_Usage::TransferDst | Image::E_Usage::Storage;
	backBufferCreateInfos.genMips = false;
	m_pImpl->backbuffer = std::make_unique<Image>(*this, backBufferCreateInfos);

	// Depthbuffer > Stores the depth (distance from camera) of each rendered fragment
	// When two triangles overlap, the depth buffer determines which one is in front.
	// Unlike the backbuffer, it is never presented, only used internally by the GPU during rendering.
	Image::CreateInfos depthBufferCreateInfos{};
	depthBufferCreateInfos.width = extent.width;
	depthBufferCreateInfos.height = extent.height;
	depthBufferCreateInfos.format = m_pImpl->depthFormat;
	depthBufferCreateInfos.usage = Image::E_Usage::DepthAttachement;
	depthBufferCreateInfos.genMips = false;
	m_pImpl->depthbuffer = std::make_unique<Image>(*this, depthBufferCreateInfos);
}

void GraphicsContext::InitFrameData()
{
	m_pImpl->commandPool = std::make_unique<CommandPool>(*this);

	VkSemaphoreCreateInfo semaphoreInfos{};
	semaphoreInfos.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	// Pre-allocate one CommandBuffer and two semaphores per frame in flight
	for (auto& frame : m_pImpl->frames)
	{
		frame.commandBuffer = &m_pImpl->commandPool->Acquire();
		VK_CHECK(vkCreateSemaphore(m_pImpl->device, &semaphoreInfos, nullptr, &frame.isImageAvailable));
	}

	uint32_t imageCount = static_cast<uint32_t>(m_pImpl->swapchainImages.size());
	m_pImpl->renderFinishedSemaphores.resize(imageCount);

	for (auto& semaphore : m_pImpl->renderFinishedSemaphores)
	{
		VK_CHECK(vkCreateSemaphore(m_pImpl->device, &semaphoreInfos, nullptr, &semaphore));
	}
}

void GraphicsContext::InitPipeline()
{
	m_pImpl->vertexShader = std::make_unique<Shader>(*this, "shaders/basic.vert.spv", ShaderStage::Vertex);
	m_pImpl->fragmentShader = std::make_unique<Shader>(*this, "shaders/basic.frag.spv", ShaderStage::Fragment);

	// Declare the push constant range for the MVP matrices
	// Only the vertex shader needs it - size must match the MVPData struct exactly
	VkPushConstantRange MVPRangeInfos{};
	MVPRangeInfos.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // Only the vertex shader can read the MVP
	MVPRangeInfos.offset = 0;
	MVPRangeInfos.size = sizeof(MVPData);

	m_pImpl->pipeline = std::make_unique<Pipeline>(*this, *m_pImpl->vertexShader, *m_pImpl->fragmentShader, *m_pImpl->descriptorSet, m_pImpl->depthFormat, MVPRangeInfos);
}

void GraphicsContext::InitTexture()
{
	// Load Mesh texture
	ImageLoader::ImageData imageData = ImageLoader::Load("assets/Textures/FrogThisWay/Tx_Frogv1_D.jpg");

	// Create Image
	Image::CreateInfos imageCreateInfos{};
	imageCreateInfos.width = imageData.width;
	imageCreateInfos.height = imageData.height;
	imageCreateInfos.format = VK_FORMAT_R8G8B8A8_SRGB;
	imageCreateInfos.usage = Image::E_Usage::TransferDst | Image::E_Usage::Sampled;
	imageCreateInfos.genMips = true;

	m_pImpl->texture = std::make_unique<Image>(*this, imageCreateInfos);
	m_pImpl->texture->Upload(imageData.pixels.data(), imageData.width, imageData.height);

	// Create sampler
	Sampler::CreateInfos samplerInfos{};
	m_pImpl->sampler = std::make_unique<Sampler>(*this, samplerInfos);

	// Create DescriptorPool
	DescriptorPool::CreateInfos descriptorPoolCreateInfos{};
	m_pImpl->descriptorPool = std::make_unique<DescriptorPool>(*this, descriptorPoolCreateInfos);

	// Create DescriptorSet
	DescriptorSet::CreateInfos descriptorSetCreateInfos{};
	descriptorSetCreateInfos.pool = m_pImpl->descriptorPool.get();

	m_pImpl->descriptorSet = std::make_unique<DescriptorSet>(*this, descriptorSetCreateInfos);
	m_pImpl->descriptorSet->Bind<Image>(0, *m_pImpl->texture, m_pImpl->sampler.get());
}

void GraphicsContext::InitMesh()
{
	// Frog Mesh from "FrogThisWay" game.
	m_pImpl->mesh = ObjLoader::Load(*this, "assets/Meshs/FrogThisWay/Frog.obj"); 
	m_pImpl->mesh->transform = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // Rotate the mesh to 180 for toward camera
}

void GraphicsContext::InitSceneObjects()
{
	// Camera
	Camera::CreateInfos cameraInfos{};
	m_pImpl->camera = std::make_unique<Camera>(cameraInfos);
	m_pImpl->camera->SetPosition({0.0f, 0.0f, -3.0f});
	m_pImpl->camera->SetRotation(90.0f, 0.0f);

	// Ligths
	m_pImpl->lightData.lightDirection = glm::vec3(1.0f, 2.0f, -1.0f);
	m_pImpl->lightData.ambientStrength = 0.15f;
	m_pImpl->lightData.lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
	m_pImpl->lightData.specularStrength = 0.5f;
	m_pImpl->lightData.shininess = 32.0f;

	Buffer::CreateInfos lightBufferInfos{};
	lightBufferInfos.sizeInBytes = sizeof(SceneData::LightData);
	lightBufferInfos.usage = Buffer::E_Usage::UniformBuffer | Buffer::E_Usage::HostVisible;
	m_pImpl->lightUBO = std::make_unique<Buffer>(*this, lightBufferInfos);

	m_pImpl->lightUBO->Upload(&m_pImpl->lightData, sizeof(SceneData::LightData));
	m_pImpl->descriptorSet->Bind<Buffer>(1, *m_pImpl->lightUBO);
}

SceneData::LightData& GraphicsContext::GetLightData()
{
	return m_pImpl->lightData;
}
