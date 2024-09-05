#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Pipeline.h"
#include "BackendDefines.h"
#include "VkAttachment.h"
#include "VkRenderPass.h"
#include "VkShader.h"

// Implementation of a dynamic pipeline state object 
template<>
class Pipeline<Vulkan>
{
public:
	static Pipeline<Vulkan> CreatePipeline(const ShaderImpl<Vulkan>& vertShader, const ShaderImpl<Vulkan>& fragShader,
		const PipelineVertexInputs& vertexInputs, const PipelineInfo& pipeInfo, const RPass<Vulkan>& renderPass);

	~Pipeline();

private:
	Pipeline();

	void SetShaderStages(stltype::vector<VkPipelineShaderStageCreateInfo>&& shaderStages);

	VkPipelineDynamicStateCreateInfo CreateDynamicPipelineInfo(const stltype::vector<VkDynamicState>& dynamicStates);

	VkPipelineVertexInputStateCreateInfo CreateVertexInputInfo(const PipelineVertexInputs& vertexInputs);
	VkPipelineVertexInputStateCreateInfo CreateEmptyVertexInputInfo();

	VkPipelineInputAssemblyStateCreateInfo CreateInputAssemblyInfo(const Topology& topology);

	VkPipelineViewportStateCreateInfo CreateViewportInfo(const DirectX::XMFLOAT4& viewportExtents);

	VkPipelineRasterizationStateCreateInfo CreateRasterizerInfo(const DirectX::XMFLOAT4& viewportExtents, const RasterizerInfo& info);

	VkPipelineMultisampleStateCreateInfo CreateMultisampleInfo(const MultisampleInfo& info);

	VkPipelineColorBlendAttachmentState CreateColorBlendAttachmentInfo(const ColorBlendAttachmentInfo& info);

	VkPipelineColorBlendStateCreateInfo CreateColorBlendInfo(const ColorBlendInfo& info, VkPipelineColorBlendAttachmentState colorBlendAttachment);

	VkPipelineLayout CreatePipelineLayout(const PipelineUniformLayout& layoutInfo);
	
	stltype::vector<VkPipelineShaderStageCreateInfo> m_shaderStages;

	VkPipelineLayout m_pipelineLayout;

	VkPipeline m_pipeline;
};
