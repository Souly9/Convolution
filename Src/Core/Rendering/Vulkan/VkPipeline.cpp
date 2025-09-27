#include "VkPipeline.h"
#include "VkGlobals.h"
#include "VkTextureManager.h"
#include "Utils/VkEnumHelpers.h"
#include "Utils/DescriptorSetLayoutConverters.h"


PipelineVulkan::PipelineVulkan(const ShaderCollection& shaders, const PipeVertInfo& vertexInputs, const PipelineInfo& pipeInfo)
{
	// Assume we always have a vert and fragment shader here for now
	Shader& vertShader = *shaders.pVertShader;
	Shader& fragShader = *shaders.pFragShader;
	if (vertShader.GetDesc() == VK_NULL_HANDLE || fragShader.GetDesc() == VK_NULL_HANDLE)
		g_pFileReader->FinishAllRequests();
	DEBUG_ASSERT(vertShader.GetDesc() != VK_NULL_HANDLE && fragShader.GetDesc() != VK_NULL_HANDLE);

	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShader.GetDesc();
	vertShaderStageInfo.pName = vertShader.GetName().c_str();

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShader.GetDesc();
	fragShaderStageInfo.pName = fragShader.GetName().c_str();

	const auto shaderStages = stltype::vector{ vertShaderStageInfo, fragShaderStageInfo };
	const auto attachmentCount = pipeInfo.attachmentInfos.colorAttachments.size();

	const auto dymState = CreateDynamicPipelineInfo(g_dynamicStates);
	const auto vertexInput = CreateVertexInputInfo(vertexInputs);
	const auto inputAssembly = CreateInputAssemblyInfo(pipeInfo.topology);
	const auto viewport = CreateViewportInfo(pipeInfo.viewPortExtents);
	const auto rasterizer = CreateRasterizerInfo(pipeInfo.viewPortExtents, pipeInfo.rasterizerInfo);
	const auto multisampling = CreateMultisampleInfo(pipeInfo.multisampleInfo);
	const auto colorAttachmentInfo = CreateColorBlendAttachmentInfo(pipeInfo.colorBlendInfo);
	stltype::vector<VkPipelineColorBlendAttachmentState> blendAttachments;
	blendAttachments.assign(attachmentCount, colorAttachmentInfo);
	auto colorBlending = CreateColorBlendInfo(pipeInfo.colorInfo, blendAttachments);
	m_pipelineLayout = CreatePipelineLayout(pipeInfo.descriptorSetLayout);

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = shaderStages.size();
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInput;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewport;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dymState;
	pipelineInfo.layout = m_pipelineLayout;
	pipelineInfo.subpass = 0;
	pipelineInfo.renderPass = VK_NULL_HANDLE;

			// Set attachment infos for dynamic rendering
	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
	{
		pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		pipelineRenderingCreateInfo.colorAttachmentCount = pipeInfo.attachmentInfos.colorAttachments.size();;
		pipelineRenderingCreateInfo.pColorAttachmentFormats = pipeInfo.attachmentInfos.colorAttachments.data(); 
		pipelineRenderingCreateInfo.depthAttachmentFormat = pipeInfo.attachmentInfos.depthAttachmentFormat;
	}

	pipelineInfo.pNext = &pipelineRenderingCreateInfo;
	if (pipeInfo.hasDepth)
	{
		const auto depthStencil = CreateDepthStencilLayout();
		pipelineInfo.pDepthStencilState = &depthStencil;
		DEBUG_ASSERT(vkCreateGraphicsPipelines(VkGlobals::GetLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, VulkanAllocator(), &m_pipeline) == VK_SUCCESS);
	}
	else
	{
		DEBUG_ASSERT(vkCreateGraphicsPipelines(VkGlobals::GetLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, VulkanAllocator(), &m_pipeline) == VK_SUCCESS);
	}
}

PipelineVulkan::~PipelineVulkan()
{
	TRACKED_DESC_IMPL
}

void PipelineVulkan::CleanUp()
{
	VK_FREE_IF(m_pipelineLayout, vkDestroyPipelineLayout(VK_LOGICAL_DEVICE, m_pipelineLayout, VulkanAllocator()))
	VK_FREE_IF(m_pipeline, vkDestroyPipeline(VK_LOGICAL_DEVICE, m_pipeline, VulkanAllocator()))
}

bool PipelineVulkan::HasDynamicViewScissorState() const
{
	return m_info.dynamicViewScissor;
}

bool PipelineVulkan::NeedsVertexBuffers() const
{
	return m_vertexInfo.bindingDescriptionCount > 0;
}

VkPipeline PipelineVulkan::GetRef() const
{
	return m_pipeline;
}

VkPipelineDynamicStateCreateInfo PipelineVulkan::CreateDynamicPipelineInfo(const stltype::vector<VkDynamicState>& dynamicStates)
{
	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	return dynamicState;
}

