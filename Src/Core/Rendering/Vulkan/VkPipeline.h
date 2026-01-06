#pragma once
#include "BackendDefines.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Defines/VertexDefines.h"
#include "Core/Rendering/Core/Pipeline.h"
#include "Core/Rendering/Core/Resource.h"
#include "Core/Rendering/Vulkan/VkDescriptorSetLayout.h"

struct PipeVertInfo
{
    VkVertexInputBindingDescription m_vertexInputDescription{};
    stltype::vector<VkVertexInputAttributeDescription> m_attributeDescriptions{};
    u32 bindingDescriptionCount{1};
};

struct ShaderCollection
{
    Shader* pVertShader;
    Shader* pFragShader;
    Shader* pComputeShader;
};

// Base class that holds shared members and helpers for Vulkan pipelines
class PipelineVulkanBase : public TrackedResource
{
protected:
    VkPipelineLayout CreatePipelineLayout(const DescriptorSetLayoutInfo& layoutInfo,
                                          const PushConstantInfo& pushConstantInfo);

    // Fills out the common components used by graphics pipeline creation. Callers provide output variables by
    // reference.
    void PrepareGraphicsBase(const ShaderCollection& shaders,
                             const PipeVertInfo& vertexInputs,
                             const PipelineInfo& pipeInfo,
                             stltype::vector<VkPipelineShaderStageCreateInfo>& outShaderStages,
                             VkPipelineDynamicStateCreateInfo& outDynamicState,
                             VkPipelineVertexInputStateCreateInfo& outVertexInput,
                             VkPipelineInputAssemblyStateCreateInfo& outInputAssembly,
                             VkPipelineViewportStateCreateInfo& outViewport,
                             VkPipelineRasterizationStateCreateInfo& outRasterizer,
                             VkPipelineMultisampleStateCreateInfo& outMultisampling,
                             VkPipelineColorBlendStateCreateInfo& outColorBlending,
                             VkPipelineRenderingCreateInfo& outPipelineRenderingCreateInfo);

    // Helper creation methods used by PrepareGraphicsBase
    VkPipelineDynamicStateCreateInfo CreateDynamicPipelineInfo(const stltype::vector<VkDynamicState>& dynamicStates);

    VkPipelineVertexInputStateCreateInfo CreateVertexInputInfo(const PipeVertInfo& vertexInputs);
    VkPipelineVertexInputStateCreateInfo CreateEmptyVertexInputInfo();

    VkPipelineInputAssemblyStateCreateInfo CreateInputAssemblyInfo(const Topology& topology);

    VkPipelineViewportStateCreateInfo CreateViewportInfo(const DirectX::XMFLOAT4& viewportExtents);

    VkPipelineRasterizationStateCreateInfo CreateRasterizerInfo(const DirectX::XMFLOAT4& viewportExtents,
                                                                const RasterizerInfo& info);

    VkPipelineMultisampleStateCreateInfo CreateMultisampleInfo(const MultisampleInfo& info);

    VkPipelineColorBlendAttachmentState CreateColorBlendAttachmentInfo(const ColorBlendAttachmentInfo& info);

    VkPipelineColorBlendStateCreateInfo CreateColorBlendInfo(
        const ColorBlendInfo& info, const stltype::vector<VkPipelineColorBlendAttachmentState>& colorBlendAttachments);

protected:
    VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};

    stltype::vector<DescriptorSetLayout> m_sharedDescriptorSetLayouts;
    DescriptorSetLayout m_descriptorSetLayout;

    PipelineInfo m_info;
    PipeVertInfo m_vertexInfo;
};

class ComputePipelineVulkan : public PipelineVulkanBase
{
public:
    ComputePipelineVulkan(const ShaderCollection& shaders, const PipelineInfo& pipeInfo);
    ComputePipelineVulkan() = default;
    ~ComputePipelineVulkan();

    void CleanUp();

    VkPipeline GetRef() const
    {
        return m_pipeline;
    }

private:
    VkPipeline m_pipeline{VK_NULL_HANDLE};
    VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
};

// Implementation of a dynamic pipeline state object
class GraphicsPipelineVulkan : public PipelineVulkanBase
{
public:
    GraphicsPipelineVulkan(const ShaderCollection& shaders,
                           const PipeVertInfo& vertexInputs,
                           const PipelineInfo& pipeInfo);

    GraphicsPipelineVulkan() = default;
    ~GraphicsPipelineVulkan();

    void CleanUp();

    bool HasDynamicViewScissorState() const;
    bool NeedsVertexBuffers() const;

    VkPipeline GetRef() const;
    VkPipelineLayout GetLayout() const
    {
        return m_pipelineLayout;
    }
    const VkDescriptorSetLayout& GetPipelineSpecificLayout() const
    {
        return m_descriptorSetLayout.GetRef();
    }

private:
    VkPipelineDepthStencilStateCreateInfo CreateDepthStencilLayout();

    VkPipeline m_pipeline{VK_NULL_HANDLE};
};
