#include "GPU/descriptor_pool.h"
#include "Core/graphics_context.h"
#include "Utils/VkCheck.h"

#include <array>

// Constructors && Destuctors
DescriptorPool::DescriptorPool(GraphicsContext& ctx, const CreateInfos& infos) : m_ctx(&ctx)
{
	std::array<VkDescriptorPoolSize, 2> poolSizes{};

	poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSizes[0].descriptorCount = infos.maxCombinedImageSamplers;

	poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSizes[1].descriptorCount = infos.maxUniformBuffers;

	VkDescriptorPoolCreateInfo poolInfos{};
	poolInfos.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfos.maxSets = infos.maxSets;
	poolInfos.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfos.pPoolSizes = poolSizes.data();

	VK_CHECK(vkCreateDescriptorPool(ctx.GetDevice(), &poolInfos, nullptr, &m_pool));
}

DescriptorPool::~DescriptorPool() noexcept
{
	vkDestroyDescriptorPool(m_ctx->GetDevice(), m_pool, nullptr);
}

VkDescriptorPool DescriptorPool::GetVkDescriptorPool() const
{
	return m_pool;
}
