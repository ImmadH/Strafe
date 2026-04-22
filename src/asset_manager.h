#pragma once
#include "mesh.h"
#include "texture.h"
#include <cstdint>

namespace AssetManager
{
    struct MeshHandle    { uint32_t id = 0; bool valid() const { return id != 0; } };
    struct TextureHandle { uint32_t id = 0; bool valid() const { return id != 0; } };

    void Init();
    void Shutdown();

    MeshHandle    LoadMesh   (const char* path);
    TextureHandle LoadTexture(const char* path, bool sRGB = true);

    Mesh::AssetData*      GetMesh   (MeshHandle h);
    Texture::TextureData* GetTexture(TextureHandle h);

    void Release(MeshHandle h);
    void Release(TextureHandle h);
}
