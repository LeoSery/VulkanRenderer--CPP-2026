#pragma once

#include <vulkan/vulkan.h>
#include <vma/vk_mem_alloc.h>

struct Image
{
	VkImage image = VK_NULL_HANDLE;
	VkImageView imageView = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
	VkExtent2D extent = {};
	VkFormat format = VK_FORMAT_UNDEFINED;
};
