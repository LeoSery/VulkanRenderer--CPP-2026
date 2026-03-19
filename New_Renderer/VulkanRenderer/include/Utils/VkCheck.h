#pragma once

#include <vulkan/vulkan.h>
#include <stdexcept>
#include <string>

#define VK_CHECK(x) \
    do { \
        VkResult _result = (x); \
        if (_result != VK_SUCCESS) \
            throw std::runtime_error( \
                std::string("VkResult: ") + std::to_string(_result) + \
                " in " __FILE__ " line " + std::to_string(__LINE__)); \
    } while(0)