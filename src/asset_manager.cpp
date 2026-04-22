#include "asset_manager.h"
#include "VulkanBackend/memory.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <iostream>

namespace AssetManager
{
    struct MeshEntry
    {
        Mesh::AssetData            data;
        std::vector<TextureHandle> textures; // released when mesh is released
        uint32_t                   refCount = 0;
        uint32_t                   id       = 0;
    };

    struct TextureEntry
    {
        Texture::TextureData data;
        uint32_t             refCount = 0;
        uint32_t             id       = 0;
    };

    static std::unordered_map<std::string, MeshEntry>    s_meshes;
    static std::unordered_map<std::string, TextureEntry> s_textures;
    static std::unordered_map<uint32_t, std::string>     s_meshIdToPath;
    static std::unordered_map<uint32_t, std::string>     s_texIdToPath;
    static uint32_t s_nextMeshId = 1;
    static uint32_t s_nextTexId  = 1;

    void Init() {}

    void Shutdown()
    {
        for (auto& [path, e] : s_meshes)    Mesh::Destroy(e.data);
        for (auto& [path, e] : s_textures)  Texture::Destroy(e.data);
        s_meshes.clear();
        s_textures.clear();
        s_meshIdToPath.clear();
        s_texIdToPath.clear();
        s_nextMeshId = 1;
        s_nextTexId  = 1;
    }

    TextureHandle LoadTexture(const char* path, bool sRGB)
    {
        std::string key(path);
        auto it = s_textures.find(key);
        if (it != s_textures.end())
        {
            it->second.refCount++;
            return { it->second.id };
        }

        TextureEntry entry;
        if (!Texture::LoadFromFile(entry.data, path, sRGB))
        {
            std::cout << "AssetManager: failed to load texture: " << path << "\n";
            return {};
        }
        entry.refCount = 1;
        entry.id       = s_nextTexId++;
        s_texIdToPath[entry.id] = key;
        s_textures[key]         = std::move(entry);
        return { s_textures[key].id };
    }

    MeshHandle LoadMesh(const char* path)
    {
        std::string key(path);
        auto it = s_meshes.find(key);
        if (it != s_meshes.end())
        {
            it->second.refCount++;
            return { it->second.id };
        }

        MeshEntry entry;
        if (!Mesh::LoadFromFile(entry.data, path))
        {
            std::cout << "AssetManager: failed to load mesh: " << path << "\n";
            return {};
        }

        std::string dir(path);
        dir = dir.substr(0, dir.find_last_of("/\\") + 1);

        for (Mesh::MeshData& sub : entry.data.meshes)
        {
            VkImageView albedoView = MemoryManager::GetFallbackImageView();
            VkSampler   albedoSamp = MemoryManager::GetFallbackSampler();
            VkImageView mrView     = MemoryManager::GetFallbackImageView();
            VkSampler   mrSamp     = MemoryManager::GetFallbackSampler();
            VkImageView normalView = MemoryManager::GetNormalFallbackImageView();
            VkSampler   normalSamp = MemoryManager::GetNormalFallbackSampler();
            VkImageView aoView     = MemoryManager::GetFallbackImageView();
            VkSampler   aoSamp     = MemoryManager::GetFallbackSampler();

            auto tryLoad = [&](const std::string& rel, bool sRGB,
                               VkImageView& outView, VkSampler& outSamp)
            {
                if (rel.empty()) return;
                TextureHandle h = LoadTexture((dir + rel).c_str(), sRGB);
                if (!h.valid()) return;
                entry.textures.push_back(h);
                auto* t    = GetTexture(h);
                outView    = t->imageView;
                outSamp    = t->sampler;
            };

            tryLoad(sub.texturePath,           true,  albedoView, albedoSamp);
            tryLoad(sub.metallicRoughnessPath, false, mrView,     mrSamp);
            tryLoad(sub.normalPath,            false, normalView, normalSamp);
            tryLoad(sub.aoPath,                false, aoView,     aoSamp);

            sub.textureDescriptorSet = MemoryManager::AllocateTextureDescriptorSet(
                albedoView, albedoSamp,
                mrView,     mrSamp,
                normalView, normalSamp,
                aoView,     aoSamp);
        }

        entry.refCount = 1;
        entry.id       = s_nextMeshId++;
        s_meshIdToPath[entry.id] = key;
        s_meshes[key]            = std::move(entry);
        return { s_meshes[key].id };
    }

    Mesh::AssetData* GetMesh(MeshHandle h)
    {
        if (!h.valid()) return nullptr;
        auto it = s_meshIdToPath.find(h.id);
        if (it == s_meshIdToPath.end()) return nullptr;
        return &s_meshes[it->second].data;
    }

    Texture::TextureData* GetTexture(TextureHandle h)
    {
        if (!h.valid()) return nullptr;
        auto it = s_texIdToPath.find(h.id);
        if (it == s_texIdToPath.end()) return nullptr;
        return &s_textures[it->second].data;
    }

    void Release(MeshHandle h)
    {
        if (!h.valid()) return;
        auto it = s_meshIdToPath.find(h.id);
        if (it == s_meshIdToPath.end()) return;
        auto& entry = s_meshes[it->second];
        if (--entry.refCount == 0)
        {
            for (TextureHandle th : entry.textures)
                Release(th);
            Mesh::Destroy(entry.data);
            s_meshes.erase(it->second);
            s_meshIdToPath.erase(it);
        }
    }

    void Release(TextureHandle h)
    {
        if (!h.valid()) return;
        auto it = s_texIdToPath.find(h.id);
        if (it == s_texIdToPath.end()) return;
        auto& entry = s_textures[it->second];
        if (--entry.refCount == 0)
        {
            Texture::Destroy(entry.data);
            s_textures.erase(it->second);
            s_texIdToPath.erase(it);
        }
    }
}
