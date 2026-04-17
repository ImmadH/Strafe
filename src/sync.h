#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace VulkanSynchronization
{
	constexpr int MAX_FRAMES_IN_FLIGHT = 2;

	bool Create();
	void Destroy();

	VkSemaphore GetImageAvailableSemaphore(uint32_t acquireIndex);
	VkSemaphore GetRenderFinishedSemaphore(uint32_t frameIndex);
	VkFence GetInFlightFence(uint32_t frameIndex);
}
