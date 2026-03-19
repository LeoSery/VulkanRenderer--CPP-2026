#pragma once

#include "GPU/image.h"

#include <vulkan/vulkan.h>
#include <VkBootstrap.h>
#include <vma/vk_mem_alloc.h>

#include <array>
#include <vector>

class Window;

static constexpr uint32_t FRAMES_IN_FLIGHT = 2;

struct PerFrameData
{
	VkCommandPool commandPool = VK_NULL_HANDLE;
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
	VkFence renderFence = VK_NULL_HANDLE;
	VkSemaphore imageAvailable = VK_NULL_HANDLE;
	VkSemaphore renderFinished = VK_NULL_HANDLE;
};

class GraphicsContext
{
public:
	GraphicsContext(Window& window);
	~GraphicsContext();

	void BeginFrame();
	void EndFrame();

private:
	void InitInstance();
	void InitSurface(Window& window);
	void InitDevice();
	void InitAllocator();
	void InitSwapchain(Window& window);
	void InitImages();
	void InitFrameData();

	// Devices
	vkb::Instance m_vkbInstance;
	VkSurfaceKHR m_surface = VK_NULL_HANDLE;
	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	VkDevice m_device = VK_NULL_HANDLE;
	VkQueue m_graphicsQueue = VK_NULL_HANDLE;
	uint32_t m_graphicsQueueFamily = 0;
	VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;

	// Memory
	VmaAllocator m_allocator = VK_NULL_HANDLE;

	// Swapchain
	vkb::Swapchain m_vkbSwapchain;
	std::vector<VkImage> m_swapchainImages;
	std::vector<VkImageView> m_swapchainImageViews;
	VkExtent2D m_swapchainExtent{};

	// Render targets
	Image m_backbuffer;
	Image m_depthbuffer;

	// Per-frame
	std::array<PerFrameData, FRAMES_IN_FLIGHT> m_frames;
	uint32_t m_currentFrame = 0;
	uint32_t m_swapchainImageIndex = 0;
};
