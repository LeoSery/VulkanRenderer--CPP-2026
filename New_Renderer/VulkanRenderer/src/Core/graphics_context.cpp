#include "Core/graphics_context.h"
#include "core/window.h"
#include "Utils/VkCheck.h"
#include "Utils/VkImages.h"

#include <GLFW/glfw3.h>
#include <stdexcept>
	
GraphicsContext::GraphicsContext(Window& window)
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
	vkDeviceWaitIdle(m_device);

	// Per-FrameData
	for (auto& frame : m_frames)
	{
		vkDestroyCommandPool(m_device, frame.commandPool, nullptr);
		vkDestroyFence(m_device, frame.renderFence, nullptr);
		vkDestroySemaphore(m_device, frame.imageAvailable, nullptr);
		vkDestroySemaphore(m_device, frame.renderFinished, nullptr);
	}

	// Render targets
	vkDestroyImageView(m_device, m_backbuffer.imageView, nullptr);
	vmaDestroyImage(m_allocator, m_backbuffer.image, m_backbuffer.allocation);

	vkDestroyImageView(m_device, m_depthbuffer.imageView, nullptr);
	vmaDestroyImage(m_allocator, m_depthbuffer.image, m_depthbuffer.allocation);

	// Swapchain
	for (auto& view : m_swapchainImageViews)
	{
		vkDestroyImageView(m_device, view, nullptr);
	}
	vkb::destroy_swapchain(m_vkbSwapchain);

	// Vulkan
	vmaDestroyAllocator(m_allocator);
	vkDestroyDevice(m_device, nullptr);
	vkDestroySurfaceKHR(m_vkbInstance.instance, m_surface, nullptr);
	vkb::destroy_instance(m_vkbInstance);
}

void GraphicsContext::BeginFrame()
{
	auto& frame = m_frames[m_currentFrame];

	VK_CHECK(vkWaitForFences(m_device, 1, &frame.renderFence, VK_TRUE, UINT64_MAX));
	VK_CHECK(vkResetFences(m_device, 1, &frame.renderFence));

	VK_CHECK(vkAcquireNextImageKHR(m_device, m_vkbSwapchain.swapchain, UINT64_MAX, frame.imageAvailable, VK_NULL_HANDLE, &m_swapchainImageIndex));

	VK_CHECK(vkResetCommandBuffer(frame.commandBuffer, 0));

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	VK_CHECK(vkBeginCommandBuffer(frame.commandBuffer, &beginInfo));
}

