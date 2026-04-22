#include "scene.h"
#include "VulkanBackend/memory.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Scene
{
    static std::vector<Node> s_nodes;

    glm::mat4 Transform::LocalMatrix() const
    {
        glm::mat4 t = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 r = glm::mat4_cast(rotation);
        glm::mat4 s = glm::scale(glm::mat4(1.0f), scale);
        return t * r * s;
    }

    void    Init()      { s_nodes.clear(); }
    void    Shutdown()  { s_nodes.clear(); }
    int32_t NodeCount() { return static_cast<int32_t>(s_nodes.size()); }

    int32_t AddNode(const char* name, int32_t parent)
    {
        int32_t index = static_cast<int32_t>(s_nodes.size());
        s_nodes.emplace_back();
        s_nodes[index].name   = name;
        s_nodes[index].parent = parent;
        if (parent >= 0 && parent < index)
            s_nodes[parent].children.push_back(index);
        return index;
    }

    Node& GetNode(int32_t index)
    {
        return s_nodes[index];
    }

    void RemoveNode(int32_t index)
    {
        s_nodes[index].active = false;
    }

    glm::mat4 WorldTransform(int32_t index)
    {
        glm::mat4 world  = s_nodes[index].localTransform.LocalMatrix();
        int32_t   parent = s_nodes[index].parent;
        while (parent >= 0)
        {
            world  = s_nodes[parent].localTransform.LocalMatrix() * world;
            parent = s_nodes[parent].parent;
        }
        return world;
    }

    void CollectDrawCalls(std::vector<DrawCall>& out)
    {
        out.clear();
        for (int32_t i = 0; i < static_cast<int32_t>(s_nodes.size()); i++)
        {
            const Node& node = s_nodes[i];
            if (!node.active || !node.mesh.valid()) continue;

            Mesh::AssetData* asset = AssetManager::GetMesh(node.mesh);
            if (!asset) continue;

            glm::mat4 world = WorldTransform(i);

            for (const Mesh::MeshData& sub : asset->meshes)
            {
                DrawCall dc{};
                dc.worldTransform  = world;
                dc.asset           = asset;
                dc.indexOffset     = sub.indexOffset;
                dc.indexCount      = sub.indexCount;
                dc.materialSet     = sub.textureDescriptorSet != VK_NULL_HANDLE
                    ? sub.textureDescriptorSet
                    : MemoryManager::GetFallbackTextureDescriptorSet();
                dc.metallicFactor  = sub.metallicFactor;
                dc.roughnessFactor = sub.roughnessFactor;
                out.push_back(dc);
            }
        }
    }
}
