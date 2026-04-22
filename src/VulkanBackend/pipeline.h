#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>

namespace VulkanPipeline
{
    bool Create();
    void Destroy();
    void RecreateFramebuffers();
    bool GetMSAAEnabled();
    void SetMSAAEnabled(bool on);

    VkRenderPass     GetRenderPass();
    VkPipeline       GetGraphicsPipeline();
    VkPipeline       GetSkyboxPipeline();
    VkPipelineLayout GetPipelineLayout();
    std::vector<VkFramebuffer> GetFramebuffers();
}