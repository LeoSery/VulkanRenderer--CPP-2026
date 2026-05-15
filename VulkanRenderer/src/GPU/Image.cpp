#include "GPU/image.h"
#include "Core/graphics_context.h"
#include "GPU/buffer.h"
#include "GPU/command_pool.h"
#include "GPU/command_buffer.h"
#include "Utils/VkCheck.h"
#include "GPU/persistent_staging_buffer.h"

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>
#include <cmath>
#include <stdexcept>
#include <algorithm>

// Implementation
struct Image::Impl
{
	VkImage image = VK_NULL_HANDLE;
	VkImageView imageView = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
};

// Helepers
static uint32_t ComputeMipLevels(uint32_t width, uint32_t height)
{
	// Mipmaps formula : Log2(LargestImageSide); -> + 1 is for take account of the first element (orignal size)
	return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
}

static VkImageUsageFlags ConvertToVkUsage(Image::E_Usage usage)
{
	// Set the necessary flags using the custom operators in our ‘E_Usage’ enumeration.
	VkImageUsageFlags flags = 0;
	if ((usage & Image::E_Usage::Sampled) == Image::E_Usage::Sampled) { flags |= VK_IMAGE_USAGE_SAMPLED_BIT; }
	if ((usage & Image::E_Usage::ColorAttachment) == Image::E_Usage::ColorAttachment) { flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; }
	if ((usage & Image::E_Usage::DepthAttachement) == Image::E_Usage::DepthAttachement) { flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT; }
	if ((usage & Image::E_Usage::TransferSrc) == Image::E_Usage::TransferSrc) { flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT; }
	if ((usage & Image::E_Usage::TransferDst) == Image::E_Usage::TransferDst) { flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT; }
	if ((usage & Image::E_Usage::Storage) == Image::E_Usage::Storage) { flags |= VK_IMAGE_USAGE_STORAGE_BIT; }
	return flags;
}

static void TransitionLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t baseMipLevel, uint32_t mipLevels)
{
	VkImageMemoryBarrier2 barrierInfos{};
	barrierInfos.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	barrierInfos.oldLayout = oldLayout;
	barrierInfos.newLayout = newLayout;
	barrierInfos.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrierInfos.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrierInfos.image = image;
	barrierInfos.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrierInfos.subresourceRange.baseMipLevel = baseMipLevel;
	barrierInfos.subresourceRange.levelCount = mipLevels;
	barrierInfos.subresourceRange.baseArrayLayer = 0;
	barrierInfos.subresourceRange.layerCount = 1;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrierInfos.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
		barrierInfos.srcAccessMask = 0;
		barrierInfos.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		barrierInfos.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrierInfos.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		barrierInfos.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		barrierInfos.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		barrierInfos.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrierInfos.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		barrierInfos.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
		barrierInfos.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		barrierInfos.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
	}
	else
	{
		throw std::runtime_error("Image > TransitionLayout : Transition not supported");
	}

	VkDependencyInfo dependencyInfo{};
	dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependencyInfo.imageMemoryBarrierCount = 1;
	dependencyInfo.pImageMemoryBarriers = &barrierInfos;

	vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}

