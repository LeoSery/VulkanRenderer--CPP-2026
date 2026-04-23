#include "Core/graphics_context.h"
#include "Core/window.h"
#include "GPU/command_pool.h"
#include "GPU/command_buffer.h"
#include "GPU/image.h"
#include "Utils/VkCheck.h"
#include "GPU/shader.h"
#include "GPU/pipeline.h"

#include <vulkan/vulkan.h>
#include <VkBootstrap.h>
#include <vma/vk_mem_alloc.h>
#include <GLFW/glfw3.h>

#include <array>
#include <vector>
#include <stdexcept>

struct FrameData
{
	CommandBuffer* commandBuffer = nullptr;
	VkSemaphore isImageAvailable = VK_NULL_HANDLE;
	VkSemaphore isRenderFinished = VK_NULL_HANDLE;
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
	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;
	VkExtent2D swapchainExtent{};

	// Render targets
	Image backbuffer;
	Image depthbuffer;

	// Pool and FrameData
	std::unique_ptr<CommandPool> commandPool;
	std::array<FrameData, FRAMES_IN_FLIGHT> frames;
	std::array<std::unique_ptr<CommandEncoder>, FRAMES_IN_FLIGHT> encoders;
	uint32_t currentFrame = 0;
	uint32_t swapchainImageIndex = 0;

	//Pipeline
	std::unique_ptr<Shader> vertexShader;
	std::unique_ptr<Shader> fragmentShader;
	std::unique_ptr<Pipeline> pipeline;
};
	
GraphicsContext::GraphicsContext(Window& window) : m_pImpl(std::make_unique<Impl>())
{
	InitInstance();
	InitSurface(window);
	InitDevice();
	InitAllocator();
	InitSwapchain(window);
	InitImages();
	InitFrameData();
	InitPipeline();
}

GraphicsContext::~GraphicsContext()
{
	vkDeviceWaitIdle(m_pImpl->device); // Wait for the GPU to finish all pending work before destroying anything

	// Destroy in reverse order of construction :
	// Encoders > Semaphores > CommandPool > Backbuffer > Depthbuffer > SwapchainImageViews > Swapchain > Allocator > Device > Surface > Instance

	// Encoders
	for (auto& encoder : m_pImpl->encoders)
	{
		encoder.reset();
	}

	// Semaphores
	for (auto& frame : m_pImpl->frames)
	{
		vkDestroySemaphore(m_pImpl->device, frame.isImageAvailable, nullptr);
		vkDestroySemaphore(m_pImpl->device, frame.isRenderFinished, nullptr);
	}

	// CommandPool
	m_pImpl->commandPool.reset();

	// BackBuffer
	vkDestroyImageView(m_pImpl->device, m_pImpl->backbuffer.imageView, nullptr);
	vmaDestroyImage(m_pImpl->allocator, m_pImpl->backbuffer.image, m_pImpl->backbuffer.allocation);

	// DepthBuffer
	vkDestroyImageView(m_pImpl->device, m_pImpl->depthbuffer.imageView, nullptr);
	vmaDestroyImage(m_pImpl->allocator, m_pImpl->depthbuffer.image, m_pImpl->depthbuffer.allocation);

	// ImageViews
	for (auto& view : m_pImpl->swapchainImageViews)
	{
		vkDestroyImageView(m_pImpl->device, view, nullptr);
	}

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
		m_pImpl->backbuffer.image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	);

	// Background clear color
	VkClearValue clearValue{};
	clearValue.color = { { 0.1f, 0.1f, 0.1f, 1.0f } };

	// Describe the color attachment > Which image to draw into, how to load and store it
	VkRenderingAttachmentInfo colorAttachementInfos{};
	colorAttachementInfos.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachementInfos.imageView = m_pImpl->backbuffer.imageView;
	colorAttachementInfos.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachementInfos.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachementInfos.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachementInfos.clearValue = clearValue;

	// Describe the rendering pass > attachments, render area, and layer count
	VkRenderingInfo renderingInfos{};
	renderingInfos.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfos.renderArea.extent = m_pImpl->swapchainExtent;
	renderingInfos.layerCount = 1;
	renderingInfos.colorAttachmentCount = 1;
	renderingInfos.pColorAttachments = &colorAttachementInfos;

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
	vkCmdDraw(frame.commandBuffer->GetCmd(), 3, 1, 0, 0); // "3, 1, 0, 0" > 3 Vertices, 1 instance, 0 is for first vertex, 0 is for first instance

	vkCmdEndRendering(frame.commandBuffer->GetCmd());
}

