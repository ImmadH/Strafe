#include "device.h"
#include <vector>
#include <iostream>
#include "device.h"
#include "app.h"
#include "instance.h"

//surface creation
//physical device
//logical device
//and queues

namespace VulkanDevice
{

    uint32_t graphicsQueueFamilyIndex = 0;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    
    std::vector<const char*> deviceExtensions = 
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };


    bool IsDeviceSuitable(VkPhysicalDevice device) {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilyCount; i++) {
            VkBool32 presentSupport = VK_FALSE;
            if (vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport) != VK_SUCCESS) {
                continue;
            }

            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupport) {
                graphicsQueueFamilyIndex = i;
                return true;
            }
        }
        return false;
    }

    bool PickPhysicalDevice() 
    {
        VkInstance instance = App::Instance::GetVulkanInstance();
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        if (deviceCount == 0) 
        {
            std::cout << "No Vulkan supported devices found\n";
            return false;
        }
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

        for (const auto& dev : devices) 
        {
            if (IsDeviceSuitable(dev)) 
            {
                physicalDevice = dev;
                return true;
            }
        }
        std::cout << "No suitable Vulkan device found\n";
        return false;
    }

    bool CreateLogicalDevice()
    {
        //float to assign priorities to queues to influence the scheduling of command buffer execution 
        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = graphicsQueueFamilyIndex;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;

        VkDeviceCreateInfo deviceInfo{};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device))
        {
            std::cerr << "Failed to create logical device\n";
            return false;
        }
        
        return true;
    }
    

    bool Create() // physical device, surface, logical device - instance was created by our app 
    {   
        if (!App::CreateSurface(&surface))
        {
            std::cout << "Failed to create Vulkan Surface\n";
            return false;
        }

        if (!PickPhysicalDevice())
        {
            std::cout << "Failed to find Vulkan device\n";
            return false;
        }

        if (!CreateLogicalDevice())
        {
            std::cout << "Failed to create Vulkan Logical device\n";
            return false;
        }

        return true;
    }

    void Destroy()
    {
        VkInstance instance = App::Instance::GetVulkanInstance();
        if (device) {
            vkDestroyDevice(device, nullptr);
            device = VK_NULL_HANDLE;
        }
        if (surface) {
            vkDestroySurfaceKHR(instance, surface, nullptr);
            surface = VK_NULL_HANDLE;
        }
    }
}
