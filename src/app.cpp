#include "app.h"
#include "instance.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdexcept>

#include "device.h"
#include "swapchain.h"
#include "pipeline.h"
#include "commands.h"
#include "sync.h"
#include "mesh.h"
#include "camera.h"

namespace App
{
  static bool framebufferResized = false;
  static uint32_t currentFrame = 0;
  static uint32_t acquireSemaphoreIndex = 0;
  static Mesh triangleMesh;

  Mesh& GetTriangleMesh() { return triangleMesh; }

  static double lastMouseX  = 0.0;
  static double lastMouseY  = 0.0;
  static bool   firstMouse  = true;

  static void MouseCallback(GLFWwindow*, double x, double y)
  {
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

  void RecreateSwapChain()
  {
    // Wait out minimization
    int width = 0, height = 0;
    while (width == 0 || height == 0)
    {
      glfwGetFramebufferSize(App::Instance::GetWindowPointer(), &width, &height);
      glfwWaitEvents();
    }

    vkDeviceWaitIdle(VulkanDevice::GetDevice());

    VulkanPipeline::RecreateFramebuffers();  // destroys old framebuffers
    VulkanSwapchain::Destroy();              // destroys image views + swapchain
    VulkanSwapchain::Create();               // recreates swapchain + image views
    VulkanPipeline::RecreateFramebuffers();  // recreates framebuffers with new image views
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
    glfwSetCursorPosCallback(window, MouseCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

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

    if (!VulkanPipeline::Create())
    {
      std::cout << "Failed to create graphics pipeline\n";
      return false;
    }

    if (!VulkanCommands::Create())
    {
      std::cout << "Failed to create command buffers\n";
      return false;
    }

    if (!VulkanSynchronization::Create())
    {
      std::cout << "Failed to create sync objects\n";
      return false;
    }

    std::vector<Vertex> verts = {
      {{ 0.0f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}},
      {{ 0.5f,  0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}},
      {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}},
    };
    if (!triangleMesh.Upload(verts))
    {
      std::cout << "Failed to upload triangle mesh\n";
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
      if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
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
    VulkanSynchronization::Destroy();
    VulkanCommands::Destroy();
    VulkanPipeline::Destroy();
    VulkanSwapchain::Destroy();
    triangleMesh.Destroy();
    Camera::Destroy();
    VulkanDevice::Destroy();
    App::Instance::Destroy();
  }
}
