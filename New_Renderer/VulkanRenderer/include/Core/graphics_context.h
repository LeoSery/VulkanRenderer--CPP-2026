#pragma once

#include <memory>
#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

class Window;
class CommandPool;

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
	VkPipeline GetPipeline() const;
	uint32_t GetGraphicsQueueFamily() const;
	VmaAllocator GetAllocator() const;
	CommandPool& GetCommandPool() const;
	VkQueue GetGraphicsQueue() const;

private:
	void InitInstance();
	void InitSurface(Window& window);
	void InitDevice();
	void InitAllocator();
	void InitSwapchain(Window& window);
	void InitImages();
	void InitFrameData();
	void InitPipeline();

	std::unique_ptr<Impl> m_pImpl;
};
