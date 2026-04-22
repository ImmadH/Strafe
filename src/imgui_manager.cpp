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
#include <algorithm>
#include <cstdio>

namespace ImGuiManager
{
    static VkDescriptorPool s_pool = VK_NULL_HANDLE;

    // ---- profiler data ----
    static constexpr int k_historySize = 300;
    static float s_frameTimes[k_historySize] = {};
    static float s_fpsHistory[k_historySize] = {};
    static int   s_frameTimeHead = 0;
    static int   s_frameCount    = 0;

    static float  s_d_fps          = 0.0f;
    static float  s_d_frameMs      = 0.0f;
    static float  s_d_avg          = 0.0f;
    static float  s_refreshAccum   = 0.0f;

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
        initInfo.PipelineInfoMain.RenderPass  = VulkanPipeline::GetRenderPass();
        initInfo.PipelineInfoMain.MSAASamples = VulkanPipeline::GetMSAAEnabled()
            ? VK_SAMPLE_COUNT_8_BIT
            : VK_SAMPLE_COUNT_1_BIT;

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

    void ProfilerUpdate(float dt)
    {
        float frameMs = dt * 1000.0f;
        s_frameTimes[s_frameTimeHead] = frameMs;
        s_fpsHistory[s_frameTimeHead] = dt > 0.0f ? 1.0f / dt : 0.0f;
        s_frameTimeHead = (s_frameTimeHead + 1) % k_historySize;
        if (s_frameCount < k_historySize) ++s_frameCount;

        s_refreshAccum += dt;
        if (s_refreshAccum >= 0.3f)
        {
            int   filled = s_frameCount;
            float sum    = 0.0f;
            for (int i = 0; i < filled; i++) sum += s_frameTimes[i];
            s_d_avg      = filled > 0 ? sum / filled : 0.0f;
            s_d_fps      = dt > 0.0f ? 1.0f / dt : 0.0f;
            s_d_frameMs  = frameMs;
            s_refreshAccum = 0.0f;
        }
    }

    void DrawProfileWindow(bool& show)
    {
        ImGui::SetNextWindowSize(ImVec2(460, 370), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(300, 30),   ImGuiCond_FirstUseEver);
        ImGui::Begin("Profile", &show);

        ImGui::Text("FPS");         ImGui::SameLine(220); ImGui::Text("%.1f", s_d_fps);
        ImGui::Text("Frame Time:"); ImGui::SameLine(220); ImGui::Text("%.2f ms", s_d_frameMs);
        ImGui::Separator();
        float avgFps = s_d_avg > 0.0f ? 1000.0f / s_d_avg : 0.0f;
        ImGui::Text("Avg  %.2f ms", s_d_avg); ImGui::SameLine(300); ImGui::Text("%.1f fps", avgFps);
        ImGui::Spacing();

        ImDrawList* dl     = ImGui::GetWindowDrawList();
        float       fontSz = ImGui::GetFontSize();
        float       labelW = fontSz * 3.2f;
        float       availW = ImGui::GetContentRegionAvail().x;
        float       graphW = availW - labelW - 4.0f;

        // ---- FPS line graph ----
        ImGui::TextDisabled("Frame Rate");
        {
            float  graphH = 140.0f;
            ImVec2 p0     = ImGui::GetCursorScreenPos();
            ImVec2 p1     = ImVec2(p0.x + graphW, p0.y + graphH);

            dl->AddRectFilled(p0, p1, IM_COL32(12, 12, 18, 255));

            static constexpr float k_fpsScale  = 300.0f;
            static constexpr float k_gridFps[] = { 60.f, 120.f, 180.f, 240.f };
            for (float g : k_gridFps)
            {
                float gy = p1.y - (g / k_fpsScale) * graphH;
                dl->AddLine(ImVec2(p0.x, gy), ImVec2(p1.x, gy), IM_COL32(45, 45, 60, 255));
                char buf[8]; snprintf(buf, sizeof(buf), "%d", (int)g);
                dl->AddText(ImVec2(p1.x + 4.f, gy - fontSz * 0.5f), IM_COL32(120, 120, 150, 255), buf);
            }

            float xStep = graphW / (float)(k_historySize - 1);
            for (int i = 0; i < k_historySize - 1; i++)
            {
                int   ia = (s_frameTimeHead + i)     % k_historySize;
                int   ib = (s_frameTimeHead + i + 1) % k_historySize;
                float fa = std::min(s_fpsHistory[ia] / k_fpsScale, 1.0f);
                float fb = std::min(s_fpsHistory[ib] / k_fpsScale, 1.0f);
                float x0 = p0.x + i       * xStep;
                float x1 = p0.x + (i + 1) * xStep;
                float y0 = p1.y - fa * graphH;
                float y1 = p1.y - fb * graphH;
                dl->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(70, 150, 255, 230), 1.5f);
            }

            dl->AddRect(p0, p1, IM_COL32(50, 50, 70, 255));
            ImGui::Dummy(ImVec2(availW, graphH));
            ImGui::Spacing();
        }

        // ---- frame-time inset ----
        ImGui::TextDisabled("Frame Time (ms)");
        {
            float  insetH   = 60.0f;
            ImVec2 p0       = ImGui::GetCursorScreenPos();
            ImVec2 p1       = ImVec2(p0.x + graphW, p0.y + insetH);
            float  ftScale  = 50.0f;
            float  targetMs = 16.667f;

            dl->AddRectFilled(p0, p1, IM_COL32(12, 12, 18, 255));

            float refY = p1.y - (targetMs / ftScale) * insetH;
            dl->AddLine(ImVec2(p0.x, refY), ImVec2(p1.x, refY), IM_COL32(70, 200, 70, 90));
            dl->AddText(ImVec2(p1.x + 4.f, refY - fontSz * 0.5f), IM_COL32(70, 200, 70, 200), "16ms");

            float barW = graphW / (float)k_historySize;
            for (int i = 0; i < k_historySize; i++)
            {
                int   idx = (s_frameTimeHead + i) % k_historySize;
                float ms  = s_frameTimes[idx];
                float t   = std::min(ms / ftScale, 1.0f);
                float x0  = p0.x + i * barW;
                float x1  = x0 + std::max(barW - 1.0f, 1.0f);
                float y0  = p1.y - t * insetH;
                ImU32 col = ms > targetMs * 2.0f ? IM_COL32(255, 70, 70, 210) : IM_COL32(80, 150, 255, 180);
                dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, p1.y), col);
            }

            dl->AddRect(p0, p1, IM_COL32(50, 50, 70, 255));
            ImGui::Dummy(ImVec2(availW, insetH));
        }

        ImGui::End();
    }
}
