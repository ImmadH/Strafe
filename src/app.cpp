#include "app.h"
#include "VulkanBackend/instance.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>

#include "vk_manager.h"
#include "VulkanBackend/device.h"
#include "VulkanBackend/swapchain.h"
#include "VulkanBackend/commands.h"
#include "VulkanBackend/sync.h"
#include "mesh.h"
#include "camera.h"

namespace App
{
  static bool framebufferResized = false;
  static uint32_t currentFrame = 0;
  static uint32_t acquireSemaphoreIndex = 0;
  static Mesh::MeshData mesh;

  Mesh::MeshData& GetMesh() { return mesh; }

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

    if (!Mesh::LoadFromFile(mesh, "assets/models/Cube/scene.gltf"))
    {
      std::cout << "Failed to load mesh\n";
      return false;
    }

    return true;
  }

  void DrawFrame()
  {
    static double lastTime = glfwGetTime();
    double now = glfwGetTime();
    float dt   = static_cast<float>(now - lastTime);
    lastTime   = now;

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
    Mesh::Destroy(mesh);
    VulkanBackendManager::Shutdown();
    App::Instance::Destroy();
  }
}
