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
  glm::vec3 GetLightDir();
  bool      IsLightEnabled();
  VkDescriptorSet GetIBLDescriptorSet();
  VkBuffer        GetSkyboxVB();
  bool            IsMaterialOverride();
  float           GetDebugMetallic();
  float           GetDebugRoughness();
  bool            UseNormalMap();
  bool            UseAO();
  bool            UseIBL();
}
