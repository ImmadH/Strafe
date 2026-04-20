#include "memory.h"
#include "device.h"
#include <cstring>
#include <stdexcept>

namespace MemoryManager
{
    static VkCommandPool s_commandPool = VK_NULL_HANDLE;

    VkCommandPool GetCommandPool() { return s_commandPool; }

    bool Init(VkCommandPool commandPool)
    {
        s_commandPool = commandPool;
        return true;
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

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = s_commandPool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmd;
        if (vkAllocateCommandBuffers(VulkanDevice::GetDevice(), &allocInfo, &cmd) != VK_SUCCESS)
        {
            vmaDestroyBuffer(VulkanDevice::GetAllocator(), stagingBuffer, stagingAllocation);
            return false;
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &beginInfo);

        VkBufferCopy region{};
        region.size = size;
        vkCmdCopyBuffer(cmd, stagingBuffer, dst, 1, &region);

        vkEndCommandBuffer(cmd);

        VkSubmitInfo submitInfo{};
        submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers    = &cmd;

        vkQueueSubmit(VulkanDevice::GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(VulkanDevice::GetGraphicsQueue());

        vkFreeCommandBuffers(VulkanDevice::GetDevice(), s_commandPool, 1, &cmd);
        vmaDestroyBuffer(VulkanDevice::GetAllocator(), stagingBuffer, stagingAllocation);

        return true;
    }
}
