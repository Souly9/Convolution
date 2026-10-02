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

GraphicsPipelineMetal::GraphicsPipelineMetal(const ShaderCollection& shaders,
                                             const PipeVertInfo& vertexInputs,
                                             const PipelineInfo& pipeInfo)
{
    m_info = pipeInfo;
}

GraphicsPipelineMetal::~GraphicsPipelineMetal()
{
    TRACKED_DESC_IMPL
}
