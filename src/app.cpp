#include "app.h"
#include "instance.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>

#include "device.h"
#include "swapchain.h"
#include "pipeline.h"

//all glfw and application related 
namespace App
{
	
  bool Init()
  {
    if(!App::Instance::Init())
    {
      std::cout << "Failed to init window\n";
      return false;
    }


    //BEFORE WRITING VulkanBackend:: gonna test

    if (!VulkanDevice::Create())
    {
      std::cout << "Failed to create vulkan device\n";
      return false;
    }

    if (!VulkanSwapchain::Create())
    {
      std::cout << "Failed to create vulkan swapchain\n";
      return false;
    }

    if (!VulkanPipeline::Create())
    {
      std::cout << "Failed to create graphics pipeline\n";
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

  bool CreateSurface(void* surface) 
  {
      VkInstance instance = App::Instance::GetVulkanInstance();
      if (glfwCreateWindowSurface(instance, App::Instance::GetWindowPointer(), nullptr, static_cast<VkSurfaceKHR*>(surface)) != VK_SUCCESS) 
      {
          return false;
      }
    return true;
  }


  void cleanup()
  {
	VulkanPipeline::Destroy();
    VulkanSwapchain::Destroy();
    VulkanDevice::Destroy();
    App::Instance::Destroy();
  }

}