void GraphicsContext::EndFrame()
{
	auto& frame = m_pImpl->frames[m_pImpl->currentFrame];
	auto& encoder = m_pImpl->encoders[m_pImpl->currentFrame];
	auto swapchainImage = m_pImpl->swapchainImages[m_pImpl->swapchainImageIndex];

	// Transition backbuffer layout to TransferSrc so it can be used as blit source
	encoder->TransitionImageLayout(
		m_pImpl->backbuffer.image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
	);

	encoder->TransitionImageLayout(
		swapchainImage,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	);

	// Blit (copy + format conversion) backbuffer HDR to swapchain SDR/LDR
	encoder->BlitImage(
		m_pImpl->backbuffer.image,
		swapchainImage,
		m_pImpl->swapchainExtent,
		m_pImpl->swapchainExtent
	);

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
	signalInfos.semaphore = frame.isRenderFinished;
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
	presentInfos.pWaitSemaphores = &frame.isRenderFinished;
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

VkPipeline GraphicsContext::GetPipeline() const
{
	return m_pImpl->pipeline->GetPipeline();
}

uint32_t GraphicsContext::GetGraphicsQueueFamily() const
{
	return m_pImpl->graphicsQueueFamily;
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
	m_pImpl->swapchainImages = m_pImpl->vkbSwapchain.get_images().value();
	m_pImpl->swapchainImageViews = m_pImpl->vkbSwapchain.get_image_views().value();
	m_pImpl->swapchainExtent = m_pImpl->vkbSwapchain.extent;
}

void GraphicsContext::InitImages()
{
	VkExtent2D extent = m_pImpl->swapchainExtent;

	//Backbuffer > Intermediate render target
	// We render into this image instead of directly into the swapchain.
	// Once rendering is done, we blit (Copy) it to the swapchain image for presentation (Show).
	// Using an intermediate buffer allows us to render at a different resolution than the swapchain, and to use HDR formats the swapchain may not support.
	VkImageCreateInfo backbufferInfos{};
	backbufferInfos.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	backbufferInfos.imageType = VK_IMAGE_TYPE_2D;
	backbufferInfos.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	backbufferInfos.extent = { extent.width, extent.height, 1 };
	backbufferInfos.mipLevels = 1;
	backbufferInfos.arrayLayers = 1;
	backbufferInfos.samples = VK_SAMPLE_COUNT_1_BIT;
	backbufferInfos.tiling = VK_IMAGE_TILING_OPTIMAL;
	backbufferInfos.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
		| VK_IMAGE_USAGE_TRANSFER_DST_BIT
		| VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
		| VK_IMAGE_USAGE_STORAGE_BIT;

	VmaAllocationCreateInfo backbufferAllocInfos{};
	backbufferAllocInfos.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	backbufferAllocInfos.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	VK_CHECK(vmaCreateImage(
		m_pImpl->allocator,
		&backbufferInfos,
		&backbufferAllocInfos,
		&m_pImpl->backbuffer.image,
		&m_pImpl->backbuffer.allocation,
		nullptr
	));

	VkImageViewCreateInfo backbufferViewInfos{};
	backbufferViewInfos.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	backbufferViewInfos.image = m_pImpl->backbuffer.image;
	backbufferViewInfos.viewType = VK_IMAGE_VIEW_TYPE_2D;
	backbufferViewInfos.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	backbufferViewInfos.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	backbufferViewInfos.subresourceRange.baseMipLevel = 0;
	backbufferViewInfos.subresourceRange.levelCount = 1;
	backbufferViewInfos.subresourceRange.baseArrayLayer = 0;
	backbufferViewInfos.subresourceRange.layerCount = 1;

	VK_CHECK(vkCreateImageView(
		m_pImpl->device,
		&backbufferViewInfos,
		nullptr,
		&m_pImpl->backbuffer.imageView
	));

	//Depthbuffer > Stores the depth (distance from camera) of each rendered fragment
	// When two triangles overlap, the depth buffer determines which one is in front.
	// Unlike the backbuffer, it is never presented, only used internally by the GPU during rendering.
	VkImageCreateInfo depthbufferInfos{};
	depthbufferInfos.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	depthbufferInfos.imageType = VK_IMAGE_TYPE_2D;
	depthbufferInfos.format = VK_FORMAT_D32_SFLOAT;
	depthbufferInfos.extent = { extent.width, extent.height, 1 };
	depthbufferInfos.mipLevels = 1;
	depthbufferInfos.arrayLayers = 1;
	depthbufferInfos.samples = VK_SAMPLE_COUNT_1_BIT;
	depthbufferInfos.tiling = VK_IMAGE_TILING_OPTIMAL;
	depthbufferInfos.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VmaAllocationCreateInfo depthAllocInfos{};
	depthAllocInfos.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	depthAllocInfos.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	VK_CHECK(vmaCreateImage(
		m_pImpl->allocator,
		&depthbufferInfos,
		&depthAllocInfos,
		&m_pImpl->depthbuffer.image,
		&m_pImpl->depthbuffer.allocation,
		nullptr
	));

	VkImageViewCreateInfo depthViewInfos{};
	depthViewInfos.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	depthViewInfos.image = m_pImpl->depthbuffer.image;
	depthViewInfos.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depthViewInfos.format = VK_FORMAT_D32_SFLOAT;
	depthViewInfos.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthViewInfos.subresourceRange.baseMipLevel = 0;
	depthViewInfos.subresourceRange.levelCount = 1;
	depthViewInfos.subresourceRange.baseArrayLayer = 0;
	depthViewInfos.subresourceRange.layerCount = 1;

	VK_CHECK(vkCreateImageView(
		m_pImpl->device,
		&depthViewInfos,
		nullptr,
		&m_pImpl->depthbuffer.imageView
	));
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
		VK_CHECK(vkCreateSemaphore(m_pImpl->device, &semaphoreInfos, nullptr, &frame.isRenderFinished));
	}
}

void GraphicsContext::InitPipeline()
{
	// Assign each shader to pipeline
	m_pImpl->vertexShader = std::make_unique<Shader>(*this, "shaders/basic.vert.spv", ShaderStage::Vertex);
	m_pImpl->fragmentShader = std::make_unique<Shader>(*this, "shaders/basic.frag.spv", ShaderStage::Fragment);
	m_pImpl->pipeline = std::make_unique<Pipeline>(*this, *m_pImpl->vertexShader, *m_pImpl->fragmentShader);
}
