#include "sync.h"
#include "device.h"
#include "swapchain.h"
#include <stdexcept>
#include <array>
#include <vector>

namespace VulkanSynchronization
{
	// Both semaphore types indexed per swapchain image  prevents reuse while
	// the presentation engine still holds a reference to them.
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences;

	bool Create()
	{
		VkDevice device = VulkanDevice::GetDevice();
		uint32_t imageCount = VulkanSwapchain::GetSwapChainImageCount();

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		imageAvailableSemaphores.resize(imageCount);
		renderFinishedSemaphores.resize(imageCount);

		for (uint32_t i = 0; i < imageCount; i++)
		{
			if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("Failed to create semaphores");
			}
		}

		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			if (vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
				throw std::runtime_error("Failed to create fence");
		}

		return true;
	}

	void Destroy()
	{
		VkDevice device = VulkanDevice::GetDevice();

		for (VkSemaphore s : imageAvailableSemaphores)
			vkDestroySemaphore(device, s, nullptr);
		for (VkSemaphore s : renderFinishedSemaphores)
			vkDestroySemaphore(device, s, nullptr);

		imageAvailableSemaphores.clear();
		renderFinishedSemaphores.clear();

		for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
			vkDestroyFence(device, inFlightFences[i], nullptr);
	}

	VkSemaphore GetImageAvailableSemaphore(uint32_t acquireIndex) { return imageAvailableSemaphores[acquireIndex]; }
	VkSemaphore GetRenderFinishedSemaphore(uint32_t imageIndex)   { return renderFinishedSemaphores[imageIndex]; }
	VkFence     GetInFlightFence(uint32_t frameIndex)             { return inFlightFences[frameIndex]; }
}
