#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace VulkanCommands
{
    bool Create();
    void Destroy();

    void RecordCommandBuffer(uint32_t imageIndex, uint32_t frameIndex);
    VkCommandBuffer GetCommandBuffer(uint32_t frameIndex);
    VkCommandPool   GetCommandPool();
}
