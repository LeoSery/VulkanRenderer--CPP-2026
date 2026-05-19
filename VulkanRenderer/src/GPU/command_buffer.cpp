#include "GPU/command_buffer.h"

#include "GPU/command_pool.h"
#include "GPU/image.h"

#include "Utils/VkCheck.h"

#include <vulkan/vulkan.h>

#include <stdexcept>

// Pimpl
struct CommandEncoder::Impl
{
	VkCommandBuffer cmd = VK_NULL_HANDLE;
};

struct CommandBuffer::Impl
{
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	VkSemaphore semaphore = VK_NULL_HANDLE;
};

// CommandEncoder
CommandEncoder::CommandEncoder(CommandBuffer& owner) : m_pImpl(std::make_unique<Impl>())
{
	m_pImpl->cmd = owner.GetCmd();

	VkCommandBufferBeginInfo beginInfos{};
	beginInfos.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfos.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK(vkBeginCommandBuffer(m_pImpl->cmd, &beginInfos));
}

CommandEncoder::~CommandEncoder()
{
	if (m_pImpl && m_pImpl->cmd != VK_NULL_HANDLE)
	{
		vkEndCommandBuffer(m_pImpl->cmd);
	}
}

void CommandEncoder::TransitionImageLayout(Image& image, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkImageMemoryBarrier2 barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
	barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
	barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.image = image.GetVkImage();
	barrier.subresourceRange.aspectMask = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	VkDependencyInfo dep{};
	dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dep.imageMemoryBarrierCount = 1;
	dep.pImageMemoryBarriers = &barrier;

	vkCmdPipelineBarrier2(m_pImpl->cmd, &dep);
}

void CommandEncoder::ClearColor(Image& image, float r, float g, float b, float a)
{
	VkClearColorValue clearColor = { { r, g, b, a } };

	VkImageSubresourceRange range{};
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.baseMipLevel = 0;
	range.levelCount = 1;
	range.baseArrayLayer = 0;
	range.layerCount = 1;

	vkCmdClearColorImage(
		m_pImpl->cmd,
		image.GetVkImage(),
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		&clearColor,
		1,
		&range
	);
}

void CommandEncoder::BlitImage(Image& src, Image& dst, VkExtent2D srcExtent, VkExtent2D dstExtent)
{
	VkImageBlit2 blitRegion{};
	blitRegion.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
	blitRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	blitRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	blitRegion.srcOffsets[0] = { 0, 0, 0 };
	blitRegion.srcOffsets[1] = { (int32_t)srcExtent.width, (int32_t)srcExtent.height, 1 };
	blitRegion.dstOffsets[0] = { 0, 0, 0 };
	blitRegion.dstOffsets[1] = { (int32_t)dstExtent.width, (int32_t)dstExtent.height, 1 };

	VkBlitImageInfo2 blitInfo{};
	blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
	blitInfo.srcImage = src.GetVkImage();
	blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	blitInfo.dstImage = dst.GetVkImage();
	blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	blitInfo.regionCount = 1;
	blitInfo.pRegions = &blitRegion;
	blitInfo.filter = VK_FILTER_LINEAR;

	vkCmdBlitImage2(m_pImpl->cmd, &blitInfo);
}

// CommandBuffer
CommandBuffer::CommandBuffer(CommandPool& pool) : m_pImpl(std::make_unique<Impl>()), m_owner(&pool)
{
	VkDevice device = pool.GetDevice();

	VkCommandBufferAllocateInfo allocateInfos{};
	allocateInfos.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfos.commandPool = pool.GetPool();
	allocateInfos.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfos.commandBufferCount = 1;

	VK_CHECK(vkAllocateCommandBuffers(device, &allocateInfos, &m_pImpl->cmd));

	VkFenceCreateInfo fenceInfos{};
	fenceInfos.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfos.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VK_CHECK(vkCreateFence(device, &fenceInfos, nullptr, &m_pImpl->fence));

	VkSemaphoreCreateInfo semaphoreInfos{};
	semaphoreInfos.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VK_CHECK(vkCreateSemaphore(device, &semaphoreInfos, nullptr, &m_pImpl->semaphore));
}

CommandBuffer::~CommandBuffer()
{
	VkDevice device = m_owner->GetDevice();

	vkFreeCommandBuffers(device, m_owner->GetPool(), 1, &m_pImpl->cmd);
	vkDestroyFence(device, m_pImpl->fence, nullptr);
	vkDestroySemaphore(device, m_pImpl->semaphore, nullptr);
}

std::unique_ptr<CommandEncoder> CommandBuffer::BeginRecording()
{
	vkResetCommandBuffer(m_pImpl->cmd, 0);
	return std::unique_ptr<CommandEncoder>(new CommandEncoder(*this));
}

void CommandBuffer::WaitForFence() const
{
	vkWaitForFences(m_owner->GetDevice(), 1, &m_pImpl->fence, VK_TRUE, UINT64_MAX);
}

void CommandBuffer::ResetFence()
{
	vkResetFences(m_owner->GetDevice(), 1, &m_pImpl->fence);
}

bool CommandBuffer::IsFenceSignaled() const
{
	return vkGetFenceStatus(m_owner->GetDevice(), m_pImpl->fence) == VK_SUCCESS;
}

VkCommandBuffer CommandBuffer::GetCmd() const
{
	return m_pImpl->cmd;
}

VkFence CommandBuffer::GetFence() const
{
	return m_pImpl->fence;
}

VkSemaphore CommandBuffer::GetSemaphore() const
{
	return m_pImpl->semaphore;
}

