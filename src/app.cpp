#include "app.h"
#include "VulkanBackend/instance.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <string>

#include "vk_manager.h"
#include "VulkanBackend/pipeline.h"
#include "VulkanBackend/device.h"
#include "VulkanBackend/swapchain.h"
#include "VulkanBackend/commands.h"
#include "VulkanBackend/sync.h"
#include "VulkanBackend/memory.h"
#include "mesh.h"
#include "texture.h"
#include "asset_manager.h"
#include "scene.h"
#include "camera.h"
#include "imgui_manager.h"
#include "imgui.h"
#include <glm/gtc/quaternion.hpp>
#include <functional>
#include <filesystem>
#include "cubemap.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace App
{
  static bool      framebufferResized  = false;
  static bool      isFullscreen        = false;
  static glm::vec3 lightDir            = glm::normalize(glm::vec3(1.0f, 2.0f, 1.0f));
  static bool      lightEnabled        = true;
  static bool      overrideMaterial    = false;
  static float     debugMetallic       = 1.0f;
  static float     debugRoughness      = 0.5f;
  static bool      useNormalMap        = true;
  static bool      useAO               = true;
  static bool      useIBL              = true;
  static int32_t   selectedNode        = -1;
  static bool      showScene           = false;
  static bool      showDebug           = false;
  static bool      showProfile         = false;
  static bool      showUI              = true;
  static bool      s_msaaPending      = false;


  struct ModelEntry { std::string name; std::string path; };
  static std::vector<ModelEntry> s_models;
  static int s_selectedModel = 0;
  static std::vector<ModelEntry> s_hdris;
  static int s_selectedHdri = 0;

  glm::vec3 GetLightDir()    { return lightDir; }
  bool      IsLightEnabled() { return lightEnabled; }

  static void ApplyFullscreenState(GLFWwindow* window)
  {
    if (isFullscreen == (glfwGetWindowMonitor(window) != nullptr)) return;

    if (isFullscreen)
    {
      GLFWmonitor* monitor = glfwGetPrimaryMonitor();
      const GLFWvidmode* mode = glfwGetVideoMode(monitor);
      glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    }
    else
    {
      glfwSetWindowMonitor(window, nullptr, 100, 100, 1280, 720, 0);
    }
    VulkanBackendManager::RecreateSwapChain();
  }
  static uint32_t currentFrame = 0;
  static uint32_t acquireSemaphoreIndex = 0;
  static Cubemap::IBLData ibl;
  static VkDescriptorSet  iblDescSet = VK_NULL_HANDLE;

  VkDescriptorSet GetIBLDescriptorSet()  { return iblDescSet; }
  VkBuffer        GetSkyboxVB()          { return ibl.skyboxVB; }
  bool            IsMaterialOverride()   { return overrideMaterial; }
  float           GetDebugMetallic()     { return debugMetallic; }
  float           GetDebugRoughness()    { return debugRoughness; }
  bool            UseNormalMap()         { return useNormalMap; }
  bool            UseAO()               { return useAO; }
  bool            UseIBL()              { return useIBL; }

  static double lastMouseX  = 0.0;
  static double lastMouseY  = 0.0;
  static bool   firstMouse  = true;
  static bool   cursorLocked = true;

  static void KeyCallback(GLFWwindow* window, int key, int, int action, int)
  {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
      glfwSetWindowShouldClose(window, GLFW_TRUE);

    if (key == GLFW_KEY_F2 && action == GLFW_PRESS)
      showUI = !showUI;

    if (key == GLFW_KEY_F1 && action == GLFW_PRESS)
    {
      cursorLocked = !cursorLocked;
      firstMouse = true;
      glfwSetInputMode(window, GLFW_CURSOR,
        cursorLocked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    }
  }

  static void MouseCallback(GLFWwindow*, double x, double y)
  {
    if (!cursorLocked)
    {
      lastMouseX = x;
      lastMouseY = y;
      return;
    }
    if (firstMouse)
    {
      lastMouseX = x;
      lastMouseY = y;
      firstMouse = false;
      return;
    }
    float dx = static_cast<float>(x - lastMouseX);
    float dy = static_cast<float>(lastMouseY - y); // y is flipped: up should increase pitch
    lastMouseX = x;
    lastMouseY = y;
    Camera::ProcessMouse(dx, dy);
  }

  static void FramebufferResizeCallback(GLFWwindow*, int, int)
  {
    framebufferResized = true;
  }

  static void RecreateSwapChain()
  {
    int width = 0, height = 0;
    while (width == 0 || height == 0)
    {
      glfwGetFramebufferSize(App::Instance::GetWindowPointer(), &width, &height);
      glfwWaitEvents();
    }

    VulkanBackendManager::RecreateSwapChain();
  }

  bool Init()
  {
    if (!App::Instance::Init())
    {
      std::cout << "Failed to init window\n";
      return false;
    }

    GLFWwindow* window = App::Instance::GetWindowPointer();
    glfwSetFramebufferSizeCallback(window, FramebufferResizeCallback);
    glfwSetKeyCallback(window, KeyCallback);
    glfwSetCursorPosCallback(window, MouseCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!VulkanBackendManager::Init())
      return false;

    if (!ImGuiManager::Init())
    {
      std::cout << "Failed to init ImGui\n";
      return false;
    }

    AssetManager::Init();
    Scene::Init();

    namespace fs = std::filesystem;
    for (auto& entry : fs::recursive_directory_iterator("assets/models"))
    {
      if (entry.path().extension() == ".gltf")
      {
        std::string path = entry.path().generic_string();
        std::string name = entry.path().parent_path().filename().string();
        s_models.push_back({ name, path });
      }
    }
    for (auto& entry : fs::recursive_directory_iterator("assets/hdri"))
    {
      if (entry.path().extension() == ".hdr")
      {
        std::string path = entry.path().generic_string();
        std::string name = entry.path().stem().string();
        s_hdris.push_back({ name, path });
      }
    }

    auto sponzaMesh = AssetManager::LoadMesh("assets/models/Sponza/Sponza.gltf");
    if (!sponzaMesh.valid())
    {
      std::cout << "Failed to load mesh\n";
      return false;
    }
    int32_t sponzaNode = Scene::AddNode("Sponza");
    Scene::GetNode(sponzaNode).mesh = sponzaMesh;


    if (!Cubemap::Create(ibl, "assets/hdri/sunset.hdr"))
    {
      std::cout << "Failed to create IBL\n";
      return false;
    }

    iblDescSet = MemoryManager::AllocateIBLDescriptorSet(
      ibl.irradiance.imageView,  ibl.irradiance.sampler,
      ibl.prefiltered.imageView, ibl.prefiltered.sampler,
      ibl.brdfLUTView,           ibl.brdfLUTSampler,
      ibl.environment.imageView, ibl.environment.sampler);

    return true;
  }

  void DrawFrame()
  {
    static double lastTime = glfwGetTime();
    double now    = glfwGetTime();
    float dt      = static_cast<float>(now - lastTime);
    float frameMs = dt * 1000.0f;
    lastTime      = now;

    ImGuiManager::ProfilerUpdate(dt);

    ApplyFullscreenState(App::Instance::GetWindowPointer());

    if (cursorLocked)
      Camera::ProcessKeyboard(App::Instance::GetWindowPointer(), dt);

    VkDevice device = VulkanDevice::GetDevice();
    VkFence inFlightFence = VulkanSynchronization::GetInFlightFence(currentFrame);
    VkSemaphore imageAvailable = VulkanSynchronization::GetImageAvailableSemaphore(acquireSemaphoreIndex);
    VkCommandBuffer commandBuffer = VulkanCommands::GetCommandBuffer(currentFrame);

    vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);

    VkExtent2D extent = VulkanSwapchain::GetSwapChainExtent();
    float aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    Camera::UpdateUBO(currentFrame, aspect);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device, VulkanSwapchain::GetSwapChain(), UINT64_MAX, imageAvailable, VK_NULL_HANDLE, &imageIndex);

    VkSemaphore renderFinished = VulkanSynchronization::GetRenderFinishedSemaphore(imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
      RecreateSwapChain();
      return;
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
      throw std::runtime_error("Failed to acquire swapchain image");
    }

    vkResetFences(device, 1, &inFlightFence);

    if (s_msaaPending)
    {
      s_msaaPending = false;
      framebufferResized = false;
      vkDeviceWaitIdle(device);
      ImGuiManager::Shutdown();
      VulkanBackendManager::RecreatePipeline();
      ImGuiManager::Init();
    }

    ImGuiManager::BeginFrame();

    if (showUI)
    {
    // ---- navbar ----
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg,     ImVec4(0.10f, 0.10f, 0.13f, 1.00f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.22f, 0.22f, 0.30f, 1.00f));
    if (ImGui::BeginMainMenuBar())
    {
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.78f, 1.00f, 1.00f));
      ImGui::Text(" STRAFE");
      ImGui::PopStyleColor();

      ImGui::Spacing();
      ImGui::Spacing();

      ImGui::MenuItem("Scene",   nullptr, &showScene);
      ImGui::MenuItem("Debug",   nullptr, &showDebug);
      ImGui::MenuItem("Profile", nullptr, &showProfile);

      ImGui::EndMainMenuBar();
    }
    ImGui::PopStyleColor(2);

    // ---- scene window ----
    if (showScene)
    {
      ImGui::SetNextWindowSize(ImVec2(280, 500), ImGuiCond_FirstUseEver);
      ImGui::SetNextWindowPos(ImVec2(10, 30),    ImGuiCond_FirstUseEver);
      ImGui::Begin("Scene", &showScene);

      // hierarchy
      if (ImGui::CollapsingHeader("Hierarchy", ImGuiTreeNodeFlags_DefaultOpen))
      {
        std::function<void(int32_t)> drawNode = [&](int32_t idx)
        {
          Scene::Node& node = Scene::GetNode(idx);
          if (!node.active) return;
          ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow
                                   | ImGuiTreeNodeFlags_SpanAvailWidth;
          if (node.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
          if (selectedNode == idx)   flags |= ImGuiTreeNodeFlags_Selected;

          bool open = ImGui::TreeNodeEx((void*)(intptr_t)idx, flags, "%s", node.name.c_str());
          if (ImGui::IsItemClicked())
            selectedNode = idx;

          if (open)
          {
            for (int32_t child : node.children) drawNode(child);
            ImGui::TreePop();
          }
        };

        for (int32_t i = 0; i < Scene::NodeCount(); i++)
          if (Scene::GetNode(i).parent == -1)
            drawNode(i);
      }

      // inspector
      ImGui::Spacing();
      if (ImGui::CollapsingHeader("Inspector", ImGuiTreeNodeFlags_DefaultOpen))
      {
        if (selectedNode >= 0 && selectedNode < Scene::NodeCount())
        {
          Scene::Node& node = Scene::GetNode(selectedNode);
          ImGui::Text("%s", node.name.c_str());
          ImGui::Checkbox("Active", &node.active);
          ImGui::Separator();

          ImGui::DragFloat3("Position", &node.localTransform.position.x, 0.01f);

          glm::vec3 euler = glm::degrees(glm::eulerAngles(node.localTransform.rotation));
          if (ImGui::DragFloat3("Rotation", &euler.x, 0.5f))
            node.localTransform.rotation = glm::quat(glm::radians(euler));
          ImGui::SameLine();
          if (ImGui::SmallButton("Reset##rot"))
            node.localTransform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

          ImGui::DragFloat3("Scale", &node.localTransform.scale.x, 0.01f, 0.001f, 100.0f);
          ImGui::SameLine();
          if (ImGui::SmallButton("Reset"))
            node.localTransform.scale = glm::vec3(1.0f);

          ImGui::Spacing();
          ImGui::Separator();
          if (ImGui::Button("Unload", ImVec2(-1, 0)))
          {
            vkDeviceWaitIdle(VulkanDevice::GetDevice());
            AssetManager::Release(Scene::GetNode(selectedNode).mesh);
            Scene::RemoveNode(selectedNode);
            selectedNode = -1;
          }
        }
        else
        {
          ImGui::TextDisabled("Nothing selected");
        }
      }

      // asset loader
      ImGui::Spacing();
      if (ImGui::CollapsingHeader("Assets", ImGuiTreeNodeFlags_DefaultOpen))
      {
        if (!s_models.empty())
        {
          ImGui::SetNextItemWidth(-1);
          if (ImGui::BeginCombo("##model", s_models[s_selectedModel].name.c_str()))
          {
            for (int i = 0; i < (int)s_models.size(); i++)
            {
              bool selected = (i == s_selectedModel);
              if (ImGui::Selectable(s_models[i].name.c_str(), selected))
                s_selectedModel = i;
              if (selected)
                ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
          }

          if (ImGui::Button("Load", ImVec2(-1, 0)))
          {
            const auto& entry = s_models[s_selectedModel];
            auto handle = AssetManager::LoadMesh(entry.path.c_str());
            if (handle.valid())
            {
              int32_t node = Scene::AddNode(entry.name.c_str());
              Scene::GetNode(node).mesh = handle;
            }
            else
            {
              std::cout << "Failed to load: " << entry.path << "\n";
            }
          }
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Environment");
        if (!s_hdris.empty())
        {
          ImGui::SetNextItemWidth(-1);
          if (ImGui::BeginCombo("##hdri", s_hdris[s_selectedHdri].name.c_str()))
          {
            for (int i = 0; i < (int)s_hdris.size(); i++)
            {
              bool selected = (i == s_selectedHdri);
              if (ImGui::Selectable(s_hdris[i].name.c_str(), selected))
                s_selectedHdri = i;
              if (selected)
                ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
          }
          if (ImGui::Button("Load##hdri", ImVec2(-1, 0)))
          {
            vkDeviceWaitIdle(VulkanDevice::GetDevice());
            Cubemap::Destroy(ibl);
            if (Cubemap::Create(ibl, s_hdris[s_selectedHdri].path.c_str()))
            {
              MemoryManager::UpdateIBLDescriptorSet(iblDescSet,
                ibl.irradiance.imageView,  ibl.irradiance.sampler,
                ibl.prefiltered.imageView, ibl.prefiltered.sampler,
                ibl.brdfLUTView,           ibl.brdfLUTSampler,
                ibl.environment.imageView, ibl.environment.sampler);
            }
            else
            {
              std::cout << "Failed to load HDRI: " << s_hdris[s_selectedHdri].path << "\n";
            }
          }
        }
      }

      ImGui::End();
    }

    // ---- debug window ----
    if (showDebug)
    {
      ImGui::SetNextWindowSize(ImVec2(280, 300), ImGuiCond_FirstUseEver);
      ImGui::SetNextWindowPos(ImVec2(10, 540),   ImGuiCond_FirstUseEver);
      ImGui::Begin("Debug", &showDebug);

      if (ImGui::CollapsingHeader("Application"))
      {
        ImGui::Checkbox("Fullscreen", &isFullscreen);
        bool msaa = VulkanPipeline::GetMSAAEnabled();
        if (ImGui::Checkbox("MSAA 8x", &msaa))
        {
          VulkanPipeline::SetMSAAEnabled(msaa);
          s_msaaPending = true;
        }
        float fov = Camera::GetFOV();
        if (ImGui::SliderFloat("FOV", &fov, 10.0f, 120.0f))
          Camera::SetFOV(fov);
      }

      if (ImGui::CollapsingHeader("Lighting"))
      {
        ImGui::Checkbox("Enabled", &lightEnabled);
        if (lightEnabled)
        {
          if (ImGui::DragFloat3("Direction", &lightDir.x, 0.01f, -1.0f, 1.0f))
            lightDir = glm::normalize(lightDir);
        }
      }

      if (ImGui::CollapsingHeader("Material"))
      {
        ImGui::Checkbox("Override Material", &overrideMaterial);
        if (overrideMaterial)
        {
          ImGui::SliderFloat("Metallic",  &debugMetallic,  0.0f, 1.0f);
          ImGui::SliderFloat("Roughness", &debugRoughness, 0.0f, 1.0f);
        }
        ImGui::Separator();
        ImGui::Checkbox("Normal Maps", &useNormalMap);
        ImGui::Checkbox("AO",          &useAO);
        ImGui::Checkbox("IBL",         &useIBL);
      }

      ImGui::End();
    }

    if (showProfile)
      ImGuiManager::DrawProfileWindow(showProfile);
    } // showUI

    ImGuiManager::EndFrame();

    vkResetCommandBuffer(commandBuffer, 0);
    VulkanCommands::RecordCommandBuffer(imageIndex, currentFrame);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &imageAvailable;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderFinished;

    if (vkQueueSubmit(VulkanDevice::GetGraphicsQueue(), 1, &submitInfo, inFlightFence) != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to submit draw command buffer");
    }

    VkSwapchainKHR swapChain = VulkanSwapchain::GetSwapChain();

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinished;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapChain;
    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(VulkanDevice::GetGraphicsQueue(), &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized)
    {
      framebufferResized = false;
      RecreateSwapChain();
    }
    else if (result != VK_SUCCESS)
    {
      throw std::runtime_error("Failed to present swapchain image");
    }

    currentFrame = (currentFrame + 1) % VulkanSynchronization::MAX_FRAMES_IN_FLIGHT;
    acquireSemaphoreIndex = (acquireSemaphoreIndex + 1) % VulkanSwapchain::GetSwapChainImageCount();
  }

  void mainLoop()
  {
    GLFWwindow* window = App::Instance::GetWindowPointer();
    while (!glfwWindowShouldClose(window))
    {
      glfwPollEvents();
      DrawFrame();
    }

    vkDeviceWaitIdle(VulkanDevice::GetDevice());
  }

  bool CreateSurface(void* surface)
  {
    VkInstance instance = App::Instance::GetVulkanInstance();
    if (glfwCreateWindowSurface(instance, App::Instance::GetWindowPointer(), nullptr, static_cast<VkSurfaceKHR*>(surface)) != VK_SUCCESS)
    {
      return false;
    }
    return true;
  }

  void cleanup()
  {
    ImGuiManager::Shutdown();
    Cubemap::Destroy(ibl);
    Scene::Shutdown();
    AssetManager::Shutdown();
    VulkanBackendManager::Shutdown();
    App::Instance::Destroy();
  }
}
