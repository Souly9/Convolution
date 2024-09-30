#pragma once
#include "Core/Global/GlobalDefines.h"
#include "BackendDefines.h"
#include "Core/Rendering/Core/Pipeline.h"
#include "Core/Rendering/Core/Resource.h"
#include "Core/Rendering/Core/VertexDefines.h"

struct PipeVertInfo
{
	VkVertexInputBindingDescription vertexInputDescription{};
	stltype::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
	u32 bindingDescriptionCount{ 1 };
};

// Implementation of a dynamic pipeline state object 
class PipelineVulkan : public TrackedResource
{
public:
	PipelineVulkan(const ShaderVulkan& vertShader, const ShaderVulkan& fragShader,
		const PipeVertInfo& vertexInputs, const PipelineInfo& pipeInfo, const RenderPassVulkan& renderPass);

	PipelineVulkan() = default;
	~PipelineVulkan();

	void CleanUp();

	bool HasDynamicViewScissorState() const;
	bool NeedsVertexBuffers() const;

	VkPipeline GetRef();
private:

	VkPipelineDynamicStateCreateInfo CreateDynamicPipelineInfo(const stltype::vector<VkDynamicState>& dynamicStates);

	VkPipelineVertexInputStateCreateInfo CreateVertexInputInfo(const PipeVertInfo& vertexInputs);
	VkPipelineVertexInputStateCreateInfo CreateEmptyVertexInputInfo();

	VkPipelineInputAssemblyStateCreateInfo CreateInputAssemblyInfo(const Topology& topology);

	VkPipelineViewportStateCreateInfo CreateViewportInfo(const DirectX::XMFLOAT4& viewportExtents);

	VkPipelineRasterizationStateCreateInfo CreateRasterizerInfo(const DirectX::XMFLOAT4& viewportExtents, const RasterizerInfo& info);

	VkPipelineMultisampleStateCreateInfo CreateMultisampleInfo(const MultisampleInfo& info);

	VkPipelineColorBlendAttachmentState CreateColorBlendAttachmentInfo(const ColorBlendAttachmentInfo& info);

	VkPipelineColorBlendStateCreateInfo CreateColorBlendInfo(const ColorBlendInfo& info, VkPipelineColorBlendAttachmentState colorBlendAttachment);

	VkPipelineLayout CreatePipelineLayout(const PipelineUniformLayout& layoutInfo);

	VkPipelineLayout m_pipelineLayout;

	VkPipeline m_pipeline;

	PipelineInfo m_info;

	PipeVertInfo m_vertexInfo;
};
