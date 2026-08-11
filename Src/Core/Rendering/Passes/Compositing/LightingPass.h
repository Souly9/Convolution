#pragma once
#include "Core/Rendering/Passes/RenderPass.h"

namespace RenderPasses
{
// Performs deferred lighting calculations using a compute shader
class LightingPass : public ConvolutionRenderPass
{
public:
    LightingPass();
    virtual void BuildBuffers() override {}
    virtual void Init(const SharedResourceManager& resourceManager) override;
    virtual void RecreateResolutionDependentResources(const SharedResourceManager& resourceManager) override;
    virtual void BuildPipelines() override;

    virtual void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                     FrameRendererContext& previousFrameCtx,
                                     u32 thisFrameNum) override {}
    virtual void Setup(::RenderGraphBuilder& builder, const MainPassData& data) override;
    virtual void RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const struct RGExecutionContext& execCtx) override;
    virtual void CreateSharedDescriptorLayout() override;
    virtual bool WantsToRender() const override;
    virtual QueueType GetQueueType() const override { return QueueType::Compute; }
    virtual PassStage GetPassStage() const override { return PassStage::Lighting; }

protected:
    ComputePipeline m_computePipeline;
};
} // namespace RenderPasses
