#include "GPU/buffer.h"

#include "Core/graphics_context.h"

#include "GPU/command_pool.h"
#include "GPU/command_buffer.h"
#include "GPU/persistent_staging_buffer.h"

#include "Utils/VkCheck.h"

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <cstring>
#include <stdexcept>

struct Buffer::Impl
{
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
	VmaAllocationInfo allocationInfos{};
};

static VkBufferUsageFlags ConvertToVkUsage(Buffer::E_Usage usage)
{
	VkBufferUsageFlags flags = 0;

	if ((usage & Buffer::E_Usage::TransferSrc) == Buffer::E_Usage::TransferSrc) { flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT; }
	if ((usage & Buffer::E_Usage::TransferDst) == Buffer::E_Usage::TransferDst) { flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT; }
	if ((usage & Buffer::E_Usage::VertexBuffer) == Buffer::E_Usage::VertexBuffer) { flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT; }
	if ((usage & Buffer::E_Usage::IndexBuffer) == Buffer::E_Usage::IndexBuffer) { flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT; }
	if ((usage & Buffer::E_Usage::UniformBuffer) == Buffer::E_Usage::UniformBuffer) { flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; }

	return flags;
}

Buffer::Buffer(GraphicsContext& ctx, const CreateInfos& infos) : m_pImpl(std::make_unique<Impl>()), m_ctx(&ctx), m_size(infos.sizeInBytes), m_usage(infos.usage)
{
	VkBufferCreateInfo bufferInfos{};
	bufferInfos.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfos.size = m_size;
	bufferInfos.usage = ConvertToVkUsage(m_usage);

	VmaAllocationCreateInfo allocationInfos{};
	allocationInfos.usage = VMA_MEMORY_USAGE_AUTO;

	// If this buffer is a staging buffer, it must be in CPU-visible memory.
	// Staging buffers (TransferSrc) and UBOs updated every frame (HostVisible) must
	// be in CPU-visible memory and persistently mapped to avoid an intermediate staging buffer.
	bool isHostVisible = (infos.usage & E_Usage::TransferSrc) == E_Usage::TransferSrc || (infos.usage & E_Usage::HostVisible) == E_Usage::HostVisible;
	if (isHostVisible)
	{
		allocationInfos.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	}

	VK_CHECK(vmaCreateBuffer(ctx.GetAllocator(), &bufferInfos, &allocationInfos, &m_pImpl->buffer, &m_pImpl->allocation, &m_pImpl->allocationInfos));
}

Buffer::~Buffer() noexcept
{
	vmaDestroyBuffer(m_ctx->GetAllocator(), m_pImpl->buffer, m_pImpl->allocation);
}

void Buffer::Upload(const void* data, size_t size)
{
	// If buffer is a host-visible buffer (CPU-Readable, direct writing)
	bool isHostVisible = (m_usage & E_Usage::TransferSrc) == E_Usage::TransferSrc || (m_usage & E_Usage::HostVisible) == E_Usage::HostVisible; // Check whether the TransferSrc flag is present in this buffer's flag combination.
	if (isHostVisible)
	{
		void* mappedData = m_pImpl->allocationInfos.pMappedData;
		std::memcpy(mappedData, data, size);
		return;
	}

	// Else if the buffer is device-local buffer (CPU-Unreadable, blit of the buffer)
	StagingBufferHandle stagingBufferHandle = m_ctx->GetPersistentStagingBuffer().Acquire(size);
	std::memcpy(stagingBufferHandle.GetMappedData(), data, size);

	// 3. Save the copy command
	CommandBuffer* cmdBuffer = &m_ctx->GetCommandPool().Acquire();

	VkCommandBufferBeginInfo beginInfos{};
	beginInfos.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfos.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(cmdBuffer->GetCmd(), &beginInfos);

	VkBufferCopy copyRegion{};
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = size;
	vkCmdCopyBuffer(cmdBuffer->GetCmd(), stagingBufferHandle.GetVkBuffer(), m_pImpl->buffer, 1, &copyRegion);

	vkEndCommandBuffer(cmdBuffer->GetCmd());

	// 4. Submit and wait
	VkSubmitInfo submitInfos{};
	submitInfos.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfos.commandBufferCount = 1;
	
	VkCommandBuffer vkCmdBuffer = cmdBuffer->GetCmd();
	submitInfos.pCommandBuffers = &vkCmdBuffer;

	vkQueueSubmit(m_ctx->GetGraphicsQueue(), 1, &submitInfos, VK_NULL_HANDLE);
	vkQueueWaitIdle(m_ctx->GetGraphicsQueue());

	// 5. Return the command buffer to the pool
	m_ctx->GetCommandPool().Release(*cmdBuffer);
}

size_t Buffer::GetSize() const
{
	return m_size;
}

Buffer::E_Usage Buffer::GetUsage() const
{
	return m_usage;
}

void* Buffer::GetMappedData() const
{
	return m_pImpl->allocationInfos.pMappedData;
}

VkBuffer Buffer::GetVkBuffer() const
{
	return m_pImpl->buffer;
}

Buffer::Impl& Buffer::GetImpl()
{
	return *m_pImpl;
}
