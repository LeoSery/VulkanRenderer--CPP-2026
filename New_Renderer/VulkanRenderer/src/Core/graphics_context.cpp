#include "Core/graphics_context.h"
#include "Core/window.h"
#include "GPU/command_pool.h"
#include "GPU/command_buffer.h"
#include "GPU/image.h"
#include "Utils/VkCheck.h"

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
}

GraphicsContext::~GraphicsContext()
{
	vkDeviceWaitIdle(m_pImpl->device);

	for (auto& encoder : m_pImpl->encoders)
	{
		encoder.reset();
	}

	for (auto& frame : m_pImpl->frames)
	{
		vkDestroySemaphore(m_pImpl->device, frame.isImageAvailable, nullptr);
		vkDestroySemaphore(m_pImpl->device, frame.isRenderFinished, nullptr);
	}

	m_pImpl->commandPool.reset();

	vkDestroyImageView(m_pImpl->device, m_pImpl->backbuffer.imageView, nullptr);
	vmaDestroyImage(m_pImpl->allocator, m_pImpl->backbuffer.image, m_pImpl->backbuffer.allocation);

	vkDestroyImageView(m_pImpl->device, m_pImpl->depthbuffer.imageView, nullptr);
	vmaDestroyImage(m_pImpl->allocator, m_pImpl->depthbuffer.image, m_pImpl->depthbuffer.allocation);

	for (auto& view : m_pImpl->swapchainImageViews)
	{
		vkDestroyImageView(m_pImpl->device, view, nullptr);
	}

	vkb::destroy_swapchain(m_pImpl->vkbSwapchain);

	vmaDestroyAllocator(m_pImpl->allocator);
	vkDestroyDevice(m_pImpl->device, nullptr);
	vkDestroySurfaceKHR(m_pImpl->vkbInstance.instance, m_pImpl->surface, nullptr);
	vkb::destroy_instance(m_pImpl->vkbInstance);
}

void GraphicsContext::BeginFrame()
{
	auto& frame = m_pImpl->frames[m_pImpl->currentFrame];
	auto& encoder = m_pImpl->encoders[m_pImpl->currentFrame];

	frame.commandBuffer->WaitForFence();
	frame.commandBuffer->ResetFence();

	VK_CHECK(vkAcquireNextImageKHR(
		m_pImpl->device,
		m_pImpl->vkbSwapchain.swapchain,
		UINT64_MAX,
		frame.isImageAvailable,
		VK_NULL_HANDLE,
		&m_pImpl->swapchainImageIndex
	));

	encoder = frame.commandBuffer->BeginRecording();

	encoder->TransitionImageLayout(
		m_pImpl->backbuffer.image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
	);

	encoder->ClearColor(m_pImpl->backbuffer.image, 0.05f, 0.05f, 0.2f, 1.0f);
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
	VkCommandBufferSubmitInfo cmdSubmitInfo{};
	cmdSubmitInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmdSubmitInfo.commandBuffer = frame.commandBuffer->GetCmd();

	VkSemaphoreSubmitInfo waitInfo{};
	waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	waitInfo.semaphore = frame.isImageAvailable;
	waitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

	VkSemaphoreSubmitInfo signalInfo{};
	signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalInfo.semaphore = frame.isRenderFinished;
	signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;

	VkSubmitInfo2 submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo.waitSemaphoreInfoCount = 1;
	submitInfo.pWaitSemaphoreInfos = &waitInfo;
	submitInfo.signalSemaphoreInfoCount = 1;
	submitInfo.pSignalSemaphoreInfos = &signalInfo;
	submitInfo.commandBufferInfoCount = 1;
	submitInfo.pCommandBufferInfos = &cmdSubmitInfo;

	VK_CHECK(vkQueueSubmit2(
		m_pImpl->graphicsQueue,
		1, 
		&submitInfo,
		frame.commandBuffer->GetFence()
	));

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &frame.isRenderFinished;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_pImpl->vkbSwapchain.swapchain;
	presentInfo.pImageIndices = &m_pImpl->swapchainImageIndex;

	VK_CHECK(vkQueuePresentKHR(m_pImpl->graphicsQueue, &presentInfo));

	m_pImpl->currentFrame = (m_pImpl->currentFrame + 1) % FRAMES_IN_FLIGHT;
}

