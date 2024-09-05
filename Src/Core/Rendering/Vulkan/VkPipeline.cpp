#include "VkGlobals.h"
#include "VkPipeline.h"

Pipeline<Vulkan> Pipeline<Vulkan>::CreatePipeline(const ShaderImpl<Vulkan>& vertShader, const ShaderImpl<Vulkan>& fragShader,
	const PipelineVertexInputs& vertexInputs, const PipelineInfo& pipeInfo, const RPass<Vulkan>& renderPass)
{
	VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = vertShader.GetHandle();
	vertShaderStageInfo.pName = vertShader.GetName().c_str();

	VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = fragShader.GetHandle();
	fragShaderStageInfo.pName = fragShader.GetName().c_str();

	Pipeline<Vulkan> pipeline{};
	pipeline.SetShaderStages(stltype::vector{ vertShaderStageInfo, fragShaderStageInfo });
	
	const auto dymState = pipeline.CreateDynamicPipelineInfo(g_dynamicStates);
	const auto vertexInput = pipeline.CreateEmptyVertexInputInfo();
	const auto inputAssembly = pipeline.CreateInputAssemblyInfo(pipeInfo.topology);
	const auto viewport = pipeline.CreateViewportInfo(pipeInfo.viewPortExtents);
	const auto rasterizer = pipeline.CreateRasterizerInfo(pipeInfo.viewPortExtents, pipeInfo.rasterizerInfo);
	const auto multisampling = pipeline.CreateMultisampleInfo(pipeInfo.multisampleInfo);
	const auto colorBlendAttachment = pipeline.CreateColorBlendAttachmentInfo(pipeInfo.colorBlendInfo);
	const auto colorBlending = pipeline.CreateColorBlendInfo(pipeInfo.colorInfo, colorBlendAttachment);
	pipeline.m_pipelineLayout = pipeline.CreatePipelineLayout(pipeInfo.uniformLayout);

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = pipeline.m_shaderStages.size();
	pipelineInfo.pStages = pipeline.m_shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInput;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewport;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr; // Optional
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dymState;
	pipelineInfo.layout = pipeline.m_pipelineLayout;
	pipelineInfo.renderPass = renderPass.GetHandle();
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional

	DEBUG_ASSERT(vkCreateGraphicsPipelines(VkGlobals::GetLogicalDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, VulkanAllocator(), &pipeline.m_pipeline) == VK_SUCCESS);

	return pipeline;
}

Pipeline<Vulkan>::~Pipeline()
{
	vkDestroyPipelineLayout(VkGlobals::GetLogicalDevice(), m_pipelineLayout, VulkanAllocator());
}

void Pipeline<Vulkan>::SetShaderStages(stltype::vector<VkPipelineShaderStageCreateInfo>&& shaderStages)
{
	m_shaderStages = stltype::move(shaderStages);
}

VkPipelineDynamicStateCreateInfo Pipeline<Vulkan>::CreateDynamicPipelineInfo(const stltype::vector<VkDynamicState>& dynamicStates)
{
	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	return dynamicState;
}

VkPipelineVertexInputStateCreateInfo Pipeline<Vulkan>::CreateEmptyVertexInputInfo()
{
	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

	return vertexInputInfo;
}

VkPipelineInputAssemblyStateCreateInfo Pipeline<Vulkan>::CreateInputAssemblyInfo(const Topology& topology)
{
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = Conv(topology);
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	return inputAssembly;
}

VkPipelineViewportStateCreateInfo Pipeline<Vulkan>::CreateViewportInfo(const DirectX::XMFLOAT4& viewportExtents)
{
	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	return viewportState;
}

VkPipelineRasterizationStateCreateInfo Pipeline<Vulkan>::CreateRasterizerInfo(const DirectX::XMFLOAT4& viewportExtents, const RasterizerInfo& info)
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

VkPipelineMultisampleStateCreateInfo Pipeline<Vulkan>::CreateMultisampleInfo(const MultisampleInfo& info)
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

VkPipelineColorBlendAttachmentState Pipeline<Vulkan>::CreateColorBlendAttachmentInfo(const ColorBlendAttachmentInfo& info)
{
	VkPipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.blendEnable = VK_TRUE;
	colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	return colorBlendAttachment;
}

VkPipelineColorBlendStateCreateInfo Pipeline<Vulkan>::CreateColorBlendInfo(const ColorBlendInfo& info, VkPipelineColorBlendAttachmentState colorBlendAttachment)
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

VkPipelineLayout Pipeline<Vulkan>::CreatePipelineLayout(const PipelineUniformLayout& layoutInfo)
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

Pipeline<Vulkan>::Pipeline()
{
}
