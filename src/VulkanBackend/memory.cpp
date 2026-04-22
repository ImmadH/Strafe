#include "memory.h"
#include "device.h"
#include "sync.h"
#include <cstring>
#include <stdexcept>
#include <vector>

//Handles all GPU memory /operations buffer/image uploads via staging
//image layout transitions and descriptor set management
namespace MemoryManager
{
    static VkCommandPool              s_commandPool          = VK_NULL_HANDLE;
    static VkDescriptorSetLayout      s_cameraLayout         = VK_NULL_HANDLE;
    static VkDescriptorSetLayout      s_textureLayout        = VK_NULL_HANDLE;
    static VkDescriptorSetLayout      s_iblLayout            = VK_NULL_HANDLE;
    static VkDescriptorPool           s_uboPool              = VK_NULL_HANDLE;
    static std::vector<VkDescriptorPool> s_imagePools;
    static std::vector<VkDescriptorSet> s_cameraDescriptorSets;
    static VkDescriptorSet            s_fallbackTextureDs    = VK_NULL_HANDLE;

    static constexpr uint32_t IMAGE_POOL_SETS  = 64;
    static constexpr uint32_t IMAGE_POOL_DESCS = 256;

    static VkDescriptorSet AllocFromImagePools(VkDescriptorSetLayout layout)
    {
        VkDevice device = VulkanDevice::GetDevice();

        for (VkDescriptorPool pool : s_imagePools)
        {
            VkDescriptorSetAllocateInfo ai{};
            ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            ai.descriptorPool     = pool;
            ai.descriptorSetCount = 1;
            ai.pSetLayouts        = &layout;

            VkDescriptorSet ds;
            if (vkAllocateDescriptorSets(device, &ai, &ds) == VK_SUCCESS)
                return ds;
        }

        VkDescriptorPoolSize poolSize{};
        poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = IMAGE_POOL_DESCS;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes    = &poolSize;
        poolInfo.maxSets       = IMAGE_POOL_SETS;

        VkDescriptorPool newPool;
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &newPool) != VK_SUCCESS)
            return VK_NULL_HANDLE;

        s_imagePools.push_back(newPool);

        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = newPool;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &layout;

        VkDescriptorSet ds;
        if (vkAllocateDescriptorSets(device, &ai, &ds) != VK_SUCCESS)
            return VK_NULL_HANDLE;

        return ds;
    }

    static VkImage        s_fallbackImage     = VK_NULL_HANDLE;
    static VmaAllocation  s_fallbackAlloc     = VK_NULL_HANDLE;
    static VkImageView    s_fallbackImageView = VK_NULL_HANDLE;
    static VkSampler      s_fallbackSampler   = VK_NULL_HANDLE;

    static VkImage        s_normalFallbackImage     = VK_NULL_HANDLE;
    static VmaAllocation  s_normalFallbackAlloc     = VK_NULL_HANDLE;
    static VkImageView    s_normalFallbackImageView = VK_NULL_HANDLE;
    static VkSampler      s_normalFallbackSampler   = VK_NULL_HANDLE;

    VkCommandPool GetCommandPool() { return s_commandPool; }

    bool Init(VkCommandPool commandPool)
    {
        s_commandPool = commandPool;
        return true;
    }

    VkCommandBuffer BeginOneTimeCommands()
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = s_commandPool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(VulkanDevice::GetDevice(), &allocInfo, &cmd);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);

        return cmd;
    }

    void EndOneTimeCommands(VkCommandBuffer cmd)
    {
        vkEndCommandBuffer(cmd);

        VkSubmitInfo submitInfo{};
        submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &cmd;

        vkQueueSubmit(VulkanDevice::GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(VulkanDevice::GetGraphicsQueue());

        vkFreeCommandBuffers(VulkanDevice::GetDevice(), s_commandPool, 1, &cmd);
    }

    bool UploadBuffer(VkBuffer dst, const void* data, VkDeviceSize size)
    {
        VkBufferCreateInfo stagingInfo{};
        stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingInfo.size  = size;
        stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo stagingAllocInfo{};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

        VkBuffer      stagingBuffer;
        VmaAllocation stagingAllocation;

        if (vmaCreateBuffer(VulkanDevice::GetAllocator(), &stagingInfo, &stagingAllocInfo,
                            &stagingBuffer, &stagingAllocation, nullptr) != VK_SUCCESS)
            return false;

        void* mapped;
        vmaMapMemory(VulkanDevice::GetAllocator(), stagingAllocation, &mapped);
        memcpy(mapped, data, static_cast<size_t>(size));
        vmaUnmapMemory(VulkanDevice::GetAllocator(), stagingAllocation);

        VkCommandBuffer cmd = BeginOneTimeCommands();

        VkBufferCopy region{};
        region.size = size;
        vkCmdCopyBuffer(cmd, stagingBuffer, dst, 1, &region);

        EndOneTimeCommands(cmd);

        vmaDestroyBuffer(VulkanDevice::GetAllocator(), stagingBuffer, stagingAllocation);
        return true;
    }

    void TransitionImageLayout(VkCommandBuffer cmd, VkImage image,
                               VkImageLayout oldLayout, VkImageLayout newLayout,
                               uint32_t baseMip, uint32_t mipCount,
                               uint32_t baseLayer, uint32_t layerCount)
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = oldLayout;
        barrier.newLayout           = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = image;
        barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, baseMip, mipCount, baseLayer, layerCount };

        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;


		//need this for proper handling of mip generation
        switch (oldLayout)
        {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            barrier.srcAccessMask = 0;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;
        default:
            barrier.srcAccessMask = 0;
            break;
        }

        switch (newLayout)
        {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;
        default:
            barrier.dstAccessMask = 0;
            break;
        }

        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    bool UploadImage(VkImage dst, const void* data, uint32_t width, uint32_t height, uint32_t bytesPerPixel)
    {
        VkDeviceSize size = (VkDeviceSize)width * height * bytesPerPixel;

        VkBufferCreateInfo stagingInfo{};
        stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingInfo.size  = size;
        stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo stagingAllocInfo{};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;

        VkBuffer      stagingBuffer;
        VmaAllocation stagingAllocation;

        if (vmaCreateBuffer(VulkanDevice::GetAllocator(), &stagingInfo, &stagingAllocInfo,
                            &stagingBuffer, &stagingAllocation, nullptr) != VK_SUCCESS)
            return false;

        void* mapped;
        vmaMapMemory(VulkanDevice::GetAllocator(), stagingAllocation, &mapped);
        memcpy(mapped, data, static_cast<size_t>(size));
        vmaUnmapMemory(VulkanDevice::GetAllocator(), stagingAllocation);

        VkCommandBuffer cmd = BeginOneTimeCommands();

        TransitionImageLayout(cmd, dst,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy region{};
        region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        region.imageExtent      = { width, height, 1 };
        vkCmdCopyBufferToImage(cmd, stagingBuffer, dst,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        TransitionImageLayout(cmd, dst,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        EndOneTimeCommands(cmd);

        vmaDestroyBuffer(VulkanDevice::GetAllocator(), stagingBuffer, stagingAllocation);
        return true;
    }

    bool CreateDescriptors(VkBuffer* uboBuffers, VkDeviceSize uboSize, uint32_t frameCount)
    {
        VkDevice device = VulkanDevice::GetDevice();

        // set 0: camera UBO
        VkDescriptorSetLayoutBinding uboBinding{};
        uboBinding.binding         = 0;
        uboBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboBinding.descriptorCount = 1;
        uboBinding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo cameraLayoutInfo{};
        cameraLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        cameraLayoutInfo.bindingCount = 1;
        cameraLayoutInfo.pBindings    = &uboBinding;

        if (vkCreateDescriptorSetLayout(device, &cameraLayoutInfo, nullptr, &s_cameraLayout) != VK_SUCCESS)
            return false;

        // set 1: per-material textures (0=albedo, 1=metallic/roughness, 2=normal, 3=AO)
        VkDescriptorSetLayoutBinding textureBindings[4]{};
        for (int i = 0; i < 4; i++)
        {
            textureBindings[i].binding         = i;
            textureBindings[i].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            textureBindings[i].descriptorCount = 1;
            textureBindings[i].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
        }

        VkDescriptorSetLayoutCreateInfo textureLayoutInfo{};
        textureLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        textureLayoutInfo.bindingCount = 4;
        textureLayoutInfo.pBindings    = textureBindings;

        if (vkCreateDescriptorSetLayout(device, &textureLayoutInfo, nullptr, &s_textureLayout) != VK_SUCCESS)
            return false;

        // set 2: IBL (binding 0=irradiance, binding 1=prefiltered, binding 2=BRDF LUT, binding 3=environment)
        VkDescriptorSetLayoutBinding iblBindings[4]{};
        for (int i = 0; i < 4; i++)
        {
            iblBindings[i].binding         = i;
            iblBindings[i].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            iblBindings[i].descriptorCount = 1;
            iblBindings[i].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;
        }
        VkDescriptorSetLayoutCreateInfo iblLayoutInfo{};
        iblLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        iblLayoutInfo.bindingCount = 4;
        iblLayoutInfo.pBindings    = iblBindings;

        if (vkCreateDescriptorSetLayout(device, &iblLayoutInfo, nullptr, &s_iblLayout) != VK_SUCCESS)
            return false;

        VkDescriptorPoolSize uboPoolSize{};
        uboPoolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboPoolSize.descriptorCount = frameCount;

        VkDescriptorPoolCreateInfo uboPoolInfo{};
        uboPoolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        uboPoolInfo.poolSizeCount = 1;
        uboPoolInfo.pPoolSizes    = &uboPoolSize;
        uboPoolInfo.maxSets       = frameCount;

        if (vkCreateDescriptorPool(device, &uboPoolInfo, nullptr, &s_uboPool) != VK_SUCCESS)
            return false;

        // 1x1 UNORM fallback for metallic/roughness 
        {
            // R=255 G=255 B=255 so texture factors multiply through cleanly (factor * 1.0 = factor)
            uint8_t pixel[4] = { 255, 255, 255, 255 };

            VkImageCreateInfo imgInfo{};
            imgInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imgInfo.imageType     = VK_IMAGE_TYPE_2D;
            imgInfo.extent        = { 1, 1, 1 };
            imgInfo.mipLevels     = 1;
            imgInfo.arrayLayers   = 1;
            imgInfo.format        = VK_FORMAT_R8G8B8A8_UNORM;
            imgInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
            imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imgInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            imgInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
            imgInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

            vmaCreateImage(VulkanDevice::GetAllocator(), &imgInfo, &allocInfo,
                           &s_fallbackImage, &s_fallbackAlloc, nullptr);
            UploadImage(s_fallbackImage, pixel, 1, 1);

            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image                           = s_fallbackImage;
            viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format                          = VK_FORMAT_R8G8B8A8_UNORM;
            viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.levelCount     = 1;
            viewInfo.subresourceRange.layerCount     = 1;
            vkCreateImageView(device, &viewInfo, nullptr, &s_fallbackImageView);

            VkSamplerCreateInfo samplerInfo{};
            samplerInfo.sType     = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.maxLod    = 1.0f;
            vkCreateSampler(device, &samplerInfo, nullptr, &s_fallbackSampler);
        }

        // flat normal fallback: (128,128,255) decodes to tangent-space (0,0,1)
        {
            uint8_t pixel[4] = { 128, 128, 255, 255 };

            VkImageCreateInfo imgInfo{};
            imgInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imgInfo.imageType     = VK_IMAGE_TYPE_2D;
            imgInfo.extent        = { 1, 1, 1 };
            imgInfo.mipLevels     = 1;
            imgInfo.arrayLayers   = 1;
            imgInfo.format        = VK_FORMAT_R8G8B8A8_UNORM;
            imgInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
            imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imgInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            imgInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
            imgInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

            vmaCreateImage(VulkanDevice::GetAllocator(), &imgInfo, &allocInfo,
                           &s_normalFallbackImage, &s_normalFallbackAlloc, nullptr);
            UploadImage(s_normalFallbackImage, pixel, 1, 1);

            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image                           = s_normalFallbackImage;
            viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format                          = VK_FORMAT_R8G8B8A8_UNORM;
            viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.levelCount     = 1;
            viewInfo.subresourceRange.layerCount     = 1;
            vkCreateImageView(device, &viewInfo, nullptr, &s_normalFallbackImageView);

            VkSamplerCreateInfo samplerInfo{};
            samplerInfo.sType     = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.maxLod    = 1.0f;
            vkCreateSampler(device, &samplerInfo, nullptr, &s_normalFallbackSampler);
        }

        s_fallbackTextureDs = AllocateTextureDescriptorSet(
            s_fallbackImageView,       s_fallbackSampler,
            s_fallbackImageView,       s_fallbackSampler,
            s_normalFallbackImageView, s_normalFallbackSampler,
            s_fallbackImageView,       s_fallbackSampler);

        //for camera
        std::vector<VkDescriptorSetLayout> cameraLayouts(frameCount, s_cameraLayout);

        VkDescriptorSetAllocateInfo dsAllocInfo{};
        dsAllocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsAllocInfo.descriptorPool     = s_uboPool;
        dsAllocInfo.descriptorSetCount = frameCount;
        dsAllocInfo.pSetLayouts        = cameraLayouts.data();

        s_cameraDescriptorSets.resize(frameCount);
        if (vkAllocateDescriptorSets(device, &dsAllocInfo, s_cameraDescriptorSets.data()) != VK_SUCCESS)
            return false;

        for (uint32_t i = 0; i < frameCount; i++)
        {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uboBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range  = uboSize;

            VkWriteDescriptorSet write{};
            write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet          = s_cameraDescriptorSets[i];
            write.dstBinding      = 0;
            write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo     = &bufferInfo;

            vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
        }

        return true;
    }

    void DestroyDescriptors()
    {
        VkDevice device = VulkanDevice::GetDevice();
        vkDestroySampler(device, s_fallbackSampler, nullptr);
        vkDestroyImageView(device, s_fallbackImageView, nullptr);
        vmaDestroyImage(VulkanDevice::GetAllocator(), s_fallbackImage, s_fallbackAlloc);
        vkDestroySampler(device, s_normalFallbackSampler, nullptr);
        vkDestroyImageView(device, s_normalFallbackImageView, nullptr);
        vmaDestroyImage(VulkanDevice::GetAllocator(), s_normalFallbackImage, s_normalFallbackAlloc);
        for (VkDescriptorPool pool : s_imagePools)
            vkDestroyDescriptorPool(device, pool, nullptr);
        s_imagePools.clear();
        vkDestroyDescriptorPool(device, s_uboPool, nullptr);
        vkDestroyDescriptorSetLayout(device, s_iblLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, s_textureLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, s_cameraLayout, nullptr);
        s_uboPool         = VK_NULL_HANDLE;
        s_cameraLayout    = VK_NULL_HANDLE;
        s_textureLayout   = VK_NULL_HANDLE;
        s_fallbackTextureDs = VK_NULL_HANDLE;
        s_cameraDescriptorSets.clear();
    }

    VkDescriptorSet AllocateTextureDescriptorSet(VkImageView albedoView,  VkSampler albedoSampler,
                                                 VkImageView mrView,      VkSampler mrSampler,
                                                 VkImageView normalView,  VkSampler normalSampler,
                                                 VkImageView aoView,      VkSampler aoSampler)
    {
        VkDescriptorSet ds = AllocFromImagePools(s_textureLayout);
        if (ds == VK_NULL_HANDLE)
            return VK_NULL_HANDLE;

        VkDescriptorImageInfo imageInfos[4]{};
        imageInfos[0] = { albedoSampler, albedoView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        imageInfos[1] = { mrSampler,     mrView,     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        imageInfos[2] = { normalSampler, normalView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        imageInfos[3] = { aoSampler,     aoView,     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

        VkWriteDescriptorSet writes[4]{};
        for (int i = 0; i < 4; i++)
        {
            writes[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet          = ds;
            writes[i].dstBinding      = i;
            writes[i].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[i].descriptorCount = 1;
            writes[i].pImageInfo      = &imageInfos[i];
        }

        vkUpdateDescriptorSets(VulkanDevice::GetDevice(), 4, writes, 0, nullptr);
        return ds;
    }

    VkDescriptorSet AllocateIBLDescriptorSet(VkImageView irradianceView,  VkSampler irradianceSampler,
                                             VkImageView prefilteredView, VkSampler prefilteredSampler,
                                             VkImageView brdfLUTView,     VkSampler brdfLUTSampler,
                                             VkImageView environmentView, VkSampler environmentSampler)
    {
        VkDescriptorSet ds = AllocFromImagePools(s_iblLayout);
        if (ds == VK_NULL_HANDLE)
            return VK_NULL_HANDLE;

        VkDescriptorImageInfo imageInfos[4]{};
        imageInfos[0] = { irradianceSampler,  irradianceView,  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        imageInfos[1] = { prefilteredSampler, prefilteredView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        imageInfos[2] = { brdfLUTSampler,     brdfLUTView,     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        imageInfos[3] = { environmentSampler, environmentView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

        VkWriteDescriptorSet writes[4]{};
        for (int i = 0; i < 4; i++)
        {
            writes[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet          = ds;
            writes[i].dstBinding      = i;
            writes[i].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[i].descriptorCount = 1;
            writes[i].pImageInfo      = &imageInfos[i];
        }
        vkUpdateDescriptorSets(VulkanDevice::GetDevice(), 4, writes, 0, nullptr);
        return ds;
    }

    void UpdateIBLDescriptorSet(VkDescriptorSet ds,
                                VkImageView irradianceView,  VkSampler irradianceSampler,
                                VkImageView prefilteredView, VkSampler prefilteredSampler,
                                VkImageView brdfLUTView,     VkSampler brdfLUTSampler,
                                VkImageView environmentView, VkSampler environmentSampler)
    {
        VkDescriptorImageInfo imageInfos[4]{};
        imageInfos[0] = { irradianceSampler,  irradianceView,  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        imageInfos[1] = { prefilteredSampler, prefilteredView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        imageInfos[2] = { brdfLUTSampler,     brdfLUTView,     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };
        imageInfos[3] = { environmentSampler, environmentView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL };

        VkWriteDescriptorSet writes[4]{};
        for (int i = 0; i < 4; i++)
        {
            writes[i].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet          = ds;
            writes[i].dstBinding      = i;
            writes[i].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[i].descriptorCount = 1;
            writes[i].pImageInfo      = &imageInfos[i];
        }
        vkUpdateDescriptorSets(VulkanDevice::GetDevice(), 4, writes, 0, nullptr);
    }

    VkDescriptorSetLayout GetCameraDescriptorSetLayout()  { return s_cameraLayout; }
    VkDescriptorSetLayout GetTextureDescriptorSetLayout() { return s_textureLayout; }
    VkDescriptorSetLayout GetIBLDescriptorSetLayout()     { return s_iblLayout; }
    VkDescriptorSet       GetCameraDescriptorSet(uint32_t frameIndex) { return s_cameraDescriptorSets[frameIndex]; }
    VkImageView           GetFallbackImageView()       { return s_fallbackImageView; }
    VkSampler             GetFallbackSampler()         { return s_fallbackSampler; }
    VkImageView           GetNormalFallbackImageView() { return s_normalFallbackImageView; }
    VkSampler             GetNormalFallbackSampler()   { return s_normalFallbackSampler; }
    VkDescriptorSet       GetFallbackTextureDescriptorSet() { return s_fallbackTextureDs; }
}
