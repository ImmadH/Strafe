#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <cstddef>

struct Vertex
{
    float pos[3];
    float color[3];

    static VkVertexInputBindingDescription GetBindingDescription();
    static std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions();
};

struct Mesh
{
    VkBuffer      vertexBuffer = VK_NULL_HANDLE;
    VmaAllocation allocation   = VK_NULL_HANDLE;
    uint32_t      vertexCount  = 0;

    bool Upload(const std::vector<Vertex>& vertices);
    void Destroy();
};