// Constructors & Destructors
Image::Image(GraphicsContext& ctx, const CreateInfos infos) : m_pImpl(std::make_unique<Impl>()), m_ctx(&ctx), m_width(infos.width), m_height(infos.height), m_format(infos.format), m_ownsImage(true)
{
	m_mipLevels = infos.genMips ? ComputeMipLevels(infos.width, infos.height) : 1;

	E_Usage finalUsage = infos.usage;
	if (infos.genMips)
	{
		finalUsage = finalUsage | E_Usage::TransferSrc | E_Usage::TransferDst;
	}

	VkImageCreateInfo imageInfos{};
	imageInfos.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfos.imageType = VK_IMAGE_TYPE_2D;
	imageInfos.format = infos.format;
	imageInfos.extent = { infos.width, infos.height, 1};
	imageInfos.mipLevels = m_mipLevels;
	imageInfos.arrayLayers = 1;
	imageInfos.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfos.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfos.usage = ConvertToVkUsage(finalUsage);
	imageInfos.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo allocationInfos{};
	allocationInfos.usage = VMA_MEMORY_USAGE_AUTO;

	VK_CHECK(vmaCreateImage(ctx.GetAllocator(), &imageInfos, &allocationInfos, &m_pImpl->image, &m_pImpl->allocation, nullptr));

	VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	if (infos.format == VK_FORMAT_D32_SFLOAT || infos.format == VK_FORMAT_D24_UNORM_S8_UINT || infos.format == VK_FORMAT_D16_UNORM)
	{
		aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	VkImageViewCreateInfo imageViewInfos{};
	imageViewInfos.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewInfos.image = m_pImpl->image;
	imageViewInfos.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewInfos.format = infos.format;
	imageViewInfos.subresourceRange.aspectMask = aspectMask;
	imageViewInfos.subresourceRange.baseMipLevel = 0;
	imageViewInfos.subresourceRange.levelCount = m_mipLevels;
	imageViewInfos.subresourceRange.baseArrayLayer = 0;
	imageViewInfos.subresourceRange.layerCount = 1;

	VK_CHECK(vkCreateImageView(ctx.GetDevice(), &imageViewInfos, nullptr, &m_pImpl->imageView));
}

Image::Image(GraphicsContext & ctx, VkImage image, VkImageView imageView, VkFormat format, uint32_t width, uint32_t height) : m_pImpl(new Impl), m_ctx(&ctx), m_width(width), m_height(height), m_format(format), m_mipLevels(1), m_ownsImage(false)
{
	m_pImpl->image = image;
	m_pImpl->imageView = imageView;
}

Image::~Image() noexcept
{
	if (m_ownsImage && m_pImpl->allocation != VK_NULL_HANDLE)
	{
		vmaDestroyImage(m_ctx->GetAllocator(), m_pImpl->image, m_pImpl->allocation);
	}

	if (m_pImpl->imageView != VK_NULL_HANDLE)
	{
		vkDestroyImageView(m_ctx->GetDevice(), m_pImpl->imageView, nullptr);
	}
}

void Image::Upload(const void* pixels, uint32_t width, uint32_t height) 
{
	size_t dataSize = width * height * 4; // 4 bytes per pixels (RGBA)

	// 1. Stagin buffer
	StagingBufferHandle stagingBufferHandle = m_ctx->GetPersistentStagingBuffer().Acquire(dataSize);
	std::memcpy(stagingBufferHandle.GetMappedData(), pixels, dataSize);

	// 2. CommandBuffer
	CommandBuffer* commandBuffer = &m_ctx->GetCommandPool().Acquire();

	VkCommandBufferBeginInfo beginInfos{};
	beginInfos.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfos.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(commandBuffer->GetCmd(), &beginInfos);

	// 3. Transition Vulkant to submit image (UNDEFINED > TRANSFERT_DST) for all mipmaps
	TransitionLayout(commandBuffer->GetCmd(), m_pImpl->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, m_mipLevels);

	// 4. Copy staging buffer
	VkBufferImageCopy regionInfos{};
	regionInfos.bufferOffset = 0;
	regionInfos.bufferRowLength = 0;
	regionInfos.bufferImageHeight = 0;
	regionInfos.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	regionInfos.imageSubresource.baseArrayLayer = 0;
	regionInfos.imageSubresource.layerCount = 1;
	regionInfos.imageOffset = { 0, 0, 0 };
	regionInfos.imageExtent = { width, height, 1 };

	vkCmdCopyBufferToImage(commandBuffer->GetCmd(), stagingBufferHandle.GetVkBuffer(), m_pImpl->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &regionInfos);

	// 5. Blit chain > mipmaps generation
	int32_t mipWidth = static_cast<int32_t>(width);
	int32_t mipHeight = static_cast<int32_t>(height);

	for (uint32_t i = 1; i < m_mipLevels; i++)
	{
		VkImageMemoryBarrier2 blitBarrierInfos{};
		blitBarrierInfos.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		blitBarrierInfos.image = m_pImpl->image;
		blitBarrierInfos.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		blitBarrierInfos.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		blitBarrierInfos.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		blitBarrierInfos.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		blitBarrierInfos.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		blitBarrierInfos.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
		blitBarrierInfos.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		blitBarrierInfos.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		blitBarrierInfos.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blitBarrierInfos.subresourceRange.baseMipLevel = i - 1;
		blitBarrierInfos.subresourceRange.levelCount = 1;
		blitBarrierInfos.subresourceRange.baseArrayLayer = 0;
		blitBarrierInfos.subresourceRange.layerCount = 1;

		VkDependencyInfo blitDependencyInfo{};
		blitDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		blitDependencyInfo.imageMemoryBarrierCount = 1;
		blitDependencyInfo.pImageMemoryBarriers = &blitBarrierInfos;

		vkCmdPipelineBarrier2(commandBuffer->GetCmd(), &blitDependencyInfo);

		VkImageBlit BlitInfos{};
		BlitInfos.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		BlitInfos.srcSubresource.mipLevel = i - 1;
		BlitInfos.srcSubresource.baseArrayLayer = 0;
		BlitInfos.srcSubresource.layerCount = 1;
		BlitInfos.srcOffsets[0] = { 0, 0, 0 };
		BlitInfos.srcOffsets[1] = { mipWidth, mipHeight, 1 };
		BlitInfos.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		BlitInfos.dstSubresource.mipLevel = i;
		BlitInfos.dstSubresource.baseArrayLayer = 0;
		BlitInfos.dstSubresource.layerCount = 1;
		BlitInfos.dstOffsets[0] = { 0, 0, 0 };
		BlitInfos.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };

		vkCmdBlitImage(commandBuffer->GetCmd(), m_pImpl->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_pImpl->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &BlitInfos, VK_FILTER_LINEAR);

		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;
	}

	if (m_mipLevels > 1)
	{
		TransitionLayout(commandBuffer->GetCmd(), m_pImpl->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, m_mipLevels - 1);
	}
	TransitionLayout(commandBuffer->GetCmd(), m_pImpl->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, m_mipLevels - 1, 1);

	vkEndCommandBuffer(commandBuffer->GetCmd());

	VkSubmitInfo submitInfos{};
	submitInfos.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfos.commandBufferCount = 1;
	VkCommandBuffer vkCommandBuffer = commandBuffer->GetCmd();
	submitInfos.pCommandBuffers = &vkCommandBuffer;

	vkQueueSubmit(m_ctx->GetGraphicsQueue(), 1, &submitInfos, VK_NULL_HANDLE);
	vkQueueWaitIdle(m_ctx->GetGraphicsQueue());

	m_ctx->GetCommandPool().Release(*commandBuffer);
}

uint32_t Image::GetWidth() const
{
	return m_width;
}

uint32_t Image::GetHeight() const
{
	return m_height;
}

uint32_t Image::GetMipLevels() const
{
	return m_mipLevels;
}

uint32_t Image::GetFormat() const
{
	return m_format;
}

VkImage Image::GetVkImage() const
{
	return m_pImpl->image;
}

VkImageView Image::GetVkImageView() const
{
	return m_pImpl->imageView;
}

VmaAllocation Image::GetVmaAllocation() const
{
	return m_pImpl->allocation;
}

Image::Impl& Image::GetImpl()
{
	return *m_pImpl;
}
