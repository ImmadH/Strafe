#pragma once
#include <vulkan/vulkan.h>

namespace VulkanDevice
{
    bool Create();
    void Destroy();
}


//App:Init 
//Init -> calls create instance
//App::InitVulkanBackend call everything except instance creation and validation
//this backend will have all other objects + utility? maybe vma?