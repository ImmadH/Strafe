#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace MemoryManager
{
    VkCommandPool GetCommandPool();

    bool Init(VkCommandPool commandPool);

    bool UploadBuffer(VkBuffer dst, const void* data, VkDeviceSize size);
    bool UploadImage(VkImage dst, const void* data, uint32_t width, uint32_t height, uint32_t bytesPerPixel = 4);

    void TransitionImageLayout(VkCommandBuffer cmd, VkImage image,
                               VkImageLayout oldLayout, VkImageLayout newLayout,
                               uint32_t baseMip = 0, uint32_t mipCount = 1,
                               uint32_t baseLayer = 0, uint32_t layerCount = 1);

    VkCommandBuffer BeginOneTimeCommands();
    void            EndOneTimeCommands(VkCommandBuffer cmd);

    // Descriptor management
    bool CreateDescriptors(VkBuffer* uboBuffers, VkDeviceSize uboSize, uint32_t frameCount);
    void DestroyDescriptors();

    VkDescriptorSet AllocateTextureDescriptorSet(VkImageView albedoView,  VkSampler albedoSampler,
                                                 VkImageView mrView,      VkSampler mrSampler,
                                                 VkImageView normalView,  VkSampler normalSampler,
                                                 VkImageView aoView,      VkSampler aoSampler);

    VkDescriptorSetLayout GetCameraDescriptorSetLayout();
    VkDescriptorSetLayout GetTextureDescriptorSetLayout();
    VkDescriptorSetLayout GetIBLDescriptorSetLayout();
    VkDescriptorSet       GetCameraDescriptorSet(uint32_t frameIndex);
    VkDescriptorSet       AllocateIBLDescriptorSet(VkImageView irradianceView,  VkSampler irradianceSampler,
                                                   VkImageView prefilteredView, VkSampler prefilteredSampler,
                                                   VkImageView brdfLUTView,     VkSampler brdfLUTSampler,
                                                   VkImageView environmentView, VkSampler environmentSampler);
    void                  UpdateIBLDescriptorSet  (VkDescriptorSet ds,
                                                   VkImageView irradianceView,  VkSampler irradianceSampler,
                                                   VkImageView prefilteredView, VkSampler prefilteredSampler,
                                                   VkImageView brdfLUTView,     VkSampler brdfLUTSampler,
                                                   VkImageView environmentView, VkSampler environmentSampler);
    VkImageView           GetFallbackImageView();
    VkSampler             GetFallbackSampler();
    VkImageView           GetNormalFallbackImageView();
    VkSampler             GetNormalFallbackSampler();
    VkDescriptorSet       GetFallbackTextureDescriptorSet();
}
