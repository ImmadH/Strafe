#include "app.h"
#include "glfw.h"
#include <GLFW/glfw3.h>
#include <iostream>
namespace App
{
  bool Init()
  {
    if(!App::GLFW::Init())
    {
      std::cout << "Failed to init window\n";
      return false;
    }
    return true;
  }

  void mainLoop()
  {
    while (!glfwWindowShouldClose(App::GLFW::GetWindowPointer()))
    {
      glfwPollEvents();
    }
  }
}
