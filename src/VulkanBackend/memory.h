#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace MemoryManager
{
    VkCommandPool GetCommandPool();

    bool Init(VkCommandPool commandPool);

    bool UploadBuffer(VkBuffer dst, const void* data, VkDeviceSize size);
    bool UploadImage(VkImage dst, const void* data, uint32_t width, uint32_t height);

    void TransitionImageLayout(VkCommandBuffer cmd, VkImage image,
                               VkImageLayout oldLayout, VkImageLayout newLayout,
                               uint32_t baseMip = 0, uint32_t mipCount = 1);

    VkCommandBuffer BeginOneTimeCommands();
    void            EndOneTimeCommands(VkCommandBuffer cmd);

    // Descriptor management
    bool CreateDescriptors(VkBuffer* uboBuffers, VkDeviceSize uboSize, uint32_t frameCount);
    void DestroyDescriptors();

    VkDescriptorSet AllocateTextureDescriptorSet(VkImageView imageView, VkSampler sampler);

    VkDescriptorSetLayout GetCameraDescriptorSetLayout();
    VkDescriptorSetLayout GetTextureDescriptorSetLayout();
    VkDescriptorSet       GetCameraDescriptorSet(uint32_t frameIndex);
}
