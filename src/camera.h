#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

struct GLFWwindow;

namespace Camera
{
    bool Create();
    void Destroy();

    void ProcessKeyboard(GLFWwindow* window, float dt);
    void ProcessMouse(float dx, float dy);
    void UpdateUBO(uint32_t frameIndex, float aspect);

    VkDescriptorSetLayout GetDescriptorSetLayout();
    VkDescriptorSet       GetDescriptorSet(uint32_t frameIndex);
}
