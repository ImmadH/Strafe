#include "pipeline.h"
#include "device.h"
#include "swapchain.h"
#include "memory.h"
#include "mesh.h"
#include <vector>
#include <string>
#include <ios>
#include <fstream>

//shader modules, pipeline, renderpass, and frame buffers

namespace VulkanPipeline
{
	VkRenderPass renderPass = VK_NULL_HANDLE;
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipeline graphicsPipeline = VK_NULL_HANDLE;
	VkPipeline skyboxPipeline   = VK_NULL_HANDLE;
	std::vector<VkFramebuffer> swapChainFramebuffers;

	static VkSampleCountFlagBits MSAA_SAMPLES = VK_SAMPLE_COUNT_8_BIT;

	bool GetMSAAEnabled()        { return MSAA_SAMPLES == VK_SAMPLE_COUNT_8_BIT; }
	void SetMSAAEnabled(bool on) { MSAA_SAMPLES = on ? VK_SAMPLE_COUNT_8_BIT : VK_SAMPLE_COUNT_1_BIT; }

	static VkImage       depthImage      = VK_NULL_HANDLE;
	static VkImageView   depthImageView  = VK_NULL_HANDLE;
	static VmaAllocation depthAllocation = VK_NULL_HANDLE;

	static VkImage       msaaImage      = VK_NULL_HANDLE;
	static VkImageView   msaaImageView  = VK_NULL_HANDLE;
	static VmaAllocation msaaAllocation = VK_NULL_HANDLE;

	static bool CreateDepthResources()
	{
		VkExtent2D   extent    = VulkanSwapchain::GetSwapChainExtent();
		VmaAllocator allocator = VulkanDevice::GetAllocator();
		VkDevice     device    = VulkanDevice::GetDevice();

		VkImageCreateInfo imageInfo{};
		imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType     = VK_IMAGE_TYPE_2D;
		imageInfo.extent        = { extent.width, extent.height, 1 };
		imageInfo.mipLevels     = 1;
		imageInfo.arrayLayers   = 1;
		imageInfo.format        = VK_FORMAT_D32_SFLOAT;
		imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage         = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		imageInfo.samples       = MSAA_SAMPLES;
		imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

		if (vmaCreateImage(allocator, &imageInfo, &allocInfo, &depthImage, &depthAllocation, nullptr) != VK_SUCCESS)
			return false;

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image                           = depthImage;
		viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format                          = VK_FORMAT_D32_SFLOAT;
		viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
		viewInfo.subresourceRange.baseMipLevel   = 0;
		viewInfo.subresourceRange.levelCount     = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount     = 1;

		return vkCreateImageView(device, &viewInfo, nullptr, &depthImageView) == VK_SUCCESS;
	}

	static void DestroyDepthResources()
	{
		vkDestroyImageView(VulkanDevice::GetDevice(), depthImageView, nullptr);
		vmaDestroyImage(VulkanDevice::GetAllocator(), depthImage, depthAllocation);
		depthImageView  = VK_NULL_HANDLE;
		depthImage      = VK_NULL_HANDLE;
		depthAllocation = VK_NULL_HANDLE;
	}

	static bool CreateMSAAResources()
	{
		VkExtent2D   extent    = VulkanSwapchain::GetSwapChainExtent();
		VmaAllocator allocator = VulkanDevice::GetAllocator();
		VkDevice     device    = VulkanDevice::GetDevice();

		VkImageCreateInfo imageInfo{};
		imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType     = VK_IMAGE_TYPE_2D;
		imageInfo.extent        = { extent.width, extent.height, 1 };
		imageInfo.mipLevels     = 1;
		imageInfo.arrayLayers   = 1;
		imageInfo.format        = VulkanSwapchain::GetSwapChainImageFormat();
		imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		imageInfo.samples       = MSAA_SAMPLES;
		imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

		if (vmaCreateImage(allocator, &imageInfo, &allocInfo, &msaaImage, &msaaAllocation, nullptr) != VK_SUCCESS)
			return false;

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image                           = msaaImage;
		viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format                          = VulkanSwapchain::GetSwapChainImageFormat();
		viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel   = 0;
		viewInfo.subresourceRange.levelCount     = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount     = 1;

		return vkCreateImageView(device, &viewInfo, nullptr, &msaaImageView) == VK_SUCCESS;
	}

