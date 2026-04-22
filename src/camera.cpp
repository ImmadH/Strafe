#include "camera.h"
#include "VulkanBackend/device.h"
#include "VulkanBackend/sync.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <algorithm>
#include <cstring>

namespace Camera
{
    struct CameraUBO { glm::mat4 view; glm::mat4 proj; };

    static constexpr uint32_t FRAME_COUNT = VulkanSynchronization::MAX_FRAMES_IN_FLIGHT;

    static glm::vec3 position    = { 0.0f, 0.0f, 3.0f };
    static float     yaw         = -90.0f;
    static float     pitch       = 0.0f;
    static float     moveSpeed   = 3.0f;
    static float     sensitivity = 0.1f;
    static float     fov         = 70.0f;
    static float     nearPlane   = 0.1f;
    static float     farPlane    = 1000.0f;

    static std::array<VkBuffer,      FRAME_COUNT> uboBuffers;
    static std::array<VmaAllocation, FRAME_COUNT> uboAllocations;
    static std::array<void*,         FRAME_COUNT> uboMapped;

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
        ubo.view        = glm::lookAt(position, position + GetForward(), glm::vec3{ 0.0f, 1.0f, 0.0f });
        ubo.proj        = glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
        ubo.proj[1][1] *= -1.0f;
        memcpy(uboMapped[frameIndex], &ubo, sizeof(ubo));
    }

    VkBuffer     GetUBOBuffer(uint32_t frameIndex) { return uboBuffers[frameIndex]; }
    VkDeviceSize GetUBOSize()                      { return sizeof(CameraUBO); }
    float        GetFOV()                          { return fov; }
    void         SetFOV(float f)                   { fov = f; }

    bool Create()
    {
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
                return false;

            uboMapped[i] = allocInfo.pMappedData;
        }

        return true;
    }

    void Destroy()
    {
        VmaAllocator allocator = VulkanDevice::GetAllocator();
        for (uint32_t i = 0; i < FRAME_COUNT; i++)
            vmaDestroyBuffer(allocator, uboBuffers[i], uboAllocations[i]);
    }
}
