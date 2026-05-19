#pragma once

#include <memory>

#include <vulkan/vulkan.h>

class GraphicsContext;
class CommandBuffer;

class CommandPool
{
public:
	explicit CommandPool(GraphicsContext& ctx);
	~CommandPool();

	CommandBuffer& Acquire();
	void Release(CommandBuffer& cb);

	CommandPool(const CommandPool&) = delete;
	CommandPool& operator= (const CommandPool&) = delete;

	VkDevice GetDevice() const;
	VkCommandPool GetPool() const;

private:
	GraphicsContext* m_ctx;
	struct Impl;
	std::unique_ptr<Impl> m_pImpl;
};
