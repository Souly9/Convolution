#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Passes/RenderPass.h"

namespace RenderPasses
{
// Shared base for ray tracing compute passes.
class RTComputePassBase : public ConvolutionRenderPass
{
public:
    explicit RTComputePassBase(const stltype::string& name) : ConvolutionRenderPass(name)
    {
    }

    QueueType GetQueueType() const override { return QueueType::Compute; }
};
} // namespace RenderPasses
