#pragma once
#include "mesh.h"

namespace App
{
  bool Init();
  void mainLoop();
  void cleanup();

  bool CreateSurface(void* surface);
  Mesh& GetTriangleMesh();
}
