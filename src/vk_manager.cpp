#include "vk_manager.h"
#include "VulkanBackend/instance.h"
#include "VulkanBackend/device.h"
#include "VulkanBackend/swapchain.h"
#include "VulkanBackend/pipeline.h"
#include "VulkanBackend/commands.h"
#include "VulkanBackend/sync.h"
#include "VulkanBackend/memory.h"
#include "camera.h"
#include <iostream>

namespace VulkanBackendManager
{
    bool Init()
    {
        if (!VulkanDevice::Create())
        {
            std::cout << "Failed to create vulkan device\n";
            return false;
        }

        if (!Camera::Create())
        {
            std::cout << "Failed to create camera\n";
            return false;
        }

        if (!VulkanSwapchain::Create())
        {
            std::cout << "Failed to create vulkan swapchain\n";
            return false;
        }

        if (!VulkanCommands::Create())
        {
            std::cout << "Failed to create command buffers\n";
            return false;
        }

        if (!MemoryManager::Init(VulkanCommands::GetCommandPool()))
        {
            std::cout << "Failed to init memory manager\n";
            return false;
        }

        VkBuffer uboBuffers[VulkanSynchronization::MAX_FRAMES_IN_FLIGHT];
        for (uint32_t i = 0; i < VulkanSynchronization::MAX_FRAMES_IN_FLIGHT; i++)
            uboBuffers[i] = Camera::GetUBOBuffer(i);

        if (!MemoryManager::CreateDescriptors(uboBuffers, Camera::GetUBOSize(),
                                              VulkanSynchronization::MAX_FRAMES_IN_FLIGHT))
        {
            std::cout << "Failed to create descriptors\n";
            return false;
        }

        if (!VulkanPipeline::Create())
        {
            std::cout << "Failed to create graphics pipeline\n";
            return false;
        }

        if (!VulkanSynchronization::Create())
        {
            std::cout << "Failed to create sync objects\n";
            return false;
        }

        return true;
    }

    void Shutdown()
    {
        VulkanSynchronization::Destroy();
        VulkanPipeline::Destroy();
        MemoryManager::DestroyDescriptors();
        VulkanCommands::Destroy();
        VulkanSwapchain::Destroy();
        Camera::Destroy();
        VulkanDevice::Destroy();
    }

    void RecreateSwapChain()
    {
        vkDeviceWaitIdle(VulkanDevice::GetDevice());

        VulkanPipeline::RecreateFramebuffers();
        VulkanSwapchain::Destroy();
        VulkanSwapchain::Create();
        VulkanPipeline::RecreateFramebuffers();
    }
}
