#pragma once

#include <memory>
#include <vulkan/vulkan.h>

class Window;

static constexpr uint32_t FRAMES_IN_FLIGHT = 2;

class GraphicsContext
{
public:
	GraphicsContext(Window& window);
	~GraphicsContext();

	void BeginFrame();
	void EndFrame();

	struct Impl;

	VkDevice GetDevice() const;
	uint32_t GetGraphicsQueueFamily() const;

private:
	void InitInstance();
	void InitSurface(Window& window);
	void InitDevice();
	void InitAllocator();
	void InitSwapchain(Window& window);
	void InitImages();
	void InitFrameData();

	std::unique_ptr<Impl> m_pImpl;
};
