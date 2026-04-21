#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace Texture
{
    struct TextureData
    {
        VkImage       image      = VK_NULL_HANDLE;
        VkImageView   imageView  = VK_NULL_HANDLE;
        VkSampler     sampler    = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
    };

    bool LoadFromFile(TextureData& texture, const char* filePath, bool sRGB = true);
    void Destroy(TextureData& texture);
}
