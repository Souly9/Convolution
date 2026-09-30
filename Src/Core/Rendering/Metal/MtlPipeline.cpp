#include "MtlPipeline.h"

// TODO(Metal): build MTL::RenderPipelineDescriptor / ComputePipelineDescriptor from PipelineInfo

ComputePipelineMetal::ComputePipelineMetal(const ShaderCollection& shaders, const PipelineInfo& pipeInfo)
{
    m_info = pipeInfo;
}

ComputePipelineMetal::~ComputePipelineMetal()
{
    TRACKED_DESC_IMPL
}

void ComputePipelineMetal::CleanUp()
{
}

void ComputePipelineMetal::NamingCallBack(const stltype::string& name)
{
}

GraphicsPipelineMetal::GraphicsPipelineMetal(const ShaderCollection& shaders,
                                             const PipeVertInfo& vertexInputs,
                                             const PipelineInfo& pipeInfo)
{
    m_info = pipeInfo;
    m_vertexInfo = vertexInputs;
}

GraphicsPipelineMetal::~GraphicsPipelineMetal()
{
    TRACKED_DESC_IMPL
}

void GraphicsPipelineMetal::CleanUp()
{
}

bool GraphicsPipelineMetal::NeedsVertexBuffers() const
{
    return !m_vertexInfo.m_attributeDescriptions.empty();
}

void GraphicsPipelineMetal::NamingCallBack(const stltype::string& name)
{
}
