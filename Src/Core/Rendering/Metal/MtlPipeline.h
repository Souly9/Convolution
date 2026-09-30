#pragma once
#include "MtlBackendDefines.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Defines/VertexDefines.h"
#include "Core/Rendering/Core/Pipeline.h"
#include "Core/Rendering/Core/Resource.h"
#include "Core/Rendering/Metal/MtlDescriptorSetLayout.h"

struct PipeVertInfo
{
    VertexBindingDescription m_vertexInputDescription{};
    stltype::vector<VertexAttributeDescription> m_attributeDescriptions{};
    u32 bindingDescriptionCount{1};
};

// Viewport/scissor are always dynamic on Metal; raster/depth state lives partly on the encoder
class PipelineMetalBase : public PipelineBase
{
public:
    const PipelineInfo& GetInfo() const
    {
        return m_info;
    }

protected:
    stltype::vector<DescriptorSetLayout> m_sharedDescriptorSetLayouts{};
    DescriptorSetLayout m_descriptorSetLayout{};
    PipelineInfo m_info{};
    PipeVertInfo m_vertexInfo{};
};

class ComputePipelineMetal : public PipelineMetalBase
{
public:
    ComputePipelineMetal(const ShaderCollection& shaders, const PipelineInfo& pipeInfo);
    ComputePipelineMetal() = default;
    ~ComputePipelineMetal();

    virtual void CleanUp() override;

    MTL::ComputePipelineState* GetRef() const
    {
        return m_pipeline;
    }

    virtual void NamingCallBack(const stltype::string& name) override;

private:
    MTL::ComputePipelineState* m_pipeline{nullptr};
};

class GraphicsPipelineMetal : public PipelineMetalBase
{
public:
    GraphicsPipelineMetal(const ShaderCollection& shaders,
                          const PipeVertInfo& vertexInputs,
                          const PipelineInfo& pipeInfo);

    GraphicsPipelineMetal() = default;
    ~GraphicsPipelineMetal();

    virtual void CleanUp() override;

    bool HasDynamicViewScissorState() const
    {
        return true;
    }
    bool NeedsVertexBuffers() const;

    MTL::RenderPipelineState* GetRef() const
    {
        return m_pipeline;
    }
    // Set on the encoder, not baked into the PSO
    MTL::DepthStencilState* GetDepthStencilState() const
    {
        return m_depthStencilState;
    }

    virtual void NamingCallBack(const stltype::string& name) override;

private:
    MTL::RenderPipelineState* m_pipeline{nullptr};
    MTL::DepthStencilState* m_depthStencilState{nullptr};
};
