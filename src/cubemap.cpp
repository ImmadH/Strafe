#include "cubemap.h"
#include "VulkanBackend/device.h"
#include "VulkanBackend/memory.h"
#include "stb_image.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <vector>
#include <array>
#include <cstring>
#include <stdexcept>
#include <iostream>

namespace Cubemap
{
    static constexpr uint32_t ENV_SIZE       = 512;
    static constexpr uint32_t IRR_SIZE       = 32;
    static constexpr uint32_t PREFILTER_SIZE = 128;
    static constexpr uint32_t PREFILTER_MIPS = 5;
    static constexpr uint32_t BRDF_SIZE      = 512;

    static const float CUBE_VERTS[] = {
        -1, 1,-1,  -1,-1,-1,   1,-1,-1,   1,-1,-1,   1, 1,-1,  -1, 1,-1,
        -1,-1, 1,  -1, 1, 1,   1, 1, 1,   1, 1, 1,   1,-1, 1,  -1,-1, 1,
        -1, 1, 1,  -1, 1,-1,  -1,-1,-1,  -1,-1,-1,  -1,-1, 1,  -1, 1, 1,
         1, 1, 1,   1,-1,-1,   1, 1,-1,   1,-1,-1,   1, 1, 1,   1,-1, 1,
        -1,-1,-1,   1,-1,-1,   1,-1, 1,   1,-1, 1,  -1,-1, 1,  -1,-1,-1,
        -1, 1,-1,   1, 1, 1,   1, 1,-1,   1, 1, 1,  -1, 1,-1,  -1, 1, 1,
    };

    struct CubePush { glm::mat4 viewProj; float roughness; };

