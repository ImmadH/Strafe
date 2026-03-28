
#include "glfw.h"


namespace App::GLFW 
{
  GLFWwindow* window = nullptr;

  bool Init()
  {
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
    if (!glfwInit())
        return false;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    window = glfwCreateWindow(800, 600, "Strafe", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return false;
    }

    return true;
  }

  GLFWwindow* GetWindowPointer()
  {
    return window;
  }
}
