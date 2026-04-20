#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace VulkanDevice
{
    bool Create();
    void Destroy();
    VkPhysicalDevice GetPhysicalDevice();
    VkDevice GetDevice();
    VkSurfaceKHR GetSurface();
    uint32_t GetGraphicsQueueFamilyIndex();
    uint32_t GetPresentQueueFamilyIndex();
    VkQueue GetGraphicsQueue();
    VmaAllocator GetAllocator();
}


//App:Init 
//Init -> calls create instance
//App::InitVulkanBackend call everything except instance creation and validation
//this backend will have all other objects + utility? maybe vma?
