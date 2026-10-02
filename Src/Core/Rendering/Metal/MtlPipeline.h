#pragma once
#include "MtlBackendDefines.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Defines/VertexDefines.h"
#include "Core/Rendering/Core/Pipeline.h"
#include "Core/Rendering/Core/Resource.h"
#include "Core/Rendering/Metal/MtlDescriptorSetLayout.h"


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
};

class ComputePipelineMetal : public PipelineMetalBase
{
public:
    ComputePipelineMetal(const ShaderCollection& shaders, const PipelineInfo& pipeInfo);
    ComputePipelineMetal() = default;
    ~ComputePipelineMetal();

    MTL::ComputePipelineState* GetRef() const
    {
        return m_pipeline;
    }

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

    bool HasDynamicViewScissorState() const
    {
        return true;
    }

    MTL::RenderPipelineState* GetRef() const
    {
        return m_pipeline;
    }
    // Set on the encoder, not baked into the PSO
    MTL::DepthStencilState* GetDepthStencilState() const
    {
        return m_depthStencilState;
    }

private:
    MTL::RenderPipelineState* m_pipeline{nullptr};
    MTL::DepthStencilState* m_depthStencilState{nullptr};
};