	static void DestroyMSAAResources()
	{
		vkDestroyImageView(VulkanDevice::GetDevice(), msaaImageView, nullptr);
		vmaDestroyImage(VulkanDevice::GetAllocator(), msaaImage, msaaAllocation);
		msaaImageView  = VK_NULL_HANDLE;
		msaaImage      = VK_NULL_HANDLE;
		msaaAllocation = VK_NULL_HANDLE;
	}

	static std::vector<char> ReadFile(const std::string& fileName)
	{
		std::ifstream file(fileName, std::ios::ate | std::ios::binary);

		if (!file.is_open())
		{
			throw std::runtime_error("Failed to open file");
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);
		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();

		return buffer;
	}

	VkShaderModule CreateShaderModule(const std::vector<char>& shaderCode)
	{
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = shaderCode.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

		VkShaderModule shaderModule;
		if (vkCreateShaderModule(VulkanDevice::GetDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create shader module");
		}

		return shaderModule;
	}

	bool CreateRenderPass()
	{
		bool msaa = MSAA_SAMPLES != VK_SAMPLE_COUNT_1_BIT;

		VkAttachmentDescription colorAttachment{};
		colorAttachment.format         = VulkanSwapchain::GetSwapChainImageFormat();
		colorAttachment.samples        = MSAA_SAMPLES;
		colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp        = msaa ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
		colorAttachment.finalLayout    = msaa ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentDescription depthAttachment{};
		depthAttachment.format         = VK_FORMAT_D32_SFLOAT;
		depthAttachment.samples        = MSAA_SAMPLES;
		depthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference colorRef{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		VkAttachmentReference depthRef{ 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount    = 1;
		subpass.pColorAttachments       = &colorRef;
		subpass.pDepthStencilAttachment = &depthRef;

		VkSubpassDependency dependency{};
		dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass    = 0;
		dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo renderPassInfo{};
		renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.subpassCount    = 1;
		renderPassInfo.pSubpasses      = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies   = &dependency;

		if (msaa)
		{
			VkAttachmentDescription resolveAttachment{};
			resolveAttachment.format         = VulkanSwapchain::GetSwapChainImageFormat();
			resolveAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
			resolveAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			resolveAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
			resolveAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			resolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			resolveAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
			resolveAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			VkAttachmentReference resolveRef{ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
			subpass.pResolveAttachments = &resolveRef;

			VkAttachmentDescription attachments[] = { colorAttachment, depthAttachment, resolveAttachment };
			renderPassInfo.attachmentCount = 3;
			renderPassInfo.pAttachments    = attachments;

			if (vkCreateRenderPass(VulkanDevice::GetDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
				throw std::runtime_error("Failed to create render pass");
		}
		else
		{
			VkAttachmentDescription attachments[] = { colorAttachment, depthAttachment };
			renderPassInfo.attachmentCount = 2;
			renderPassInfo.pAttachments    = attachments;

			if (vkCreateRenderPass(VulkanDevice::GetDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
				throw std::runtime_error("Failed to create render pass");
		}

		return true;
	}

	bool CreateFramebuffers()
	{
		std::vector<VkImageView> swapChainImageViews = VulkanSwapchain::GetSwapChainImageViews();
		VkExtent2D swapChainExtent = VulkanSwapchain::GetSwapChainExtent();

		swapChainFramebuffers.resize(swapChainImageViews.size());

		bool msaa = MSAA_SAMPLES != VK_SAMPLE_COUNT_1_BIT;

		for (size_t i = 0; i < swapChainImageViews.size(); i++)
		{
			VkImageView msaaAttachments[]   = { msaaImageView, depthImageView, swapChainImageViews[i] };
			VkImageView plainAttachments[]  = { swapChainImageViews[i], depthImageView };

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass      = renderPass;
			framebufferInfo.attachmentCount = msaa ? 3 : 2;
			framebufferInfo.pAttachments    = msaa ? msaaAttachments : plainAttachments;
			framebufferInfo.width           = swapChainExtent.width;
			framebufferInfo.height          = swapChainExtent.height;
			framebufferInfo.layers          = 1;

			if (vkCreateFramebuffer(VulkanDevice::GetDevice(), &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
				throw std::runtime_error("Failed to create framebuffer");
		}

		return true;
	}

	bool Create()
	{
		CreateRenderPass();

		auto vertShaderCode = ReadFile("shaders/compiled/pbr.vert.spv");
		auto fragShaderCode = ReadFile("shaders/compiled/pbr.frag.spv");

		VkShaderModule vertShaderModule = CreateShaderModule(vertShaderCode);
		VkShaderModule fragShaderModule = CreateShaderModule(fragShaderCode);

		VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

		auto binding    = Mesh::GetBindingDescription();
		auto attributes = Mesh::GetAttributeDescriptions();

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount   = 1;
		vertexInputInfo.pVertexBindingDescriptions      = &binding;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
		vertexInputInfo.pVertexAttributeDescriptions    = attributes.data();

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;

		VkPipelineRasterizationStateCreateInfo rasterizer{};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;
		rasterizer.rasterizerDiscardEnable = VK_FALSE;
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizer.lineWidth = 1.0f;
		rasterizer.cullMode = VK_CULL_MODE_NONE;
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizer.depthBiasEnable = VK_FALSE;

		VkPipelineMultisampleStateCreateInfo multisampling{};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = MSAA_SAMPLES;

		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable       = VK_TRUE;
		depthStencil.depthWriteEnable      = VK_TRUE;
		depthStencil.depthCompareOp        = VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.stencilTestEnable     = VK_FALSE;

		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.colorWriteMask =
			VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
			VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo colorBlending{};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;

		std::vector<VkDynamicState> dynamicStates = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};

		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
		dynamicState.pDynamicStates = dynamicStates.data();

		VkDescriptorSetLayout setLayouts[] = {
			MemoryManager::GetCameraDescriptorSetLayout(),
			MemoryManager::GetTextureDescriptorSetLayout(),
			MemoryManager::GetIBLDescriptorSetLayout()
		};

		VkPushConstantRange pushConstant{};
		pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		pushConstant.offset     = 0;
		pushConstant.size       = sizeof(float) * 16 + sizeof(float) * 4 + sizeof(float) * 2 + sizeof(int) * 3; // mat4 + vec4 lightDir + metallic/roughness + normal/ao/ibl toggles

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
		pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount         = 3;
		pipelineLayoutInfo.pSetLayouts            = setLayouts;
		pipelineLayoutInfo.pushConstantRangeCount = 1;
		pipelineLayoutInfo.pPushConstantRanges    = &pushConstant;

		if (vkCreatePipelineLayout(VulkanDevice::GetDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create pipeline layout");
		}

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pColorBlendState    = &colorBlending;
		pipelineInfo.pDepthStencilState  = &depthStencil;
		pipelineInfo.pDynamicState       = &dynamicState;
		pipelineInfo.layout = pipelineLayout;
		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = 0;
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

		if (vkCreateGraphicsPipelines(VulkanDevice::GetDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create graphics pipeline");
		}

		vkDestroyShaderModule(VulkanDevice::GetDevice(), fragShaderModule, nullptr);
		vkDestroyShaderModule(VulkanDevice::GetDevice(), vertShaderModule, nullptr);

		if (MSAA_SAMPLES != VK_SAMPLE_COUNT_1_BIT)
		{
			if (!CreateMSAAResources())
				throw std::runtime_error("Failed to create MSAA resources");
		}

		if (!CreateDepthResources())
			throw std::runtime_error("Failed to create depth resources");

		CreateFramebuffers();

		// --- skybox pipeline (same layout, depth LESS_OR_EQUAL, no depth write) ---
		{
			auto skyboxVert = ReadFile("shaders/compiled/skybox.vert.spv");
			auto skyboxFrag = ReadFile("shaders/compiled/skybox.frag.spv");
			VkShaderModule skyboxVertMod = CreateShaderModule(skyboxVert);
			VkShaderModule skyboxFragMod = CreateShaderModule(skyboxFrag);

			VkPipelineShaderStageCreateInfo skyboxStages[2]{};
			skyboxStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			skyboxStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
			skyboxStages[0].module = skyboxVertMod;
			skyboxStages[0].pName  = "main";
			skyboxStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			skyboxStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
			skyboxStages[1].module = skyboxFragMod;
			skyboxStages[1].pName  = "main";

			// position-only vertex input (stride = 12 bytes)
			VkVertexInputBindingDescription skyboxBinding{};
			skyboxBinding.binding   = 0;
			skyboxBinding.stride    = sizeof(float) * 3;
			skyboxBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			VkVertexInputAttributeDescription skyboxAttr{};
			skyboxAttr.binding  = 0;
			skyboxAttr.location = 0;
			skyboxAttr.format   = VK_FORMAT_R32G32B32_SFLOAT;
			skyboxAttr.offset   = 0;

			VkPipelineVertexInputStateCreateInfo skyboxVI{};
			skyboxVI.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			skyboxVI.vertexBindingDescriptionCount   = 1;
			skyboxVI.pVertexBindingDescriptions      = &skyboxBinding;
			skyboxVI.vertexAttributeDescriptionCount = 1;
			skyboxVI.pVertexAttributeDescriptions    = &skyboxAttr;

			VkPipelineDepthStencilStateCreateInfo skyboxDepth{};
			skyboxDepth.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
			skyboxDepth.depthTestEnable  = VK_TRUE;
			skyboxDepth.depthWriteEnable = VK_FALSE;          // skybox never writes depth
			skyboxDepth.depthCompareOp   = VK_COMPARE_OP_LESS_OR_EQUAL;

			VkGraphicsPipelineCreateInfo skyboxInfo = pipelineInfo;
			skyboxInfo.pStages            = skyboxStages;
			skyboxInfo.pVertexInputState  = &skyboxVI;
			skyboxInfo.pDepthStencilState = &skyboxDepth;

			if (vkCreateGraphicsPipelines(VulkanDevice::GetDevice(), VK_NULL_HANDLE, 1, &skyboxInfo, nullptr, &skyboxPipeline) != VK_SUCCESS)
				throw std::runtime_error("Failed to create skybox pipeline");

			vkDestroyShaderModule(VulkanDevice::GetDevice(), skyboxVertMod, nullptr);
			vkDestroyShaderModule(VulkanDevice::GetDevice(), skyboxFragMod, nullptr);
		}

		return true;
	}

	void RecreateFramebuffers()
	{
		VkDevice device = VulkanDevice::GetDevice();
		for (VkFramebuffer fb : swapChainFramebuffers)
			vkDestroyFramebuffer(device, fb, nullptr);
		swapChainFramebuffers.clear();
		if (msaaImage != VK_NULL_HANDLE) DestroyMSAAResources();
		DestroyDepthResources();
		if (MSAA_SAMPLES != VK_SAMPLE_COUNT_1_BIT) CreateMSAAResources();
		CreateDepthResources();
		CreateFramebuffers();
	}

	VkRenderPass GetRenderPass()
	{
		return renderPass;
	}

	VkPipeline GetGraphicsPipeline()
	{
		return graphicsPipeline;
	}

	VkPipeline GetSkyboxPipeline()
	{
		return skyboxPipeline;
	}

	std::vector<VkFramebuffer> GetFramebuffers()
	{
		return swapChainFramebuffers;
	}

	VkPipelineLayout GetPipelineLayout()
	{
		return pipelineLayout;
	}

	void Destroy()
	{
		const VkDevice device = VulkanDevice::GetDevice();

		for (VkFramebuffer framebuffer : swapChainFramebuffers)
		{
			vkDestroyFramebuffer(device, framebuffer, nullptr);
		}
		swapChainFramebuffers.clear();

		if (msaaImage != VK_NULL_HANDLE) DestroyMSAAResources();
		DestroyDepthResources();
		vkDestroyPipeline(device, skyboxPipeline,   nullptr);
		vkDestroyPipeline(device, graphicsPipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyRenderPass(device, renderPass, nullptr);
	}
}
