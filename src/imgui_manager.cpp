#include "imgui_manager.h"
#include "VulkanBackend/device.h"
#include "VulkanBackend/pipeline.h"
#include "VulkanBackend/swapchain.h"
#include "VulkanBackend/sync.h"
#include "VulkanBackend/instance.h"
#include "app.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include "theme.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace ImGuiManager
{
    static VkDescriptorPool s_pool = VK_NULL_HANDLE;

    bool Init()
    {
        VkDescriptorPoolSize poolSize{};
        poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 16;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets       = 16;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes    = &poolSize;

        if (vkCreateDescriptorPool(VulkanDevice::GetDevice(), &poolInfo, nullptr, &s_pool) != VK_SUCCESS)
            return false;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        applyOrangeTheme();

        ImGuiIO& io = ImGui::GetIO();
        io.FontGlobalScale = 2.0f;
        ImGui::GetStyle().ScaleAllSizes(2.0f);

        ImGui_ImplGlfw_InitForVulkan(App::Instance::GetWindowPointer(), true);

        uint32_t imageCount = VulkanSwapchain::GetSwapChainImageCount();

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.ApiVersion      = VK_API_VERSION_1_0;
        initInfo.Instance        = App::Instance::GetVulkanInstance();
        initInfo.PhysicalDevice  = VulkanDevice::GetPhysicalDevice();
        initInfo.Device          = VulkanDevice::GetDevice();
        initInfo.QueueFamily     = VulkanDevice::GetGraphicsQueueFamilyIndex();
        initInfo.Queue           = VulkanDevice::GetGraphicsQueue();
        initInfo.DescriptorPool  = s_pool;
        initInfo.MinImageCount   = imageCount;
        initInfo.ImageCount      = imageCount;
        initInfo.PipelineInfoMain.RenderPass   = VulkanPipeline::GetRenderPass();
        initInfo.PipelineInfoMain.MSAASamples  = VK_SAMPLE_COUNT_1_BIT;

        ImGui_ImplVulkan_Init(&initInfo);

        return true;
    }

    void Shutdown()
    {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        vkDestroyDescriptorPool(VulkanDevice::GetDevice(), s_pool, nullptr);
    }

    void BeginFrame()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void EndFrame()
    {
        ImGui::Render();
    }

    void Render(void* cmd)
    {
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), static_cast<VkCommandBuffer>(cmd));
    }
}
