#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>

namespace VulkanPipeline
{
    bool Create();
    void Destroy();
    void RecreateFramebuffers();

    VkRenderPass GetRenderPass();
    VkPipeline GetGraphicsPipeline();
    std::vector<VkFramebuffer> GetFramebuffers();
}