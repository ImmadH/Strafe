#include "device.h"
#include <optional>
#include <set>
#include <iostream>
#include <vector>
#include "app.h"
#include "instance.h"
#include <vk_mem_alloc.h>

//surface creation
//physical device
//logical device
//and queues

namespace VulkanDevice
{
    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool IsComplete() const
        {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    uint32_t graphicsQueueFamilyIndex = 0;
    uint32_t presentQueueFamilyIndex = 0;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;
    
    std::vector<const char*> deviceExtensions = 
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    bool CheckDeviceExtensionSupport(VkPhysicalDevice device)
    {
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

        for (const VkExtensionProperties& extension : availableExtensions)
        {
            requiredExtensions.erase(extension.extensionName);
        }

        return requiredExtensions.empty();
    }

    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices{};
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const VkQueueFamilyProperties& queueFamily : queueFamilies)
        {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                indices.graphicsFamily = i;
            }

            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

            if (presentSupport)
            {
                indices.presentFamily = i;
            }

            if (indices.IsComplete())
            {
                break;
            }

            i++;
        }

        return indices;
    }

    bool IsDeviceSuitable(VkPhysicalDevice device)
    {
        const QueueFamilyIndices indices = FindQueueFamilies(device);
        if (!indices.IsComplete())
        {
            return false;
        }

        if (!CheckDeviceExtensionSupport(device))
        {
            return false;
        }

        graphicsQueueFamilyIndex = indices.graphicsFamily.value();
        presentQueueFamilyIndex = indices.presentFamily.value();
        return true;
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
        const std::set<uint32_t> uniqueQueueFamilies = {
            graphicsQueueFamilyIndex,
            presentQueueFamilyIndex
        };

        float queuePriority = 1.0f;
        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

        for (uint32_t queueFamily : uniqueQueueFamilies)
        {
            VkDeviceQueueCreateInfo queueInfo{};
            queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueInfo.queueFamilyIndex = queueFamily;
            queueInfo.queueCount = 1;
            queueInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueInfo);
        }

        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;

        VkDeviceCreateInfo deviceInfo{};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        deviceInfo.pQueueCreateInfos = queueCreateInfos.data();
        deviceInfo.pEnabledFeatures = &deviceFeatures;
        deviceInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        deviceInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if (vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device))
        {
            std::cerr << "Failed to create logical device\n";
            return false;
        }

        vkGetDeviceQueue(device, graphicsQueueFamilyIndex, 0, &graphicsQueue);

        return true;
    }

    bool CreateAllocator()
    {
        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.instance       = App::Instance::GetVulkanInstance();
        allocatorInfo.physicalDevice = physicalDevice;
        allocatorInfo.device         = device;
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_0;

        if (vmaCreateAllocator(&allocatorInfo, &allocator) != VK_SUCCESS)
        {
            std::cerr << "Failed to create VMA allocator\n";
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

        if (!CreateAllocator())
        {
            std::cout << "Failed to create VMA allocator\n";
            return false;
        }

        return true;
    }

    void Destroy()
    {
        VkInstance instance = App::Instance::GetVulkanInstance();
        if (allocator) {
            vmaDestroyAllocator(allocator);
            allocator = VK_NULL_HANDLE;
        }
        if (device) {
            vkDestroyDevice(device, nullptr);
            device = VK_NULL_HANDLE;
        }
        if (surface) {
            vkDestroySurfaceKHR(instance, surface, nullptr);
            surface = VK_NULL_HANDLE;
        }
    }

    VkDevice GetDevice()
    {
        return device;
    }

    VkPhysicalDevice GetPhysicalDevice()
    {
        return physicalDevice;
    }

    VkSurfaceKHR GetSurface()
    {
        return surface;
    }

    uint32_t GetGraphicsQueueFamilyIndex()
    {
        return graphicsQueueFamilyIndex;
    }

    uint32_t GetPresentQueueFamilyIndex()
    {
        return presentQueueFamilyIndex;
    }

    VkQueue GetGraphicsQueue()
    {
        return graphicsQueue;
    }

    VmaAllocator GetAllocator()
    {
        return allocator;
    }

    float GetMaxAnisotropy()
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physicalDevice, &props);
        return props.limits.maxSamplerAnisotropy;
    }
}