VkDevice GraphicsContext::GetDevice() const
{
	return m_pImpl->device;
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
	VkPhysicalDeviceFeatures features10{};
	features10.samplerAnisotropy = true;

	// Features Vulkan 1.3
	VkPhysicalDeviceVulkan12Features features12{};
	features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	features12.bufferDeviceAddress = true;
	features12.descriptorIndexing = true;

	// Features Vulkan 1.3
	VkPhysicalDeviceVulkan13Features features13{};
	features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features13.dynamicRendering = true;
	features13.synchronization2 = true;

	vkb::PhysicalDeviceSelector selector{ m_pImpl->vkbInstance };
	auto physResult = selector
		.set_surface(m_pImpl->surface)
		.set_minimum_version(1, 3)
		.set_required_features(features10)
		.set_required_features_12(features12)
		.set_required_features_13(features13)
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
	VmaAllocatorCreateInfo allocatorInfo{};
	allocatorInfo.physicalDevice = m_pImpl->physicalDevice;
	allocatorInfo.device = m_pImpl->device;
	allocatorInfo.instance = m_pImpl->vkbInstance.instance;
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

	VK_CHECK(vmaCreateAllocator(&allocatorInfo, &m_pImpl->allocator));
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

	//Backbuffer
	VkImageCreateInfo backbufferInfo{};
	backbufferInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	backbufferInfo.imageType = VK_IMAGE_TYPE_2D;
	backbufferInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	backbufferInfo.extent = { extent.width, extent.height, 1 };
	backbufferInfo.mipLevels = 1;
	backbufferInfo.arrayLayers = 1;
	backbufferInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	backbufferInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	backbufferInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
		| VK_IMAGE_USAGE_TRANSFER_DST_BIT
		| VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
		| VK_IMAGE_USAGE_STORAGE_BIT;

	VmaAllocationCreateInfo backbufferAllocInfo{};
	backbufferAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	backbufferAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	VK_CHECK(vmaCreateImage(
		m_pImpl->allocator,
		&backbufferInfo,
		&backbufferAllocInfo,
		&m_pImpl->backbuffer.image,
		&m_pImpl->backbuffer.allocation,
		nullptr
	));

	VkImageViewCreateInfo backbufferViewInfo{};
	backbufferViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	backbufferViewInfo.image = m_pImpl->backbuffer.image;
	backbufferViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	backbufferViewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	backbufferViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	backbufferViewInfo.subresourceRange.baseMipLevel = 0;
	backbufferViewInfo.subresourceRange.levelCount = 1;
	backbufferViewInfo.subresourceRange.baseArrayLayer = 0;
	backbufferViewInfo.subresourceRange.layerCount = 1;

	VK_CHECK(vkCreateImageView(
		m_pImpl->device,
		&backbufferViewInfo,
		nullptr,
		&m_pImpl->backbuffer.imageView
	));

	//DepthBuffer
	VkImageCreateInfo depthbufferInfo{};
	depthbufferInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	depthbufferInfo.imageType = VK_IMAGE_TYPE_2D;
	depthbufferInfo.format = VK_FORMAT_D32_SFLOAT;
	depthbufferInfo.extent = { extent.width, extent.height, 1 };
	depthbufferInfo.mipLevels = 1;
	depthbufferInfo.arrayLayers = 1;
	depthbufferInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	depthbufferInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	depthbufferInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VmaAllocationCreateInfo depthAllocInfo{};
	depthAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	depthAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	VK_CHECK(vmaCreateImage(
		m_pImpl->allocator,
		&depthbufferInfo,
		&depthAllocInfo,
		&m_pImpl->depthbuffer.image,
		&m_pImpl->depthbuffer.allocation,
		nullptr
	));

	VkImageViewCreateInfo depthViewInfo{};
	depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	depthViewInfo.image = m_pImpl->depthbuffer.image;
	depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depthViewInfo.format = VK_FORMAT_D32_SFLOAT;
	depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthViewInfo.subresourceRange.baseMipLevel = 0;
	depthViewInfo.subresourceRange.levelCount = 1;
	depthViewInfo.subresourceRange.baseArrayLayer = 0;
	depthViewInfo.subresourceRange.layerCount = 1;

	VK_CHECK(vkCreateImageView(
		m_pImpl->device,
		&depthViewInfo,
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
		frame.commandBuffer = &m_pImpl->commandPool->Aquire();

		VK_CHECK(vkCreateSemaphore(m_pImpl->device, &semaphoreInfos, nullptr, &frame.isImageAvailable));
		VK_CHECK(vkCreateSemaphore(m_pImpl->device, &semaphoreInfos, nullptr, &frame.isRenderFinished));
	}
}
