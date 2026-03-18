#pragma once

#include <vulkan/vulkan.h>
#include <VkBootstrap.h>

class Window;

class GraphicsContext
{
public:
	GraphicsContext(Window& window);
	~GraphicsContext();

private:
	void InitInstance();
	void InitSurface(Window& window);
	void InitDevice();

	vkb::Instance m_vkbInstance;

	VkSurfaceKHR m_surface = VK_NULL_HANDLE;
	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	VkDevice m_device = VK_NULL_HANDLE;
	VkQueue m_graphicsQueue = VK_NULL_HANDLE;
	uint32_t m_graphicsQueueFamily = 0;

	VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
};