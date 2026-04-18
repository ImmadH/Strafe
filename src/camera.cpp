#include "camera.h"
#include "device.h"
#include "sync.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace Camera
{
    struct CameraUBO { glm::mat4 view; glm::mat4 proj; };

    static constexpr uint32_t FRAME_COUNT = VulkanSynchronization::MAX_FRAMES_IN_FLIGHT;

    static glm::vec3 position  = { 0.0f, 0.0f, 3.0f };
    static float     yaw       = -90.0f;
    static float     pitch     = 0.0f;
    static float     moveSpeed = 3.0f;
    static float     sensitivity = 0.1f;
    static float     fov       = 45.0f;
    static float     nearPlane = 0.1f;
    static float     farPlane  = 1000.0f;

    static std::array<VkBuffer,        FRAME_COUNT> uboBuffers;
    static std::array<VmaAllocation,   FRAME_COUNT> uboAllocations;
    static std::array<void*,           FRAME_COUNT> uboMapped;
    static VkDescriptorSetLayout                    descriptorSetLayout = VK_NULL_HANDLE;
    static VkDescriptorPool                         descriptorPool      = VK_NULL_HANDLE;
    static std::array<VkDescriptorSet, FRAME_COUNT> descriptorSets;

    static glm::vec3 GetForward()
    {
        float y = glm::radians(yaw);
        float p = glm::radians(pitch);
        return glm::normalize(glm::vec3{
            std::cos(y) * std::cos(p),
            std::sin(p),
            std::sin(y) * std::cos(p)
        });
    }

    static glm::vec3 GetRight()
    {
        return glm::normalize(glm::cross(GetForward(), glm::vec3{ 0.0f, 1.0f, 0.0f }));
    }

    void ProcessKeyboard(GLFWwindow* window, float dt)
    {
        float     speed   = moveSpeed * dt;
        glm::vec3 forward = GetForward();
        glm::vec3 right   = GetRight();
        glm::vec3 up      = { 0.0f, 1.0f, 0.0f };

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) position += forward * speed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) position -= forward * speed;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) position -= right   * speed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) position += right   * speed;
        if (glfwGetKey(window, GLFW_KEY_SPACE)        == GLFW_PRESS) position += up * speed;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) position -= up * speed;
    }

    void ProcessMouse(float dx, float dy)
    {
        yaw   += dx * sensitivity;
        pitch += dy * sensitivity;
        pitch  = std::clamp(pitch, -89.0f, 89.0f);
    }

    void UpdateUBO(uint32_t frameIndex, float aspect)
    {
        CameraUBO ubo{};
        ubo.view         = glm::lookAt(position, position + GetForward(), glm::vec3{ 0.0f, 1.0f, 0.0f });
        ubo.proj         = glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
        ubo.proj[1][1]  *= -1.0f; // Vulkan NDC has Y pointing down
        memcpy(uboMapped[frameIndex], &ubo, sizeof(ubo));
    }

    VkDescriptorSetLayout GetDescriptorSetLayout() { return descriptorSetLayout; }
    VkDescriptorSet       GetDescriptorSet(uint32_t frameIndex) { return descriptorSets[frameIndex]; }

    bool Create()
    {
        VkDevice     device    = VulkanDevice::GetDevice();
        VmaAllocator allocator = VulkanDevice::GetAllocator();

        for (uint32_t i = 0; i < FRAME_COUNT; i++)
        {
            VkBufferCreateInfo bufInfo{};
            bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufInfo.size  = sizeof(CameraUBO);
            bufInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

            VmaAllocationCreateInfo allocCreateInfo{};
            allocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
            allocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

            VmaAllocationInfo allocInfo;
            if (vmaCreateBuffer(allocator, &bufInfo, &allocCreateInfo,
                    &uboBuffers[i], &uboAllocations[i], &allocInfo) != VK_SUCCESS)
            {
                return false;
            }
            uboMapped[i] = allocInfo.pMappedData;
        }

        // Descriptor set layout
        VkDescriptorSetLayoutBinding uboBinding{};
        uboBinding.binding         = 0;
        uboBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboBinding.descriptorCount = 1;
        uboBinding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings    = &uboBinding;

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
            return false;

        VkDescriptorPoolSize poolSize{};
        poolSize.type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = FRAME_COUNT;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes    = &poolSize;
        poolInfo.maxSets       = FRAME_COUNT;

        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
            return false;

        std::array<VkDescriptorSetLayout, FRAME_COUNT> layouts;
        layouts.fill(descriptorSetLayout);

        VkDescriptorSetAllocateInfo dsAllocInfo{};
        dsAllocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        dsAllocInfo.descriptorPool     = descriptorPool;
        dsAllocInfo.descriptorSetCount = FRAME_COUNT;
        dsAllocInfo.pSetLayouts        = layouts.data();

        if (vkAllocateDescriptorSets(device, &dsAllocInfo, descriptorSets.data()) != VK_SUCCESS)
            return false;

        // Point each descriptor set at its own UBO buffer
        for (uint32_t i = 0; i < FRAME_COUNT; i++)
        {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = uboBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range  = sizeof(CameraUBO);

            VkWriteDescriptorSet write{};
            write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet          = descriptorSets[i];
            write.dstBinding      = 0;
            write.dstArrayElement = 0;
            write.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo     = &bufferInfo;

            vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
        }

        return true;
    }

    void Destroy()
    {
        VkDevice     device    = VulkanDevice::GetDevice();
        VmaAllocator allocator = VulkanDevice::GetAllocator();

        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

        for (uint32_t i = 0; i < FRAME_COUNT; i++)
            vmaDestroyBuffer(allocator, uboBuffers[i], uboAllocations[i]);
    }
}
