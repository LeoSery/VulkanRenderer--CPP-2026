#pragma once

#include <memory>
#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

class Window;
class CommandPool;
class PersistentStagingBuffer;
class Scene;
class DescriptorPool;

static constexpr uint32_t FRAMES_IN_FLIGHT = 2;
static constexpr VkFormat BACKBUFFER_FORMAT = VK_FORMAT_R16G16B16A16_SFLOAT;

class GraphicsContext
{
public:
	GraphicsContext(Window& window);
	~GraphicsContext();

	void BeginFrame();
	void RenderScene(Scene& scene);
	void EndFrame();

	struct Impl;

	VkDevice GetDevice() const;
	VkPhysicalDevice GetPhysicalDevice() const;
	VkPipeline GetPipeline() const;
	uint32_t GetGraphicsQueueFamily() const;
	VmaAllocator GetAllocator() const;
	CommandPool& GetCommandPool() const;
	VkQueue GetGraphicsQueue() const;
	PersistentStagingBuffer& GetPersistentStagingBuffer() const;
	DescriptorPool& GetDescriptorPool() const;

	float GetDeltaTime() const;
	void SetScene(Scene& scene);
	void WaitIdle() const;

private:
	void InitInstance();
	void InitSurface(Window& window);
	void InitDevice();
	void InitPersistentBuffer();
	void InitAllocator();
	void InitSwapchain();
	void RecreateSwapchain();
	void InitImages();
	void InitFrameData();
	void InitDescriptorPool();
	void InitSceneDescriptors();
	void InitPipeline();
	void InitDebugUI();

	std::unique_ptr<Impl> m_pImpl;
};
