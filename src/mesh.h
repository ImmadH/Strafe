#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <cstddef>

namespace Mesh
{
    struct Vertex
    {
        float pos[3];
        float normal[3];
        float uv[2];
    };

    struct MeshData
    {
        VkBuffer      vertexBuffer  = VK_NULL_HANDLE;
        VmaAllocation vertexAlloc   = VK_NULL_HANDLE;
        uint32_t      vertexCount   = 0;

        VkBuffer      indexBuffer   = VK_NULL_HANDLE;
        VmaAllocation indexAlloc    = VK_NULL_HANDLE;
        uint32_t      indexCount    = 0;
    };

    VkVertexInputBindingDescription                GetBindingDescription();
    std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions();


	bool LoadFromFile(MeshData& mesh, const char* filePath);
    bool Upload(MeshData& mesh, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    void Destroy(MeshData& mesh);
}
