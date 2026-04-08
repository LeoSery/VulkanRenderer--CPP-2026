#include "GPU/command_pool.h"
#include "GPU/command_buffer.h"
#include "Core/graphics_context.h"

#include <vulkan/vulkan.h>
#include <stdexcept>
#include <vector>

struct PoolEntry
{
	std::unique_ptr<CommandBuffer> commandBuffer;
	bool isAvailable = true;
};

struct CommandPool::Impl
{
	VkCommandPool pool = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	std::vector<PoolEntry> entries;
};

CommandPool::CommandPool(GraphicsContext& ctx) : m_pImpl(std::make_unique<Impl>()), m_ctx(&ctx)
{
	m_pImpl->device = ctx.GetDevice();

	VkCommandPoolCreateInfo poolInfos{};
	poolInfos.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfos.queueFamilyIndex = ctx.GetGraphicsQueueFamily();
	poolInfos.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	if (vkCreateCommandPool(m_pImpl->device, &poolInfos, nullptr, &m_pImpl->pool) != VK_SUCCESS)
	{
		throw std::runtime_error("CommandPool > failed to create");
	}
}

CommandPool::~CommandPool()
{
	m_pImpl->entries.clear();
	vkDestroyCommandPool(m_pImpl->device, m_pImpl->pool, nullptr);
}

CommandBuffer& CommandPool::Aquire()
{
	// Find the first CommandBuffer whose fence is signaled (GPU finished executing it)
	for (auto& entry : m_pImpl->entries)
	{
		if (entry.isAvailable && entry.commandBuffer->IsFenceSignaled())
		{
			entry.isAvailable = false;
			return *entry.commandBuffer;
		}
	}

	auto& defaultEntry = m_pImpl->entries.emplace_back();
	defaultEntry.commandBuffer = std::unique_ptr<CommandBuffer>(new CommandBuffer(*this));
	defaultEntry.isAvailable = false;
	
	return *defaultEntry.commandBuffer;
}

void CommandPool::Release(CommandBuffer& cb)
{
	for (auto& entry : m_pImpl->entries)
	{
		if (entry.commandBuffer.get() == &cb)
		{
			entry.isAvailable = true;
			return;
		}
	}

	throw std::runtime_error("CommandPool::Release() > unknown CommandBuffer");
}

VkDevice CommandPool::GetDevice() const
{
	return m_pImpl->device;
}

VkCommandPool CommandPool::GetPool() const
{
	return m_pImpl->pool;
}

