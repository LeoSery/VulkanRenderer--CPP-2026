#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <stdexcept>
#include <string>

#define VK_CHECK(x)                                                          \
    do {                                                                     \
        VkResult vkCheckResult = (x);                                        \
        if (vkCheckResult != VK_SUCCESS)                                     \
        {                                                                    \
            throw std::runtime_error(                                        \
                std::string("[Vulkan Error]\n")                              \
                + "  Result   : " + string_VkResult(vkCheckResult) + "\n"   \
                + "  File     : " + __FILE__ + "\n"                          \
                + "  Line     : " + std::to_string(__LINE__) + "\n"          \
                + "  Function : " + __func__                                 \
            );                                                               \
        }                                                                    \
    } while(0)