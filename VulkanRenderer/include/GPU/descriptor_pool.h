#pragma once

#include <cstdint>

#include <vulkan/vulkan.h>

class GraphicsContext;

class DescriptorPool
{
public:
	struct CreateInfos
	{
		uint32_t maxSets = 10;
		uint32_t maxCombinedImageSamplers = 10;
		uint32_t maxUniformBuffers = 10;
	};

	explicit DescriptorPool(GraphicsContext& ctx, const CreateInfos& infos);
	~DescriptorPool() noexcept;

	DescriptorPool(const DescriptorPool&) = delete;
	DescriptorPool&  operator=(const DescriptorPool&) = delete;
	DescriptorPool(DescriptorPool&&) = delete;
	DescriptorPool& operator=(DescriptorPool &&) = delete;

	VkDescriptorPool GetVkDescriptorPool() const;

private:
	GraphicsContext* m_ctx = nullptr;
	VkDescriptorPool m_pool = VK_NULL_HANDLE;
};
