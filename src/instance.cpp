#include "instance.h"

#include <cstring>
#include <iostream>
#include <vector>

namespace App::Instance
{
  namespace
  {
    constexpr uint32_t kWidth = 800;
    constexpr uint32_t kHeight = 600;

    const std::vector<const char*> kValidationLayers = {
      "VK_LAYER_KHRONOS_validation"
    };

#ifdef NDEBUG
    constexpr bool kEnableValidationLayers = false;
#else
    constexpr bool kEnableValidationLayers = true;
#endif

    GLFWwindow* window = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
  }

  VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instanceHandle,
    const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
    const VkAllocationCallbacks* allocator,
    VkDebugUtilsMessengerEXT* debugMessengerHandle)
  {
    const auto createDebugUtilsMessenger =
      reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instanceHandle, "vkCreateDebugUtilsMessengerEXT"));

    if (createDebugUtilsMessenger == nullptr)
    {
      return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    return createDebugUtilsMessenger(
      instanceHandle,
      createInfo,
      allocator,
      debugMessengerHandle);
  }

  void DestroyDebugUtilsMessengerEXT(
    VkInstance instanceHandle,
    VkDebugUtilsMessengerEXT debugMessengerHandle,
    const VkAllocationCallbacks* allocator)
  {
    const auto destroyDebugUtilsMessenger =
      reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instanceHandle, "vkDestroyDebugUtilsMessengerEXT"));

    if (destroyDebugUtilsMessenger != nullptr)
    {
      destroyDebugUtilsMessenger(instanceHandle, debugMessengerHandle, allocator);
    }
  }

  bool Init()
  {
    if (glfwInit() == GLFW_FALSE)
    {
      std::cout << "Failed to initialize GLFW\n";
      return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    window = glfwCreateWindow(kWidth, kHeight, "Strafe", nullptr, nullptr);
    if (window == nullptr)
    {
      std::cout << "Failed to create GLFW window\n";
      glfwTerminate();
      return false;
    }

    if (!createInstance())
    {
      Destroy();
      return false;
    }

    setupDebugMessenger();
    return true;
  }

  bool createInstance()
  {
    if (kEnableValidationLayers && !checkValidationSupport())
    {
      std::cout << "Validation layers requested, but not available\n";
      return false;
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Strafe";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    if (glfwExtensions == nullptr)
    {
      std::cout << "Failed to query GLFW Vulkan extensions\n";
      return false;
    }

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    if (kEnableValidationLayers)
    {
      extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (kEnableValidationLayers)
    {
      createInfo.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
      createInfo.ppEnabledLayerNames = kValidationLayers.data();
      populateDebugMessengerCreateInfo(debugCreateInfo);
      createInfo.pNext = &debugCreateInfo;
    }
    else
    {
      createInfo.enabledLayerCount = 0;
      createInfo.pNext = nullptr;
    }

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
    {
      std::cout << "Failed to create Vulkan instance\n";
      return false;
    }

    return true;
  }

  bool checkValidationSupport()
  {
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : kValidationLayers)
    {
      bool layerFound = false;
      for (const auto& layerProperties : availableLayers)
      {
        if (std::strcmp(layerName, layerProperties.layerName) == 0)
        {
          layerFound = true;
          break;
        }
      }

      if (!layerFound)
      {
        return false;
      }
    }

    return true;
  }

  void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
  {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;
  }

  void setupDebugMessenger()
  {
    if (!kEnableValidationLayers)
    {
      return;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);

    if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
    {
      std::cout << "Failed to set up debug messenger\n";
    }
  }

  VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
  {
    (void)messageSeverity;
    (void)messageType;
    (void)pUserData;

    std::cerr << "validation layer: " << pCallbackData->pMessage << '\n';
    return VK_FALSE;
  }

  GLFWwindow* GetWindowPointer()
  {
    return window;
  }

  void Destroy()
  {
    if (debugMessenger != VK_NULL_HANDLE)
    {
      DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
      debugMessenger = VK_NULL_HANDLE;
    }

    if (instance != VK_NULL_HANDLE)
    {
      vkDestroyInstance(instance, nullptr);
      instance = VK_NULL_HANDLE;
    }

    if (window != nullptr)
    {
      glfwDestroyWindow(window);
      window = nullptr;
    }

    glfwTerminate();
  }
}
