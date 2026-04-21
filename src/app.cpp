#include "app.h"
#include "VulkanBackend/instance.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>
#include <unordered_map>
#include <string>

#include "vk_manager.h"
#include "VulkanBackend/device.h"
#include "VulkanBackend/swapchain.h"
#include "VulkanBackend/commands.h"
#include "VulkanBackend/sync.h"
#include "VulkanBackend/memory.h"
#include "mesh.h"
#include "texture.h"
#include "camera.h"
#include "imgui_manager.h"
#include "imgui.h"

namespace App
{
  static bool framebufferResized = false;
  static bool isFullscreen = false;

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
  static Mesh::AssetData mesh;
  static std::unordered_map<std::string, Texture::TextureData> textures;

  Mesh::AssetData& GetMesh() { return mesh; }

  static double lastMouseX  = 0.0;
  static double lastMouseY  = 0.0;
  static bool   firstMouse  = true;
  static bool   cursorLocked = true;

  static void KeyCallback(GLFWwindow* window, int key, int, int action, int)
  {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
      glfwSetWindowShouldClose(window, GLFW_TRUE);

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

    const char* modelPath = "assets/models/Person/scene.gltf";
    if (!Mesh::LoadFromFile(mesh, modelPath))
    {
      std::cout << "Failed to load mesh\n";
      return false;
    }

	//TODO MAKE A ASSET MANAGER
    //texture directory from model path
    std::string modelDir(modelPath);
    modelDir = modelDir.substr(0, modelDir.find_last_of("/\\") + 1);

    for (Mesh::MeshData& sub : mesh.meshes)
    {
      if (sub.texturePath.empty()) continue;

      std::string fullPath = modelDir + sub.texturePath;

      if (textures.find(fullPath) == textures.end())
      {
        Texture::TextureData tex;
        if (!Texture::LoadFromFile(tex, fullPath.c_str()))
        {
          std::cout << "Failed to load texture: " << fullPath << "\n";
          return false;
        }
        textures[fullPath] = tex;
      }

      sub.textureDescriptorSet = MemoryManager::AllocateTextureDescriptorSet(
        textures[fullPath].imageView, textures[fullPath].sampler);
    }

    return true;
  }

  void DrawFrame()
  {
    static double lastTime = glfwGetTime();
    double now = glfwGetTime();
    float dt   = static_cast<float>(now - lastTime);
    lastTime   = now;

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

    ImGuiManager::BeginFrame();
    ImGui::Begin("Debug - F1 For Cursor");
    if (ImGui::CollapsingHeader("Application Settings"))
      ImGui::Checkbox("Fullscreen", &isFullscreen);
    ImGui::End();
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
    Mesh::Destroy(mesh);
    for (auto& [path, tex] : textures)
      Texture::Destroy(tex);
    VulkanBackendManager::Shutdown();
    App::Instance::Destroy();
  }
}
