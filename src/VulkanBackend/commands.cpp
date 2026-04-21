#include "commands.h"
#include "device.h"
#include "pipeline.h"
#include "swapchain.h"
#include "sync.h"
#include "memory.h"
#include "app.h"
#include "camera.h"
#include <glm/gtc/type_ptr.hpp>
#include "imgui_manager.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <array>

namespace VulkanCommands
{
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::array<VkCommandBuffer, VulkanSynchronization::MAX_FRAMES_IN_FLIGHT> commandBuffers;

    bool CreateCommandPool()
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = VulkanDevice::GetGraphicsQueueFamilyIndex();

        if (vkCreateCommandPool(VulkanDevice::GetDevice(), &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create command pool");
        }

        return true;
    }

    bool CreateCommandBuffers()
    {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

        if (vkAllocateCommandBuffers(VulkanDevice::GetDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate command buffers");
        }

        return true;
    }

    bool Create()
    {
        if (!CreateCommandPool())    return false;
        if (!CreateCommandBuffers()) return false;
        return true;
    }

    void RecordCommandBuffer(uint32_t imageIndex, uint32_t frameIndex)
    {
        VkCommandBuffer cmd = commandBuffers[frameIndex];

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to begin recording command buffer");
        }

        VkClearValue clearValues[2];
        clearValues[0].color        = {{ 0.0f, 0.0f, 0.0f, 1.0f }};
        clearValues[1].depthStencil = { 1.0f, 0 };

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass      = VulkanPipeline::GetRenderPass();
        renderPassInfo.framebuffer     = VulkanPipeline::GetFramebuffers()[imageIndex];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = VulkanSwapchain::GetSwapChainExtent();
        renderPassInfo.clearValueCount = 2;
        renderPassInfo.pClearValues    = clearValues;

        vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanPipeline::GetGraphicsPipeline());

        VkDescriptorSet cameraDs = MemoryManager::GetCameraDescriptorSet(frameIndex);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            VulkanPipeline::GetPipelineLayout(), 0, 1, &cameraDs, 0, nullptr);

        VkDescriptorSet iblDs = App::GetIBLDescriptorSet();
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            VulkanPipeline::GetPipelineLayout(), 2, 1, &iblDs, 0, nullptr);

        struct PushData { glm::mat4 model; glm::vec4 lightDir; float metallicFactor; float roughnessFactor; };

        Mesh::AssetData& mesh = App::GetMesh();
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertexBuffer, &offset);
        vkCmdBindIndexBuffer(cmd, mesh.indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        VkExtent2D extent = VulkanSwapchain::GetSwapChainExtent();

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = { 0, 0 };
        scissor.extent = extent;
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        bool  matOverride = App::IsMaterialOverride();
        float debugMetal  = App::GetDebugMetallic();
        float debugRough  = App::GetDebugRoughness();

        for (const Mesh::MeshData& sub : mesh.meshes)
        {
            float mf = matOverride ? debugMetal : sub.metallicFactor;
            float rf = matOverride ? debugRough : sub.roughnessFactor;
            PushData push{ glm::mat4(1.0f), glm::vec4(App::GetLightDir(), 0.0f), mf, rf };
            vkCmdPushConstants(cmd, VulkanPipeline::GetPipelineLayout(),
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushData), &push);

            VkDescriptorSet texDs = sub.textureDescriptorSet != VK_NULL_HANDLE
                ? sub.textureDescriptorSet
                : MemoryManager::GetFallbackTextureDescriptorSet();
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                VulkanPipeline::GetPipelineLayout(), 1, 1, &texDs, 0, nullptr);

            vkCmdDrawIndexed(cmd, sub.indexCount, 1, sub.indexOffset, 0, 0);
        }

        // skybox — drawn after geometry, depth LESS_OR_EQUAL so it fills empty space only
        {
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanPipeline::GetSkyboxPipeline());
            VkBuffer skyboxVB = App::GetSkyboxVB();
            VkDeviceSize skyboxOffset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &skyboxVB, &skyboxOffset);
            vkCmdDraw(cmd, 36, 1, 0, 0);
        }

        ImGuiManager::Render(cmd);

        vkCmdEndRenderPass(cmd);

        if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to record command buffer");
        }
    }

    void Destroy()
    {
        vkDestroyCommandPool(VulkanDevice::GetDevice(), commandPool, nullptr);
    }

    VkCommandBuffer GetCommandBuffer(uint32_t frameIndex)
    {
        return commandBuffers[frameIndex];
    }

    VkCommandPool GetCommandPool()
    {
        return commandPool;
    }
}