void GraphicsContext::EndFrame()
{
	auto& frame = m_frames[m_currentFrame];
	VkCommandBuffer cmd = frame.commandBuffer;

	TransitionImage(cmd, m_backbuffer.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	VkClearColorValue clearColor{ 0.1f, 0.1f, 0.3f, 1.0f };
	VkImageSubresourceRange range{};
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.levelCount = VK_REMAINING_MIP_LEVELS;
	range.layerCount = VK_REMAINING_ARRAY_LAYERS;
	vkCmdClearColorImage(cmd, m_backbuffer.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);

	TransitionImage(cmd, m_backbuffer.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	TransitionImage(cmd, m_swapchainImages[m_swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	VkImageBlit2 blitRegion{};
	blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
	blitRegion.srcOffsets[1] = { (int32_t)m_backbuffer.extent.width, (int32_t)m_backbuffer.extent.height, 1 };
	blitRegion.dstOffsets[1] = { (int32_t)m_swapchainExtent.width,   (int32_t)m_swapchainExtent.height,   1 };
	blitRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	blitRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };

	VkBlitImageInfo2 blitInfo{};
	blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
	blitInfo.srcImage = m_backbuffer.image;
	blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	blitInfo.dstImage = m_swapchainImages[m_swapchainImageIndex];
	blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	blitInfo.regionCount = 1;
	blitInfo.pRegions = &blitRegion;
	blitInfo.filter = VK_FILTER_LINEAR;

	vkCmdBlitImage2(cmd, &blitInfo);

	TransitionImage(cmd, m_swapchainImages[m_swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkCommandBufferSubmitInfo cmdSubmit{};
	cmdSubmit.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
	cmdSubmit.commandBuffer = cmd;

	VkSemaphoreSubmitInfo waitSem{};
	waitSem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	waitSem.semaphore = frame.imageAvailable;
	waitSem.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSemaphoreSubmitInfo signalSem{};
	signalSem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
	signalSem.semaphore = frame.renderFinished;
	signalSem.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;

	VkSubmitInfo2 submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
	submitInfo.waitSemaphoreInfoCount = 1;
	submitInfo.pWaitSemaphoreInfos = &waitSem;
	submitInfo.commandBufferInfoCount = 1;
	submitInfo.pCommandBufferInfos = &cmdSubmit;
	submitInfo.signalSemaphoreInfoCount = 1;
	submitInfo.pSignalSemaphoreInfos = &signalSem;

	VK_CHECK(vkQueueSubmit2(m_graphicsQueue, 1, &submitInfo, frame.renderFence));

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &frame.renderFinished;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_vkbSwapchain.swapchain;
	presentInfo.pImageIndices = &m_swapchainImageIndex;

	VK_CHECK(vkQueuePresentKHR(m_graphicsQueue, &presentInfo));

	m_currentFrame = (m_currentFrame + 1) % FRAMES_IN_FLIGHT;
}

void GraphicsContext::InitInstance()
{
	vkb::InstanceBuilder builder;

	auto result = builder
		.set_app_name("Vulkan Renderer")
		.require_api_version(1, 3, 0)
		.request_validation_layers()
		.use_default_debug_messenger()
		.build();

	if (!result)
	{
		throw std::runtime_error("Failed to create Vulkan instance");
	}

	m_vkbInstance = result.value();
	m_debugMessenger = m_vkbInstance.debug_messenger;
}

void GraphicsContext::InitSurface(Window& window)
{
	VK_CHECK(glfwCreateWindowSurface(
		m_vkbInstance.instance,
		window.GetHandle(),
		nullptr,
		&m_surface));
}

void GraphicsContext::InitDevice()
{
	VkPhysicalDeviceFeatures features{};
	features.samplerAnisotropy = true;

	VkPhysicalDeviceVulkan13Features feature13{};
	feature13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	feature13.dynamicRendering = true;
	feature13.synchronization2 = true;

	vkb::PhysicalDeviceSelector selector{ m_vkbInstance };
	auto physResult = selector
		.set_surface(m_surface)
		.set_minimum_version(1, 3)
		.set_required_features(features)
		.set_required_features_13(feature13)
		.prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
		.select();

	if (!physResult)
	{
		throw std::runtime_error("Failed to select physical device");
	}

	vkb::DeviceBuilder deviceBuilder{ physResult.value() };
	auto deviceResult = deviceBuilder.build();

	if (!deviceResult)
	{
		throw std::runtime_error("Failed to create logical device");
	}

	vkb::Device vkbDevice = deviceResult.value();
	m_physicalDevice = physResult.value().physical_device;
	m_device = vkbDevice.device;
	m_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	m_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
}

void GraphicsContext::InitAllocator()
{
	VmaAllocatorCreateInfo info{};
	info.physicalDevice = m_physicalDevice;
	info.device = m_device;
	info.instance = m_vkbInstance.instance;

	VK_CHECK(vmaCreateAllocator(&info, &m_allocator));
}

void GraphicsContext::InitSwapchain(Window& window)
{
	int width, height;
	glfwGetFramebufferSize(window.GetHandle(), &width, &height);

	vkb::SwapchainBuilder builder{ m_physicalDevice, m_device, m_surface };
	auto result = builder
		.set_desired_format({ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(width, height)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.build();

	if (!result)
	{
		throw std::runtime_error("Failed to create swapchain");
	}

	m_vkbSwapchain = result.value();
	m_swapchainImages = m_vkbSwapchain.get_images().value();
	m_swapchainImageViews = m_vkbSwapchain.get_image_views().value();
	m_swapchainExtent = m_vkbSwapchain.extent;
}

void GraphicsContext::InitImages()
{
	{
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		imageInfo.extent = { m_swapchainExtent.width, m_swapchainExtent.height, 1 };
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		VK_CHECK(vmaCreateImage(m_allocator, &imageInfo, &allocInfo, &m_backbuffer.image, &m_backbuffer.allocation, nullptr));

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = m_backbuffer.image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VK_CHECK(vkCreateImageView(m_device, &viewInfo, nullptr, &m_backbuffer.imageView));

		m_backbuffer.extent = m_swapchainExtent;
		m_backbuffer.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	}

	{
		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = VK_FORMAT_D32_SFLOAT;
		imageInfo.extent = { m_swapchainExtent.width, m_swapchainExtent.height, 1 };
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

		VK_CHECK(vmaCreateImage(m_allocator, &imageInfo, &allocInfo, &m_depthbuffer.image, &m_depthbuffer.allocation, nullptr));

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = m_depthbuffer.image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = VK_FORMAT_D32_SFLOAT;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VK_CHECK(vkCreateImageView(m_device, &viewInfo, nullptr, &m_depthbuffer.imageView));

		m_depthbuffer.extent = m_swapchainExtent;
		m_depthbuffer.format = VK_FORMAT_D32_SFLOAT;
	}
}

void GraphicsContext::InitFrameData()
{
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = m_graphicsQueueFamily;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	VkCommandBufferAllocateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	bufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	bufferInfo.commandBufferCount = 1;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkSemaphoreCreateInfo semInfo{};
	semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	for (auto& frame : m_frames)
	{
		VK_CHECK(vkCreateCommandPool(m_device, &poolInfo, nullptr, &frame.commandPool));

		bufferInfo.commandPool = frame.commandPool;
		VK_CHECK(vkAllocateCommandBuffers(m_device, &bufferInfo, &frame.commandBuffer));

		VK_CHECK(vkCreateFence(m_device, &fenceInfo, nullptr, &frame.renderFence));
		VK_CHECK(vkCreateSemaphore(m_device, &semInfo, nullptr, &frame.imageAvailable));
		VK_CHECK(vkCreateSemaphore(m_device, &semInfo, nullptr, &frame.renderFinished));
	}
}
