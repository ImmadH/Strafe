#pragma once
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

//GLFW Init + Vulkan Instance creation 
namespace App::Instance 
{
  bool Init();
  bool createInstance();
  GLFWwindow* GetWindowPointer();
  void Destroy();
}

