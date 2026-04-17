#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace VulkanSwapchain
{
    bool Create();
    void Destroy();

	VkFormat GetSwapChainImageFormat();
}
