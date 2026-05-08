#include "GPU/descriptor_set.h"
#include "GPU/descriptor_pool.h"
#include "GPU/image.h"
#include "GPU/sampler.h"
#include "GPU/buffer.h"
#include "Core/graphics_context.h"
#include "Utils/VkCheck.h"

// Constructors && Destructors
DescriptorSet::DescriptorSet(GraphicsContext& ctx, CreateInfos& infos) : m_ctx(&ctx)
{
	// 1. Create layout and descriptor set structure
	VkDescriptorSetLayoutBinding samplerBindingInfos{};
	samplerBindingInfos.binding = 0;
	samplerBindingInfos.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerBindingInfos.descriptorCount = 1;
	samplerBindingInfos.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo samplerLayoutInfos{};
	samplerLayoutInfos.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	samplerLayoutInfos.bindingCount = 1;
	samplerLayoutInfos.pBindings = &samplerBindingInfos;

	VK_CHECK(vkCreateDescriptorSetLayout(ctx.GetDevice(), &samplerLayoutInfos, nullptr, &m_descriptorSetLayout));

	// 2. Allocate the descriptor set from the pool
	VkDescriptorSetAllocateInfo descriptorSetAllocationInfos{};
	descriptorSetAllocationInfos.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocationInfos.descriptorPool = infos.pool->GetVkDescriptorPool();
	descriptorSetAllocationInfos.descriptorSetCount = 1;
	descriptorSetAllocationInfos.pSetLayouts = &m_descriptorSetLayout;

	VK_CHECK(vkAllocateDescriptorSets(ctx.GetDevice(), &descriptorSetAllocationInfos, &m_descriptorSet));
}

DescriptorSet::~DescriptorSet() noexcept
{
	vkDestroyDescriptorSetLayout(m_ctx->GetDevice(), m_descriptorSetLayout, nullptr);
}

// Getters
VkDescriptorSet DescriptorSet::GetVkDescriptorSet() const
{
	return m_descriptorSet;
}

VkDescriptorSetLayout DescriptorSet::GetVkDescriptorSetLayout() const
{
	return m_descriptorSetLayout;
}

// Template specialisations
template<>
void DescriptorSet::Bind<Image>(uint32_t binding, Image& image, Sampler* sampler)
{
	if (!sampler)
	{
		throw std::runtime_error("[DescriptorSet] > Bind<Image>(): A sampler is required, but it is null or empty");
	}

	VkDescriptorImageInfo descriptorImageInfos{};
	descriptorImageInfos.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	descriptorImageInfos.imageView = image.GetVkImageView();
	descriptorImageInfos.sampler = sampler->GetVkSampler();

	VkWriteDescriptorSet descriptorSetImageWriteInfos{};
	descriptorSetImageWriteInfos.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorSetImageWriteInfos.dstSet = m_descriptorSet;
	descriptorSetImageWriteInfos.dstBinding = binding;
	descriptorSetImageWriteInfos.dstArrayElement = 0;
	descriptorSetImageWriteInfos.descriptorCount = 1;
	descriptorSetImageWriteInfos.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorSetImageWriteInfos.pImageInfo = &descriptorImageInfos;

	vkUpdateDescriptorSets(m_ctx->GetDevice(), 1, &descriptorSetImageWriteInfos, 0, nullptr);
}

template<>
void DescriptorSet::Bind<Buffer>(uint32_t binding, Buffer& buffer, Sampler* sampler)
{
	VkDescriptorBufferInfo descriptorBufferInfos{};
	descriptorBufferInfos.buffer = buffer.GetVkBuffer();
	descriptorBufferInfos.offset = 0;
	descriptorBufferInfos.range = buffer.GetSize();

	VkWriteDescriptorSet descriptorSetBufferWriteInfos{};
	descriptorSetBufferWriteInfos.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorSetBufferWriteInfos.dstSet = m_descriptorSet;
	descriptorSetBufferWriteInfos.dstBinding = binding;
	descriptorSetBufferWriteInfos.dstArrayElement = 0;
	descriptorSetBufferWriteInfos.descriptorCount = 1;
	descriptorSetBufferWriteInfos.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	descriptorSetBufferWriteInfos.pBufferInfo = &descriptorBufferInfos;

	vkUpdateDescriptorSets(m_ctx->GetDevice(), 1, &descriptorSetBufferWriteInfos, 0, nullptr);
}
