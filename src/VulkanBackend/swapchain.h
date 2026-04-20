#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>

namespace VulkanSwapchain
{
    bool Create();
    void Destroy();

	VkFormat GetSwapChainImageFormat();
	VkExtent2D GetSwapChainExtent();
	std::vector<VkImageView> GetSwapChainImageViews();
	VkSwapchainKHR GetSwapChain();
	uint32_t GetSwapChainImageCount();
}
