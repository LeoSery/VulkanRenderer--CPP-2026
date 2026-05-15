#include "GPU/sampler.h"
#include "Core/graphics_context.h"
#include "Utils/VkCheck.h"

#include <vulkan/vulkan.h>

// Helpers
static VkFilter ConvertToVkFilter(Sampler::E_Filter filter)
{
	switch (filter)
	{
	case Sampler::E_Filter::Nearest:
		return VK_FILTER_NEAREST;
	case Sampler::E_Filter::Linear:
		return VK_FILTER_LINEAR;
	default:
		throw std::runtime_error("[Sampler] > ConvertToVkFilter() : Invalid Filter passed by parameters");
	}
}

static VkSamplerAddressMode ConvertToVkWrapMode(Sampler::E_WrapMode wrapMode)
{
	switch (wrapMode)
	{
	case Sampler::E_WrapMode::Reapeat:
		return VK_SAMPLER_ADDRESS_MODE_REPEAT;
	case Sampler::E_WrapMode::Clamp:
		return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	case Sampler::E_WrapMode::Mirror:
		return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
	default:
		throw std::runtime_error("[Sampler] > ConvertToVkWrapMode() : Invalid wrapMode passed by parameters");
	}
}

// Constructor && destructors
Sampler::Sampler(GraphicsContext& ctx, const CreateInfos& infos) : m_ctx(&ctx)
{
	// Check if anistropy is supported by the current GPU
	VkPhysicalDeviceProperties physDevProperties{};
	vkGetPhysicalDeviceProperties(ctx.GetPhysicalDevice(), &physDevProperties);

	VkSamplerCreateInfo samplerInfos{};
	samplerInfos.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfos.minFilter = ConvertToVkFilter(infos.minimumFilter);
	samplerInfos.magFilter = ConvertToVkFilter(infos.magnificationFilter);
	samplerInfos.addressModeU = ConvertToVkWrapMode(infos.wrapMode);
	samplerInfos.addressModeV = ConvertToVkWrapMode(infos.wrapMode);
	samplerInfos.addressModeW = ConvertToVkWrapMode(infos.wrapMode);
	samplerInfos.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfos.minLod = 0.0f;
	samplerInfos.maxLod = infos.maxLOD;
	samplerInfos.mipLodBias = 0.0f;

	if (infos.anistropy && physDevProperties.limits.maxSamplerAnisotropy > 1.0f)
	{
		samplerInfos.anisotropyEnable = VK_TRUE;
		samplerInfos.maxAnisotropy = physDevProperties.limits.maxSamplerAnisotropy;
	}
	else
	{
		samplerInfos.anisotropyEnable = VK_FALSE;
		samplerInfos.maxAnisotropy = 1.0f;
	}

	VK_CHECK(vkCreateSampler(ctx.GetDevice(), &samplerInfos, nullptr, &m_sampler));
}

Sampler::~Sampler() noexcept
{
	vkDestroySampler(m_ctx->GetDevice(), m_sampler, nullptr);
}

// Getters
VkSampler Sampler::GetVkSampler() const
{
	return m_sampler;
}
