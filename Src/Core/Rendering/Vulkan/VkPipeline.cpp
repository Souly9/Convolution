#include "VkPipeline.h"
#include "VkGlobals.h"
#include "VkTextureManager.h"
#include "VkEnumHelpers.h"


PipelineVulkan::PipelineVulkan(const ShaderVulkan& vertShader, const ShaderVulkan& fragShader,
	const PipeVertInfo& vertexInputs, const PipelineInfo& pipeInfo, const RenderPassVulkan& renderPass)
{
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
	
	const auto dymState = CreateDynamicPipelineInfo(g_dynamicStates);
	const auto vertexInput = CreateVertexInputInfo(vertexInputs);
	const auto inputAssembly = CreateInputAssemblyInfo(pipeInfo.topology);
	const auto viewport = CreateViewportInfo(pipeInfo.viewPortExtents);
	const auto rasterizer = CreateRasterizerInfo(pipeInfo.viewPortExtents, pipeInfo.rasterizerInfo);
	const auto multisampling = CreateMultisampleInfo(pipeInfo.multisampleInfo);
	const auto colorBlendAttachment = CreateColorBlendAttachmentInfo(pipeInfo.colorBlendInfo);
	const auto colorBlending = CreateColorBlendInfo(pipeInfo.colorInfo, colorBlendAttachment);
	m_pipelineLayout = CreatePipelineLayout(pipeInfo.uniformLayout);

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = shaderStages.size();
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInput;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewport;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr; // Optional
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dymState;
	pipelineInfo.layout = m_pipelineLayout;
	pipelineInfo.renderPass = renderPass.GetRef();
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional

	DEBUG_ASSERT(vkCreateGraphicsPipelines(VkGlobals::GetLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, VulkanAllocator(), &m_pipeline) == VK_SUCCESS);
}

PipelineVulkan::~PipelineVulkan()
{
	TRACKED_DESC_IMPL
}

void PipelineVulkan::CleanUp()
{
	VK_FREE_IF(m_pipelineLayout, vkDestroyPipelineLayout(VkGlobals::GetLogicalDevice(), m_pipelineLayout, VulkanAllocator()))
	VK_FREE_IF(m_pipeline, vkDestroyPipeline(VkGlobals::GetLogicalDevice(), m_pipeline, VulkanAllocator()))
}

bool PipelineVulkan::HasDynamicViewScissorState() const
{
	return m_info.dynamicViewScissor;
}

bool PipelineVulkan::NeedsVertexBuffers() const
{
	return m_vertexInfo.bindingDescriptionCount > 0;
}

VkPipeline PipelineVulkan::GetRef()
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
	vertexInputInfo.pVertexBindingDescriptions = &vertexInputs.vertexInputDescription; 
	vertexInputInfo.vertexAttributeDescriptionCount = vertexInputs.attributeDescriptions.size();
	vertexInputInfo.pVertexAttributeDescriptions = vertexInputs.attributeDescriptions.data(); 

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

VkPipelineColorBlendStateCreateInfo PipelineVulkan::CreateColorBlendInfo(const ColorBlendInfo& info, VkPipelineColorBlendAttachmentState colorBlendAttachment)
{
	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f; // Optional
	colorBlending.blendConstants[1] = 0.0f; // Optional
	colorBlending.blendConstants[2] = 0.0f; // Optional
	colorBlending.blendConstants[3] = 0.0f; // Optional
	
	return colorBlending;
}

VkPipelineLayout PipelineVulkan::CreatePipelineLayout(const PipelineUniformLayout& layoutInfo)
{
	VkPipelineLayout pipelineLayout; 
	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 0; // Optional
	pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
	pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
	pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

	DEBUG_ASSERT(vkCreatePipelineLayout(VkGlobals::GetLogicalDevice(), &pipelineLayoutInfo, VulkanAllocator(), &pipelineLayout) == VK_SUCCESS);
	return pipelineLayout;
}
