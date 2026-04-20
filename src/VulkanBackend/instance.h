#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

//GLFW Init + Vulkan Instance creation 
namespace App::Instance 
{
  bool Init();
  void Destroy();
  bool createInstance();
  GLFWwindow* GetWindowPointer();
  VkInstance GetVulkanInstance();
  

  //Validation utility 
  bool checkValidationSupport();
  void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

  void setupDebugMessenger();
  VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
    const VkAllocationCallbacks* allocator,
    VkDebugUtilsMessengerEXT* debugMessenger);

  void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* allocator);

  static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData);

;
}

