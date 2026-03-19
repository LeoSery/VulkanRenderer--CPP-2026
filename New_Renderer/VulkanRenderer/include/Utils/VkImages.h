#pragma once

#include <vulkan/vulkan.h>

void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);