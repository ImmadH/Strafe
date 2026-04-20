#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace MemoryManager
{
    VkCommandPool GetCommandPool();

    bool Init(VkCommandPool commandPool);

    // Allocates a GPU-only buffer and uploads data via a staging buffer
    bool UploadBuffer(VkBuffer dst, const void* data, VkDeviceSize size);
}
