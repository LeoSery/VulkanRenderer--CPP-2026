#pragma once

#include <vulkan/vulkan.h>
#include <stdexcept>

class GraphicsContext;
class DescriptorPool;
class Image;
class Sampler;
class Buffer;

class DescriptorSet
{
public:
	struct CreateInfos
	{
		DescriptorPool* pool = nullptr;
	};

	explicit DescriptorSet(GraphicsContext& ctx, CreateInfos& infos);
	~DescriptorSet() noexcept;

	DescriptorSet(DescriptorSet&) = delete;
	DescriptorSet& operator=(DescriptorSet&) = delete;
	DescriptorSet(DescriptorSet&&) = delete;
	DescriptorSet& operator=(DescriptorSet&&) = delete;

	// bind Texture + Sampler at specified binding
	template<typename T>
	void Bind(uint32_t binding, T& resource, Sampler* sampler = nullptr);

	VkDescriptorSet GetVkDescriptorSet() const;
	VkDescriptorSetLayout GetVkDescriptorSetLayout() const;

private:
	GraphicsContext* m_ctx = nullptr;
	VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
};

// Template specialisations
template<>
void DescriptorSet::Bind<Image>(uint32_t binding, Image& image, Sampler* sampler);

template<>
void DescriptorSet::Bind<Buffer>(uint32_t binding, Buffer& buffer, Sampler* sampler);

template<typename T>
inline void DescriptorSet::Bind(uint32_t binding, T& resource, Sampler* sampler)
{
	throw std::runtime_error("[DescriptorSet] > Bind<T>() : Unsupported type");
}
