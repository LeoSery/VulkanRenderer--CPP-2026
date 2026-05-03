#pragma once

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

class GraphicsContext;

class Image
{
public:
	enum class E_Usage : uint32_t
	{
		Sampled = 1 << 0, // The image will be loaded into a shader (e.g., texture, normal map, etc.)
		ColorAttachment = 1 << 1, // The image will serve as a render target for Vulkan to write to
		DepthAttachement = 1 << 2, // The image will serve as a render target for Vulkan to write to
		TransferSrc = 1 << 3, // Flag to indicate that the image will be the source of a copy
		TransferDst = 1 << 4, // Flag to indicate that the image will be the destination of a copy
		Storage = 1 << 5, // The image can be read from and written to in a compute shader
	};

	struct CreateInfos
	{
		uint32_t width = 0;
		uint32_t height = 0;
		VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
		E_Usage usage = E_Usage::Sampled;
		bool genMips = true; // Do we need to compute mipmap for this texture ?
	};

	explicit Image(GraphicsContext& ctx, const CreateInfos infos); // Create image for a texture > VMA create allow memory for and we contro all setings
	explicit Image(GraphicsContext& ctx, VkImage image, VkImageView imageView, VkFormat format, uint32_t width, uint32_t height); // Create image for an existing vulkan image, we wrap vulkan type into our Image class

	~Image() noexcept;

	Image(const Image&) = delete; //copy constructor
	Image& operator=(const Image&) = delete; // copy assignation
	Image(Image&&) = delete; // move constructor
	Image& operator=(Image&&) = delete; // move assignation

	void Upload(const void* pixels, uint32_t width, uint32_t height);

	uint32_t GetWidth() const;
	uint32_t GetHeight() const;
	uint32_t GetMipLevels() const;
	uint32_t GetFormat() const;

	VkImage GetVkImage() const;
	VkImageView GetVkImageView() const;
	VmaAllocation GetVmaAllocation() const;

	struct Impl;
	Impl& GetImpl();

private:
	GraphicsContext* m_ctx = nullptr;
	bool m_ownsImage = true; // Do we own image and we need to destroy it a the end ?
	uint32_t m_width = 0;
	uint32_t m_height = 0;
	uint32_t m_mipLevels = 1;
	VkFormat m_format = VK_FORMAT_UNDEFINED;
	struct Impl* m_pImpl = nullptr;
};

inline Image::E_Usage operator|(Image::E_Usage a, Image::E_Usage b)
{
	return static_cast<Image::E_Usage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline Image::E_Usage operator&(Image::E_Usage a, Image::E_Usage b)
{
	return static_cast<Image::E_Usage>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}
