#include "app.h"
#include "instance.h"
#include <GLFW/glfw3.h>
#include <iostream>
namespace App
{

  bool Init()
  {
    if(!App::Instance::Init())
    {
      std::cout << "Failed to init window\n";
      return false;
    }
    return true;
  }

  void mainLoop()
  {
    while (!glfwWindowShouldClose(App::Instance::GetWindowPointer()))
    {
      glfwPollEvents();
    }
  }

  void cleanup()
  {
    App::Instance::Destroy();
  }
}
