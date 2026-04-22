#pragma once
#include "asset_manager.h"
#include "mesh.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <cstdint>

namespace Scene
{
    struct Transform
    {
        glm::vec3 position = glm::vec3(0.0f);
        glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 scale    = glm::vec3(1.0f);

        glm::mat4 LocalMatrix() const;
    };

    struct Node
    {
        std::string              name;
        Transform                localTransform;
        int32_t                  parent = -1;
        std::vector<int32_t>     children;
        AssetManager::MeshHandle mesh;
        bool                     active = true;
    };

    struct DrawCall
    {
        glm::mat4        worldTransform;
        Mesh::AssetData* asset;
        uint32_t         indexOffset;
        uint32_t         indexCount;
        VkDescriptorSet  materialSet;
        float            metallicFactor;
        float            roughnessFactor;
    };

    void    Init();
    void    Shutdown();

    int32_t AddNode   (const char* name, int32_t parent = -1);
    Node&   GetNode   (int32_t index);
    void    RemoveNode(int32_t index);
    int32_t NodeCount ();

    glm::mat4 WorldTransform (int32_t index);
    void      CollectDrawCalls(std::vector<DrawCall>& out);
}
