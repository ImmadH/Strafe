#pragma once
#include "mesh.h"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

namespace App
{
  bool Init();
  void mainLoop();
  void cleanup();

  bool CreateSurface(void* surface);
  Mesh::AssetData& GetMesh();
  glm::vec3 GetLightDir();
  VkDescriptorSet GetIBLDescriptorSet();
  VkBuffer        GetSkyboxVB();
  bool            IsMaterialOverride();
  float           GetDebugMetallic();
  float           GetDebugRoughness();
}