VkPipelineVertexInputStateCreateInfo PipelineVulkan::CreateVertexInputInfo(const PipeVertInfo& vertexInputs)
{
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = vertexInputs.bindingDescriptionCount;
	vertexInputInfo.pVertexBindingDescriptions = &vertexInputs.m_vertexInputDescription; 
	vertexInputInfo.vertexAttributeDescriptionCount = vertexInputs.m_attributeDescriptions.size();
	vertexInputInfo.pVertexAttributeDescriptions = vertexInputs.m_attributeDescriptions.data(); 

	m_vertexInfo = vertexInputs;

	return vertexInputInfo;
}

VkPipelineVertexInputStateCreateInfo PipelineVulkan::CreateEmptyVertexInputInfo()
{
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

	return vertexInputInfo;
}

VkPipelineInputAssemblyStateCreateInfo PipelineVulkan::CreateInputAssemblyInfo(const Topology& topology)
{
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = Conv(topology);
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	return inputAssembly;
}

VkPipelineViewportStateCreateInfo PipelineVulkan::CreateViewportInfo(const DirectX::XMFLOAT4& viewportExtents)
{
	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	return viewportState;
}

VkPipelineRasterizationStateCreateInfo PipelineVulkan::CreateRasterizerInfo(const DirectX::XMFLOAT4& viewportExtents, const RasterizerInfo& info)
{
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = info.enableDepthClamp;
	rasterizer.rasterizerDiscardEnable = info.discardEverythingBeforeRasterizer;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = info.lineWidth;
	rasterizer.cullMode = Conv(info.cullmode);
	rasterizer.frontFace = Conv(info.frontMode);
	rasterizer.depthBiasEnable = info.depthBias;
	rasterizer.depthBiasConstantFactor = info.depthBiasConstantFactor;
	rasterizer.depthBiasClamp = info.depthBiasClamp;
	rasterizer.depthBiasSlopeFactor = info.depthBiasSlopeFactor;

	return rasterizer;
}

VkPipelineMultisampleStateCreateInfo PipelineVulkan::CreateMultisampleInfo(const MultisampleInfo& info)
{
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = nullptr; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	return multisampling;
}

VkPipelineColorBlendAttachmentState PipelineVulkan::CreateColorBlendAttachmentInfo(const ColorBlendAttachmentInfo& info)
{
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;
	return colorBlendAttachment;
}

VkPipelineColorBlendStateCreateInfo PipelineVulkan::CreateColorBlendInfo(const ColorBlendInfo& info, const stltype::vector<VkPipelineColorBlendAttachmentState>& colorBlendAttachments)
{
	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = colorBlendAttachments.size();
	colorBlending.pAttachments = colorBlendAttachments.data();
	colorBlending.blendConstants[0] = 0.0f; // Optional
	colorBlending.blendConstants[1] = 0.0f; // Optional
	colorBlending.blendConstants[2] = 0.0f; // Optional
	colorBlending.blendConstants[3] = 0.0f; // Optional
	
	return colorBlending;
}

VkPipelineDepthStencilStateCreateInfo PipelineVulkan::CreateDepthStencilLayout()
{
	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencil.depthTestEnable = VK_TRUE;
	depthStencil.depthWriteEnable = VK_TRUE;
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f; // Optional
	depthStencil.maxDepthBounds = 1.0f; // Optional
	depthStencil.stencilTestEnable = VK_FALSE;
	depthStencil.front = {}; // Optional
	depthStencil.back = {}; // Optional
	return depthStencil;
}

VkPipelineLayout PipelineVulkan::CreatePipelineLayout(const DescriptorSetLayoutInfo& layoutInfo)
{
	m_sharedDescriptorSetLayouts = DescriptorLaytoutUtils::CreateOneDescriptorSetLayoutPerSet(layoutInfo.sharedDescriptors);

	m_descriptorSetLayout = DescriptorLaytoutUtils::CreateOneDescriptorSetForAll(layoutInfo.pipelineSpecificDescriptors);

	stltype::vector<VkDescriptorSetLayout> descriptorSetLayouts;
	descriptorSetLayouts.reserve(m_sharedDescriptorSetLayouts.size());
	for (const auto& layout : m_sharedDescriptorSetLayouts)
	{
		descriptorSetLayouts.emplace_back(layout.GetRef());
	}
	descriptorSetLayouts.emplace_back(m_descriptorSetLayout.GetRef());

	VkPipelineLayout pipelineLayout;
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = descriptorSetLayouts.size();
	pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
	pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional
	
	DEBUG_ASSERT(vkCreatePipelineLayout(VkGlobals::GetLogicalDevice(), &pipelineLayoutInfo, VulkanAllocator(), &pipelineLayout) == VK_SUCCESS);
	return pipelineLayout;
}
