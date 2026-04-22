#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <string>
#include <cstddef>

namespace Mesh
{
    struct Vertex
    {
        float pos[3];
        float normal[3];
        float uv[2];
        float tangent[4];
    };

    struct MeshData
    {
        uint32_t        indexOffset               = 0;
        uint32_t        indexCount                = 0;
        std::string     texturePath;
        std::string     metallicRoughnessPath;
        std::string     normalPath;
        std::string     aoPath;
        float           metallicFactor            = 1.0f;
        float           roughnessFactor           = 1.0f;
        VkDescriptorSet textureDescriptorSet      = VK_NULL_HANDLE;
    };

    struct AssetData
    {
        VkBuffer      vertexBuffer  = VK_NULL_HANDLE;
        VmaAllocation vertexAlloc   = VK_NULL_HANDLE;
        uint32_t      vertexCount   = 0;

        VkBuffer      indexBuffer   = VK_NULL_HANDLE;
        VmaAllocation indexAlloc    = VK_NULL_HANDLE;
        uint32_t      indexCount    = 0;

        std::vector<MeshData> meshes;
    };

    VkVertexInputBindingDescription                GetBindingDescription();
    std::vector<VkVertexInputAttributeDescription> GetAttributeDescriptions();


	bool LoadFromFile(AssetData& asset, const char* filePath);
    bool Upload(AssetData& asset, const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices);
    void Destroy(AssetData& asset);
}
