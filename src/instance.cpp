#include "instance.h"
#include <iostream>

namespace App::Instance
{
  //GLFW RELATED
  GLFWwindow* window = nullptr;
  const uint32_t WIDTH = 800;
  const uint32_t HEIGHT = 600;


  //VULKAN TYPES 
  VkInstance instance = VK_NULL_HANDLE;

  bool Init()
  {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(WIDTH, HEIGHT, "Strafe", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return false;
    }

    if (!createInstance())
      return false;

    return true;
  }

  bool createInstance()
  {
    //Create Vulkan Instance
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "StrafeInst";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    //tells the Vulkan driver which global extensions and validation layers we want to use.
    //TO DO - Check if extensions are supported by vulkan implementation
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;

    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;
    createInfo.enabledLayerCount = 0;

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
    {
      std::cout << "Failed to create VK Instance\n";
      return false;
    }

    std::cout << "Vulkan Instance Created\n";

    return true;
  }


  GLFWwindow* GetWindowPointer()
  {
    return window;
  }

  void Destroy()
  {
    glfwDestroyWindow(window);
    glfwTerminate();
  }
}