    // -------------------------------------------------------------------------
    static std::vector<char> ReadSPV(const char* path)
    {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open())
            throw std::runtime_error(std::string("Failed to open shader: ") + path);
        size_t size = (size_t)file.tellg();
        std::vector<char> buf(size);
        file.seekg(0);
        file.read(buf.data(), size);
        return buf;
    }

    static VkShaderModule CreateShaderModule(const std::vector<char>& code)
    {
        VkShaderModuleCreateInfo info{};
        info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize = code.size();
        info.pCode    = reinterpret_cast<const uint32_t*>(code.data());
        VkShaderModule mod;
        vkCreateShaderModule(VulkanDevice::GetDevice(), &info, nullptr, &mod);
        return mod;
    }

    static VkRenderPass CreateOffscreenRenderPass(VkFormat format)
    {
        VkAttachmentDescription color{};
        color.format         = format;
        color.samples        = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        color.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout    = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentReference ref{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &ref;

        VkSubpassDependency deps[2]{};
        deps[0].srcSubpass    = VK_SUBPASS_EXTERNAL;
        deps[0].dstSubpass    = 0;
        deps[0].srcStageMask  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        deps[0].dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        deps[1].srcSubpass    = 0;
        deps[1].dstSubpass    = VK_SUBPASS_EXTERNAL;
        deps[1].srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[1].dstStageMask  = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        VkRenderPassCreateInfo rpInfo{};
        rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        rpInfo.attachmentCount = 1;
        rpInfo.pAttachments    = &color;
        rpInfo.subpassCount    = 1;
        rpInfo.pSubpasses      = &subpass;
        rpInfo.dependencyCount = 2;
        rpInfo.pDependencies   = deps;

        VkRenderPass rp;
        vkCreateRenderPass(VulkanDevice::GetDevice(), &rpInfo, nullptr, &rp);
        return rp;
    }

    static VkDescriptorSetLayout CreateSamplerLayout()
    {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding         = 0;
        binding.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo info{};
        info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = 1;
        info.pBindings    = &binding;

        VkDescriptorSetLayout layout;
        vkCreateDescriptorSetLayout(VulkanDevice::GetDevice(), &info, nullptr, &layout);
        return layout;
    }

    static VkDescriptorSet AllocateInputSet(VkDescriptorPool pool, VkDescriptorSetLayout layout,
                                            VkImageView view, VkSampler sampler)
    {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool     = pool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &layout;

        VkDescriptorSet ds;
        vkAllocateDescriptorSets(VulkanDevice::GetDevice(), &allocInfo, &ds);

        VkDescriptorImageInfo imgInfo{};
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imgInfo.imageView   = view;
        imgInfo.sampler     = sampler;

        VkWriteDescriptorSet write{};
        write.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet          = ds;
        write.dstBinding      = 0;
        write.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo      = &imgInfo;

        vkUpdateDescriptorSets(VulkanDevice::GetDevice(), 1, &write, 0, nullptr);
        return ds;
    }

    struct Pipeline { VkPipeline handle = VK_NULL_HANDLE; VkPipelineLayout layout = VK_NULL_HANDLE; };

    static Pipeline CreateCubePipeline(VkRenderPass rp, VkDescriptorSetLayout dsLayout,
                                       const char* vertSpv, const char* fragSpv)
    {
        VkDevice device = VulkanDevice::GetDevice();

        auto vertCode = ReadSPV(vertSpv);
        auto fragCode = ReadSPV(fragSpv);
        VkShaderModule vertMod = CreateShaderModule(vertCode);
        VkShaderModule fragMod = CreateShaderModule(fragCode);

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertMod;
        stages[0].pName  = "main";
        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragMod;
        stages[1].pName  = "main";

        VkVertexInputBindingDescription binding{};
        binding.binding   = 0;
        binding.stride    = sizeof(float) * 3;
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attr{};
        attr.binding  = 0;
        attr.location = 0;
        attr.format   = VK_FORMAT_R32G32B32_SFLOAT;
        attr.offset   = 0;

        VkPipelineVertexInputStateCreateInfo vertInput{};
        vertInput.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertInput.vertexBindingDescriptionCount   = 1;
        vertInput.pVertexBindingDescriptions      = &binding;
        vertInput.vertexAttributeDescriptionCount = 1;
        vertInput.pVertexAttributeDescriptions    = &attr;

        VkPipelineInputAssemblyStateCreateInfo ia{};
        ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode    = VK_CULL_MODE_NONE;
        raster.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth   = 1.0f;

        VkPipelineMultisampleStateCreateInfo ms{};
        ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState blendAtt{};
        blendAtt.colorWriteMask = 0xF;

        VkPipelineColorBlendStateCreateInfo blend{};
        blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments    = &blendAtt;

        VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dyn{};
        dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dyn.dynamicStateCount = 2;
        dyn.pDynamicStates    = dynStates;

        VkPipelineViewportStateCreateInfo vp{};
        vp.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vp.viewportCount = 1;
        vp.scissorCount  = 1;

        VkPushConstantRange pushRange{};
        pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushRange.size       = sizeof(CubePush);

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount         = 1;
        layoutInfo.pSetLayouts            = &dsLayout;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges    = &pushRange;

        Pipeline out{};
        vkCreatePipelineLayout(device, &layoutInfo, nullptr, &out.layout);

        VkGraphicsPipelineCreateInfo pipeInfo{};
        pipeInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeInfo.stageCount          = 2;
        pipeInfo.pStages             = stages;
        pipeInfo.pVertexInputState   = &vertInput;
        pipeInfo.pInputAssemblyState = &ia;
        pipeInfo.pViewportState      = &vp;
        pipeInfo.pRasterizationState = &raster;
        pipeInfo.pMultisampleState   = &ms;
        pipeInfo.pColorBlendState    = &blend;
        pipeInfo.pDynamicState       = &dyn;
        pipeInfo.layout              = out.layout;
        pipeInfo.renderPass          = rp;

        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &out.handle);

        vkDestroyShaderModule(device, vertMod, nullptr);
        vkDestroyShaderModule(device, fragMod, nullptr);

        return out;
    }

    static Pipeline CreateBRDFPipeline(VkRenderPass rp)
    {
        VkDevice device = VulkanDevice::GetDevice();

        auto vertCode = ReadSPV("shaders/compiled/brdf.vert.spv");
        auto fragCode = ReadSPV("shaders/compiled/brdf.frag.spv");
        VkShaderModule vertMod = CreateShaderModule(vertCode);
        VkShaderModule fragMod = CreateShaderModule(fragCode);

        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertMod;
        stages[0].pName  = "main";
        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragMod;
        stages[1].pName  = "main";

        VkPipelineVertexInputStateCreateInfo vertInput{};
        vertInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

        VkPipelineInputAssemblyStateCreateInfo ia{};
        ia.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode    = VK_CULL_MODE_NONE;
        raster.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth   = 1.0f;

        VkPipelineMultisampleStateCreateInfo ms{};
        ms.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState blendAtt{};
        blendAtt.colorWriteMask = 0xF;

        VkPipelineColorBlendStateCreateInfo blend{};
        blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments    = &blendAtt;

        VkDynamicState dynStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dyn{};
        dyn.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dyn.dynamicStateCount = 2;
        dyn.pDynamicStates    = dynStates;

        VkPipelineViewportStateCreateInfo vp{};
        vp.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        vp.viewportCount = 1;
        vp.scissorCount  = 1;

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

        Pipeline out{};
        vkCreatePipelineLayout(device, &layoutInfo, nullptr, &out.layout);

        VkGraphicsPipelineCreateInfo pipeInfo{};
        pipeInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeInfo.stageCount          = 2;
        pipeInfo.pStages             = stages;
        pipeInfo.pVertexInputState   = &vertInput;
        pipeInfo.pInputAssemblyState = &ia;
        pipeInfo.pViewportState      = &vp;
        pipeInfo.pRasterizationState = &raster;
        pipeInfo.pMultisampleState   = &ms;
        pipeInfo.pColorBlendState    = &blend;
        pipeInfo.pDynamicState       = &dyn;
        pipeInfo.layout              = out.layout;
        pipeInfo.renderPass          = rp;

        vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &out.handle);

        vkDestroyShaderModule(device, vertMod, nullptr);
        vkDestroyShaderModule(device, fragMod, nullptr);

        return out;
    }

    static void RenderCubeFaces(VkRenderPass rp, const Pipeline& pipeline,
                                VkDescriptorSet inputSet, VkBuffer cubeVB,
                                CubemapData& target, uint32_t size,
                                uint32_t mipLevel, float roughness)
    {
        VkDevice device = VulkanDevice::GetDevice();

        // Sascha Willems' face rotation matrices — correct for Vulkan cubemap conventions
        const std::array<glm::mat4, 6> faceViews = {{
            glm::rotate(glm::rotate(glm::mat4(1.f), glm::radians( 90.f), {0,1,0}), glm::radians(180.f), {1,0,0}),
            glm::rotate(glm::rotate(glm::mat4(1.f), glm::radians(-90.f), {0,1,0}), glm::radians(180.f), {1,0,0}),
            glm::rotate(glm::mat4(1.f), glm::radians(-90.f), {1,0,0}),
            glm::rotate(glm::mat4(1.f), glm::radians( 90.f), {1,0,0}),
            glm::rotate(glm::mat4(1.f), glm::radians(180.f), {1,0,0}),
            glm::rotate(glm::mat4(1.f), glm::radians(180.f), {0,0,1}),
        }};
        const glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);

        // create per-face views and framebuffers
        std::array<VkImageView,   6> faceViews2D{};
        std::array<VkFramebuffer, 6> faceFramebuffers{};

        for (uint32_t f = 0; f < 6; f++)
        {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image                           = target.image;
            viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format                          = VK_FORMAT_R16G16B16A16_SFLOAT;
            viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel   = mipLevel;
            viewInfo.subresourceRange.levelCount     = 1;
            viewInfo.subresourceRange.baseArrayLayer = f;
            viewInfo.subresourceRange.layerCount     = 1;
            vkCreateImageView(device, &viewInfo, nullptr, &faceViews2D[f]);

            VkFramebufferCreateInfo fbInfo{};
            fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            fbInfo.renderPass      = rp;
            fbInfo.attachmentCount = 1;
            fbInfo.pAttachments    = &faceViews2D[f];
            fbInfo.width           = size;
            fbInfo.height          = size;
            fbInfo.layers          = 1;
            vkCreateFramebuffer(device, &fbInfo, nullptr, &faceFramebuffers[f]);
        }

        VkCommandBuffer cmd = MemoryManager::BeginOneTimeCommands();

        VkViewport viewport{};
        viewport.width    = static_cast<float>(size);
        viewport.height   = static_cast<float>(size);
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.extent = { size, size };
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        for (uint32_t f = 0; f < 6; f++)
        {
            VkClearValue clear{};
            clear.color = {{ 0,0,0,1 }};

            VkRenderPassBeginInfo rpBegin{};
            rpBegin.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpBegin.renderPass      = rp;
            rpBegin.framebuffer     = faceFramebuffers[f];
            rpBegin.renderArea      = { {0,0}, {size, size} };
            rpBegin.clearValueCount = 1;
            rpBegin.pClearValues    = &clear;

            vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle);
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                pipeline.layout, 0, 1, &inputSet, 0, nullptr);

            CubePush push{ proj * faceViews[f], roughness };
            vkCmdPushConstants(cmd, pipeline.layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, sizeof(CubePush), &push);

            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(cmd, 0, 1, &cubeVB, &offset);
            vkCmdDraw(cmd, 36, 1, 0, 0);

            vkCmdEndRenderPass(cmd);
        }

        MemoryManager::EndOneTimeCommands(cmd);

        for (uint32_t f = 0; f < 6; f++)
        {
            vkDestroyFramebuffer(device, faceFramebuffers[f], nullptr);
            vkDestroyImageView(device,   faceViews2D[f],      nullptr);
        }
    }

    static void RenderBRDFLUT(VkRenderPass rp, const Pipeline& pipeline, IBLData& ibl)
    {
        VkDevice device = VulkanDevice::GetDevice();

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass      = rp;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments    = &ibl.brdfLUTView;
        fbInfo.width           = BRDF_SIZE;
        fbInfo.height          = BRDF_SIZE;
        fbInfo.layers          = 1;

        VkFramebuffer fb;
        vkCreateFramebuffer(device, &fbInfo, nullptr, &fb);

        VkCommandBuffer cmd = MemoryManager::BeginOneTimeCommands();

        VkClearValue clear{};
        clear.color = {{ 0,0,0,1 }};

        VkRenderPassBeginInfo rpBegin{};
        rpBegin.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBegin.renderPass      = rp;
        rpBegin.framebuffer     = fb;
        rpBegin.renderArea      = { {0,0}, {BRDF_SIZE, BRDF_SIZE} };
        rpBegin.clearValueCount = 1;
        rpBegin.pClearValues    = &clear;

        VkViewport viewport{};
        viewport.width    = BRDF_SIZE;
        viewport.height   = BRDF_SIZE;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.extent = { BRDF_SIZE, BRDF_SIZE };

        vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle);
        vkCmdDraw(cmd, 3, 1, 0, 0); // fullscreen triangle
        vkCmdEndRenderPass(cmd);

        MemoryManager::EndOneTimeCommands(cmd);
        vkDestroyFramebuffer(device, fb, nullptr);
    }

    static void DestroyCubemapData(CubemapData& d)
    {
        VkDevice device = VulkanDevice::GetDevice();
        if (d.sampler   != VK_NULL_HANDLE) vkDestroySampler(device, d.sampler, nullptr);
        if (d.imageView != VK_NULL_HANDLE) vkDestroyImageView(device, d.imageView, nullptr);
        if (d.image     != VK_NULL_HANDLE) vmaDestroyImage(VulkanDevice::GetAllocator(), d.image, d.allocation);
        d = {};
    }

    static bool CreateCubemapImage(CubemapData& out, uint32_t size, uint32_t mips, VkFormat format,
                                   VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
    {
        VkImageCreateInfo imgInfo{};
        imgInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.flags         = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        imgInfo.imageType     = VK_IMAGE_TYPE_2D;
        imgInfo.extent        = { size, size, 1 };
        imgInfo.mipLevels     = mips;
        imgInfo.arrayLayers   = 6;
        imgInfo.format        = format;
        imgInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgInfo.usage         = usage;
        imgInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        if (vmaCreateImage(VulkanDevice::GetAllocator(), &imgInfo, &allocInfo,
                           &out.image, &out.allocation, nullptr) != VK_SUCCESS)
            return false;

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = out.image;
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_CUBE;
        viewInfo.format                          = format;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount     = mips;
        viewInfo.subresourceRange.layerCount     = 6;

        if (vkCreateImageView(VulkanDevice::GetDevice(), &viewInfo, nullptr, &out.imageView) != VK_SUCCESS)
            return false;

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter    = VK_FILTER_LINEAR;
        samplerInfo.minFilter    = VK_FILTER_LINEAR;
        samplerInfo.mipmapMode   = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxLod       = static_cast<float>(mips);

        if (vkCreateSampler(VulkanDevice::GetDevice(), &samplerInfo, nullptr, &out.sampler) != VK_SUCCESS)
            return false;

        return true;
    }

    static bool CreateBRDFLUTImage(IBLData& ibl)
    {
        VkImageCreateInfo imgInfo{};
        imgInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.imageType     = VK_IMAGE_TYPE_2D;
        imgInfo.extent        = { BRDF_SIZE, BRDF_SIZE, 1 };
        imgInfo.mipLevels     = 1;
        imgInfo.arrayLayers   = 1;
        imgInfo.format        = VK_FORMAT_R16G16_SFLOAT;
        imgInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgInfo.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imgInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

        if (vmaCreateImage(VulkanDevice::GetAllocator(), &imgInfo, &allocInfo,
                           &ibl.brdfLUT, &ibl.brdfLUTAlloc, nullptr) != VK_SUCCESS)
            return false;

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = ibl.brdfLUT;
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = VK_FORMAT_R16G16_SFLOAT;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.layerCount     = 1;

        if (vkCreateImageView(VulkanDevice::GetDevice(), &viewInfo, nullptr, &ibl.brdfLUTView) != VK_SUCCESS)
            return false;

        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter    = VK_FILTER_LINEAR;
        samplerInfo.minFilter    = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxLod       = 1.0f;

        if (vkCreateSampler(VulkanDevice::GetDevice(), &samplerInfo, nullptr, &ibl.brdfLUTSampler) != VK_SUCCESS)
            return false;

        return true;
    }

    bool Create(IBLData& ibl, const char* hdrPath)
    {
        VkDevice device = VulkanDevice::GetDevice();

        std::cout << "[IBL] Loading HDR: " << hdrPath << "\n";
        int   w, h, ch;
        float* pixels = stbi_loadf(hdrPath, &w, &h, &ch, STBI_rgb_alpha);
        if (!pixels)
        {
            std::cout << "[IBL] stbi_loadf failed: " << stbi_failure_reason() << "\n";
            return false;
        }
        std::cout << "[IBL] HDR loaded: " << w << "x" << h << "\n";

        VkImage       hdrImage; VmaAllocation hdrAlloc;
        VkImageView   hdrView;  VkSampler     hdrSampler;

        {
            VkImageCreateInfo imgInfo{};
            imgInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imgInfo.imageType     = VK_IMAGE_TYPE_2D;
            imgInfo.extent        = { (uint32_t)w, (uint32_t)h, 1 };
            imgInfo.mipLevels     = 1;
            imgInfo.arrayLayers   = 1;
            imgInfo.format        = VK_FORMAT_R32G32B32A32_SFLOAT;
            imgInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
            imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imgInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            imgInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
            imgInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
            VmaAllocationCreateInfo ai{};
            ai.usage = VMA_MEMORY_USAGE_GPU_ONLY;
            vmaCreateImage(VulkanDevice::GetAllocator(), &imgInfo, &ai, &hdrImage, &hdrAlloc, nullptr);

            // 4 floats × 4 bytes = 16 bytes per pixel
            MemoryManager::UploadImage(hdrImage, pixels, (uint32_t)w, (uint32_t)h, 16);
            stbi_image_free(pixels);

            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image                           = hdrImage;
            viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format                          = VK_FORMAT_R32G32B32A32_SFLOAT;
            viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.levelCount     = 1;
            viewInfo.subresourceRange.layerCount     = 1;
            vkCreateImageView(device, &viewInfo, nullptr, &hdrView);

            VkSamplerCreateInfo si{};
            si.sType        = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            si.magFilter    = VK_FILTER_LINEAR;
            si.minFilter    = VK_FILTER_LINEAR;
            si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            si.maxLod       = 1.0f;
            vkCreateSampler(device, &si, nullptr, &hdrSampler);
        }

        std::cout << "[IBL] Allocating cubemap images\n";
        if (!CreateCubemapImage(ibl.environment, ENV_SIZE,       1,              VK_FORMAT_R16G16B16A16_SFLOAT)) { std::cout << "[IBL] Failed: environment image\n"; return false; }
        if (!CreateCubemapImage(ibl.irradiance,  IRR_SIZE,       1,              VK_FORMAT_R16G16B16A16_SFLOAT)) { std::cout << "[IBL] Failed: irradiance image\n"; return false; }
        if (!CreateCubemapImage(ibl.prefiltered, PREFILTER_SIZE, PREFILTER_MIPS, VK_FORMAT_R16G16B16A16_SFLOAT)) { std::cout << "[IBL] Failed: prefiltered image\n"; return false; }
        if (!CreateBRDFLUTImage(ibl)) { std::cout << "[IBL] Failed: BRDF LUT image\n"; return false; }

        VkBuffer      cubeVB;
        VmaAllocation cubeAlloc;
        {
            VkBufferCreateInfo bufInfo{};
            bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufInfo.size  = sizeof(CUBE_VERTS);
            bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            VmaAllocationCreateInfo ai{};
            ai.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
            ai.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
            VmaAllocationInfo info;
            vmaCreateBuffer(VulkanDevice::GetAllocator(), &bufInfo, &ai, &cubeVB, &cubeAlloc, &info);
            memcpy(info.pMappedData, CUBE_VERTS, sizeof(CUBE_VERTS));
        }

        VkDescriptorPool dsPool;
        {
            VkDescriptorPoolSize poolSize{};
            poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            poolSize.descriptorCount = 3;
            VkDescriptorPoolCreateInfo poolInfo{};
            poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.poolSizeCount = 1;
            poolInfo.pPoolSizes    = &poolSize;
            poolInfo.maxSets       = 3;
            vkCreateDescriptorPool(device, &poolInfo, nullptr, &dsPool);
        }
        VkDescriptorSetLayout dsLayout = CreateSamplerLayout();

        std::cout << "[IBL] Creating render passes\n";
        VkRenderPass cubeRP = CreateOffscreenRenderPass(VK_FORMAT_R16G16B16A16_SFLOAT);
        VkRenderPass brdfRP = CreateOffscreenRenderPass(VK_FORMAT_R16G16_SFLOAT);

        std::cout << "[IBL] Creating pipelines\n";
        Pipeline equirectPipeline = CreateCubePipeline(cubeRP, dsLayout,
            "shaders/compiled/cubemap.vert.spv",
            "shaders/compiled/equirect_to_cube.frag.spv");

        Pipeline irradiancePipeline = CreateCubePipeline(cubeRP, dsLayout,
            "shaders/compiled/cubemap.vert.spv",
            "shaders/compiled/irradiance.frag.spv");

        Pipeline prefilterPipeline = CreateCubePipeline(cubeRP, dsLayout,
            "shaders/compiled/cubemap.vert.spv",
            "shaders/compiled/prefilter.frag.spv");

        Pipeline brdfPipeline = CreateBRDFPipeline(brdfRP);

        std::cout << "[IBL] Pass 1: equirect -> environment\n";
        {
            VkDescriptorSet ds = AllocateInputSet(dsPool, dsLayout, hdrView, hdrSampler);
            RenderCubeFaces(cubeRP, equirectPipeline, ds, cubeVB, ibl.environment, ENV_SIZE, 0, 0.0f);
        }

        std::cout << "[IBL] Pass 2: irradiance convolution\n";
        {
            VkDescriptorSet ds = AllocateInputSet(dsPool, dsLayout, ibl.environment.imageView, ibl.environment.sampler);
            RenderCubeFaces(cubeRP, irradiancePipeline, ds, cubeVB, ibl.irradiance, IRR_SIZE, 0, 0.0f);
        }

        std::cout << "[IBL] Pass 3: prefilter\n";
        {
            VkDescriptorSet ds = AllocateInputSet(dsPool, dsLayout, ibl.environment.imageView, ibl.environment.sampler);
            for (uint32_t mip = 0; mip < PREFILTER_MIPS; mip++)
            {
                uint32_t mipSize   = PREFILTER_SIZE >> mip;
                float    roughness = static_cast<float>(mip) / static_cast<float>(PREFILTER_MIPS - 1);
                std::cout << "[IBL]   mip " << mip << " (" << mipSize << "x" << mipSize << ") roughness=" << roughness << "\n";
                RenderCubeFaces(cubeRP, prefilterPipeline, ds, cubeVB, ibl.prefiltered, mipSize, mip, roughness);
            }
        }

        std::cout << "[IBL] Pass 4: BRDF LUT\n";
        RenderBRDFLUT(brdfRP, brdfPipeline, ibl);
        std::cout << "[IBL] Done\n";

        // keep cube VB alive for skybox rendering
        ibl.skyboxVB      = cubeVB;
        ibl.skyboxVBAlloc = cubeAlloc;

        vkDestroyPipeline(device, equirectPipeline.handle,   nullptr);
        vkDestroyPipeline(device, irradiancePipeline.handle, nullptr);
        vkDestroyPipeline(device, prefilterPipeline.handle,  nullptr);
        vkDestroyPipeline(device, brdfPipeline.handle,       nullptr);
        vkDestroyPipelineLayout(device, equirectPipeline.layout,   nullptr);
        vkDestroyPipelineLayout(device, irradiancePipeline.layout, nullptr);
        vkDestroyPipelineLayout(device, prefilterPipeline.layout,  nullptr);
        vkDestroyPipelineLayout(device, brdfPipeline.layout,       nullptr);
        vkDestroyRenderPass(device, cubeRP, nullptr);
        vkDestroyRenderPass(device, brdfRP, nullptr);
        vkDestroyDescriptorSetLayout(device, dsLayout, nullptr);
        vkDestroyDescriptorPool(device, dsPool, nullptr);
        vkDestroySampler(device, hdrSampler, nullptr);
        vkDestroyImageView(device, hdrView, nullptr);
        vmaDestroyImage(VulkanDevice::GetAllocator(), hdrImage, hdrAlloc);

        return true;
    }

    static const char* FACE_NAMES_SKYBOX[6]     = { "px.hdr","nx.hdr","ny.hdr","py.hdr","pz.hdr","nz.hdr" };
    static const char* FACE_NAMES_STANDARD[6]   = { "px.hdr","nx.hdr","py.hdr","ny.hdr","pz.hdr","nz.hdr" };

    static bool LoadAndUploadFaces(const char* dir, CubemapData& out,
                                   const char* const faceNames[6], bool flipY)
    {
        int w = 0, h = 0, ch = 0;
        float* faces[6] = {};

        stbi_set_flip_vertically_on_load(flipY ? 1 : 0);
        for (int f = 0; f < 6; f++)
        {
            std::string path = std::string(dir) + "/" + faceNames[f];
            faces[f] = stbi_loadf(path.c_str(), &w, &h, &ch, STBI_rgb_alpha);
            if (!faces[f])
            {
                std::cout << "[IBL] Failed to load face: " << path << " (" << stbi_failure_reason() << ")\n";
                for (int j = 0; j < f; j++) stbi_image_free(faces[j]);
                stbi_set_flip_vertically_on_load(0);
                return false;
            }
        }
        stbi_set_flip_vertically_on_load(0);

        if (!CreateCubemapImage(out, (uint32_t)w, 1, VK_FORMAT_R32G32B32A32_SFLOAT,
                                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT))
        {
            for (int f = 0; f < 6; f++) stbi_image_free(faces[f]);
            return false;
        }

        VkDeviceSize faceBytes  = (VkDeviceSize)w * h * 4 * sizeof(float);
        VkDeviceSize totalBytes = faceBytes * 6;

        VkBufferCreateInfo stagingInfo{};
        stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingInfo.size  = totalBytes;
        stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

        VmaAllocationCreateInfo stagingAI{};
        stagingAI.usage = VMA_MEMORY_USAGE_CPU_ONLY;

        VkBuffer      stagingBuf;
        VmaAllocation stagingAlloc;
        vmaCreateBuffer(VulkanDevice::GetAllocator(), &stagingInfo, &stagingAI, &stagingBuf, &stagingAlloc, nullptr);

        void* mapped;
        vmaMapMemory(VulkanDevice::GetAllocator(), stagingAlloc, &mapped);
        for (int f = 0; f < 6; f++)
            memcpy((char*)mapped + faceBytes * f, faces[f], (size_t)faceBytes);
        vmaUnmapMemory(VulkanDevice::GetAllocator(), stagingAlloc);

        for (int f = 0; f < 6; f++) stbi_image_free(faces[f]);

        VkCommandBuffer cmd = MemoryManager::BeginOneTimeCommands();

        MemoryManager::TransitionImageLayout(cmd, out.image,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            0, 1, 0, 6);

        VkBufferImageCopy regions[6]{};
        for (int f = 0; f < 6; f++)
        {
            regions[f].bufferOffset                    = faceBytes * f;
            regions[f].imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            regions[f].imageSubresource.mipLevel       = 0;
            regions[f].imageSubresource.baseArrayLayer = f;
            regions[f].imageSubresource.layerCount     = 1;
            regions[f].imageExtent                     = { (uint32_t)w, (uint32_t)h, 1 };
        }
        vkCmdCopyBufferToImage(cmd, stagingBuf, out.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, regions);

        MemoryManager::TransitionImageLayout(cmd, out.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            0, 1, 0, 6);

        MemoryManager::EndOneTimeCommands(cmd);
        vmaDestroyBuffer(VulkanDevice::GetAllocator(), stagingBuf, stagingAlloc);
        return true;
    }

    bool CreateFromFaces(IBLData& ibl, const char* skyboxDir, const char* irradianceDir)
    {
        VkDevice device = VulkanDevice::GetDevice();
        std::cout << "[IBL] Loading skybox faces from: " << skyboxDir << "\n";

        if (!LoadAndUploadFaces(skyboxDir, ibl.environment, FACE_NAMES_SKYBOX, true))
        {
            std::cout << "[IBL] Failed to load skybox faces\n";
            return false;
        }
        std::cout << "[IBL] Skybox faces loaded\n";

        std::cout << "[IBL] Loading irradiance faces from: " << irradianceDir << "\n";
        if (!LoadAndUploadFaces(irradianceDir, ibl.irradiance, FACE_NAMES_SKYBOX, false))
        {
            std::cout << "[IBL] Failed to load irradiance faces\n";
            return false;
        }
        std::cout << "[IBL] Irradiance faces loaded\n";

        if (!CreateCubemapImage(ibl.prefiltered, PREFILTER_SIZE, PREFILTER_MIPS, VK_FORMAT_R16G16B16A16_SFLOAT)) return false;
        if (!CreateBRDFLUTImage(ibl)) return false;

        VkBuffer      cubeVB;
        VmaAllocation cubeAlloc;
        {
            VkBufferCreateInfo bufInfo{};
            bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufInfo.size  = sizeof(CUBE_VERTS);
            bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            VmaAllocationCreateInfo ai{};
            ai.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
            ai.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
            VmaAllocationInfo info;
            vmaCreateBuffer(VulkanDevice::GetAllocator(), &bufInfo, &ai, &cubeVB, &cubeAlloc, &info);
            memcpy(info.pMappedData, CUBE_VERTS, sizeof(CUBE_VERTS));
        }
        ibl.skyboxVB      = cubeVB;
        ibl.skyboxVBAlloc = cubeAlloc;

        VkDescriptorPool dsPool;
        {
            VkDescriptorPoolSize poolSize{};
            poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            poolSize.descriptorCount = 2;
            VkDescriptorPoolCreateInfo poolInfo{};
            poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.poolSizeCount = 1;
            poolInfo.pPoolSizes    = &poolSize;
            poolInfo.maxSets       = 2;
            vkCreateDescriptorPool(device, &poolInfo, nullptr, &dsPool);
        }
        VkDescriptorSetLayout dsLayout = CreateSamplerLayout();

        VkRenderPass cubeRP = CreateOffscreenRenderPass(VK_FORMAT_R16G16B16A16_SFLOAT);
        VkRenderPass brdfRP = CreateOffscreenRenderPass(VK_FORMAT_R16G16_SFLOAT);

        Pipeline prefilterPipeline = CreateCubePipeline(cubeRP, dsLayout,
            "shaders/compiled/cubemap.vert.spv",
            "shaders/compiled/prefilter.frag.spv");
        Pipeline brdfPipeline = CreateBRDFPipeline(brdfRP);

        // --- prefilter from environment ---
        std::cout << "[IBL] Pass: prefilter\n";
        {
            VkDescriptorSet ds = AllocateInputSet(dsPool, dsLayout,
                ibl.environment.imageView, ibl.environment.sampler);
            for (uint32_t mip = 0; mip < PREFILTER_MIPS; mip++)
            {
                uint32_t mipSize   = PREFILTER_SIZE >> mip;
                float    roughness = static_cast<float>(mip) / static_cast<float>(PREFILTER_MIPS - 1);
                RenderCubeFaces(cubeRP, prefilterPipeline, ds, cubeVB, ibl.prefiltered, mipSize, mip, roughness);
            }
        }

        std::cout << "[IBL] Pass: BRDF LUT\n";
        RenderBRDFLUT(brdfRP, brdfPipeline, ibl);
        std::cout << "[IBL] Done\n";

        vkDestroyPipeline(device, prefilterPipeline.handle, nullptr);
        vkDestroyPipelineLayout(device, prefilterPipeline.layout, nullptr);
        vkDestroyPipeline(device, brdfPipeline.handle, nullptr);
        vkDestroyPipelineLayout(device, brdfPipeline.layout, nullptr);
        vkDestroyRenderPass(device, cubeRP, nullptr);
        vkDestroyRenderPass(device, brdfRP, nullptr);
        vkDestroyDescriptorSetLayout(device, dsLayout, nullptr);
        vkDestroyDescriptorPool(device, dsPool, nullptr);

        return true;
    }

    void Destroy(IBLData& ibl)
    {
        VkDevice device = VulkanDevice::GetDevice();
        DestroyCubemapData(ibl.environment);
        DestroyCubemapData(ibl.irradiance);
        DestroyCubemapData(ibl.prefiltered);
        if (ibl.brdfLUTSampler != VK_NULL_HANDLE) vkDestroySampler(device, ibl.brdfLUTSampler, nullptr);
        if (ibl.brdfLUTView    != VK_NULL_HANDLE) vkDestroyImageView(device, ibl.brdfLUTView, nullptr);
        if (ibl.brdfLUT        != VK_NULL_HANDLE) vmaDestroyImage(VulkanDevice::GetAllocator(), ibl.brdfLUT, ibl.brdfLUTAlloc);
        if (ibl.skyboxVB       != VK_NULL_HANDLE) vmaDestroyBuffer(VulkanDevice::GetAllocator(), ibl.skyboxVB, ibl.skyboxVBAlloc);
        ibl = {};
    }
}
