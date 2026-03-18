#include "Core/graphics_context.h"
#include "core/window.h"

#include <GLFW/glfw3.h>
#include <stdexcept>
	
GraphicsContext::GraphicsContext(Window& window)
{
	InitInstance();
	InitSurface(window);
	InitDevice();
}

GraphicsContext::~GraphicsContext()
{
	vkDestroyDevice(m_device, nullptr);
	vkDestroySurfaceKHR(m_vkbInstance.instance, m_surface, nullptr);
	vkb::destroy_instance(m_vkbInstance);
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
	VkResult result = glfwCreateWindowSurface(
		m_vkbInstance.instance,
		window.GetHandle(),
		nullptr,
		&m_surface
	);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create window surface");
	}
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

	vkb::PhysicalDevice vkbPhysDevice = physResult.value();
	m_physicalDevice = vkbPhysDevice.physical_device;

	vkb::DeviceBuilder deviceBuilder{ vkbPhysDevice };
	auto deviceResult = deviceBuilder.build();

	if (!deviceResult)
	{
		throw std::runtime_error("Failed to create logical device");
	}

	vkb::Device vkbDevice = deviceResult.value();
	m_device = vkbDevice.device;
	m_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	m_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();
}
