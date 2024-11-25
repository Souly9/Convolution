#pragma once
#include "Core/Global/GlobalDefines.h"
#include "BackendDefines.h"
#include "Core/Rendering/Core/Pipeline.h"
#include "Core/Rendering/Core/Resource.h"
#include "Core/Rendering/Core/VertexDefines.h"
#include "Core/Rendering/Vulkan/VkDescriptorSetLayout.h"

struct PipeVertInfo
{
	VkVertexInputBindingDescription m_vertexInputDescription{};
	stltype::vector<VkVertexInputAttributeDescription> m_attributeDescriptions{};
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

	VkPipeline GetRef() const;
	VkPipelineLayout GetLayout() const { return m_pipelineLayout; }
	const VkDescriptorSetLayout& GetPipelineSpecificLayout() const { return m_descriptorSetLayout.GetRef(); }

private:

	VkPipelineDynamicStateCreateInfo CreateDynamicPipelineInfo(const stltype::vector<VkDynamicState>& dynamicStates);

	VkPipelineVertexInputStateCreateInfo CreateVertexInputInfo(const PipeVertInfo& vertexInputs);
	VkPipelineVertexInputStateCreateInfo CreateEmptyVertexInputInfo();

	VkPipelineInputAssemblyStateCreateInfo CreateInputAssemblyInfo(const Topology& topology);

	VkPipelineViewportStateCreateInfo CreateViewportInfo(const DirectX::XMFLOAT4& viewportExtents);

	VkPipelineRasterizationStateCreateInfo CreateRasterizerInfo(const DirectX::XMFLOAT4& viewportExtents, const RasterizerInfo& info);

	VkPipelineMultisampleStateCreateInfo CreateMultisampleInfo(const MultisampleInfo& info);

	VkPipelineColorBlendAttachmentState CreateColorBlendAttachmentInfo(const ColorBlendAttachmentInfo& info);

	VkPipelineColorBlendStateCreateInfo CreateColorBlendInfo(const ColorBlendInfo& info, const VkPipelineColorBlendAttachmentState& colorBlendAttachment);

	VkPipelineDepthStencilStateCreateInfo CreateDepthStencilLayout();
	VkPipelineLayout CreatePipelineLayout(const DescriptorSetLayoutInfo& layoutInfo);

	VkPipelineLayout m_pipelineLayout; 

	stltype::vector<DescriptorSetLayout> m_sharedDescriptorSetLayouts;
	DescriptorSetLayout m_descriptorSetLayout;

	VkPipeline m_pipeline;

	PipelineInfo m_info;

	PipeVertInfo m_vertexInfo;
};
