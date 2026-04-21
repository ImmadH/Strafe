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
    static VkDescriptorPool           s_descriptorPool       = VK_NULL_HANDLE;
    static std::vector<VkDescriptorSet> s_cameraDescriptorSets;

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
                               uint32_t baseMip, uint32_t mipCount)
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout           = oldLayout;
        barrier.newLayout           = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image               = image;
        barrier.subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, baseMip, mipCount, 0, 1 };

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

    bool UploadImage(VkImage dst, const void* data, uint32_t width, uint32_t height)
    {
        VkDeviceSize size = width * height * 4;

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
        uboBinding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo cameraLayoutInfo{};
        cameraLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        cameraLayoutInfo.bindingCount = 1;
        cameraLayoutInfo.pBindings    = &uboBinding;

        if (vkCreateDescriptorSetLayout(device, &cameraLayoutInfo, nullptr, &s_cameraLayout) != VK_SUCCESS)
            return false;

        // set 1: per-material texture
        VkDescriptorSetLayoutBinding samplerBinding{};
        samplerBinding.binding         = 0;
        samplerBinding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerBinding.descriptorCount = 1;
        samplerBinding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo textureLayoutInfo{};
        textureLayoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        textureLayoutInfo.bindingCount = 1;
        textureLayoutInfo.pBindings    = &samplerBinding;

        if (vkCreateDescriptorSetLayout(device, &textureLayoutInfo, nullptr, &s_textureLayout) != VK_SUCCESS)
            return false;

        VkDescriptorPoolSize poolSizes[2];
        poolSizes[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = frameCount;
        poolSizes[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = 32;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes    = poolSizes;
        poolInfo.maxSets       = frameCount + 32;

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &s_descriptorPool) != VK_SUCCESS)
            return false;

		//for camera
        std::vector<VkDescriptorSetLayout> cameraLayouts(frameCount, s_cameraLayout);

        VkDescriptorSetAllocateInfo dsAllocInfo{};
        dsAllocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsAllocInfo.descriptorPool     = s_descriptorPool;
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
        vkDestroyDescriptorPool(device, s_descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, s_textureLayout, nullptr);
        vkDestroyDescriptorSetLayout(device, s_cameraLayout, nullptr);
        s_descriptorPool  = VK_NULL_HANDLE;
        s_cameraLayout    = VK_NULL_HANDLE;
        s_textureLayout   = VK_NULL_HANDLE;
        s_cameraDescriptorSets.clear();
    }

    VkDescriptorSet AllocateTextureDescriptorSet(VkImageView imageView, VkSampler sampler)
    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = s_descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &s_textureLayout;

        VkDescriptorSet ds;
        if (vkAllocateDescriptorSets(VulkanDevice::GetDevice(), &allocInfo, &ds) != VK_SUCCESS)
            return VK_NULL_HANDLE;

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView   = imageView;
        imageInfo.sampler     = sampler;

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = ds;
        write.dstBinding      = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo      = &imageInfo;

        vkUpdateDescriptorSets(VulkanDevice::GetDevice(), 1, &write, 0, nullptr);
        return ds;
    }

    VkDescriptorSetLayout GetCameraDescriptorSetLayout()  { return s_cameraLayout; }
    VkDescriptorSetLayout GetTextureDescriptorSetLayout() { return s_textureLayout; }
    VkDescriptorSet       GetCameraDescriptorSet(uint32_t frameIndex) { return s_cameraDescriptorSets[frameIndex]; }
}
