#pragma once

namespace VulkanDevice
{
    void Create();
    void Destroy();
}


//App:Init 
//Init -> calls create instance
//App::InitVulkanBackend call everything except instance creation and validation
//this backend will have all other objects + utility? maybe vma?