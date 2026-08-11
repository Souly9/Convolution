#pragma once
#include "../GenericGeometryPass.h"

namespace RenderPasses
{
// Final composition pass that combines the TAA output (or lighting output) and UI into the swapchain
class CompositPass : public GenericGeometryPass
{
public:
    CompositPass();
    virtual void BuildBuffers() override
    {
    }
    virtual void Init(const SharedResourceManager& resourceManager) override;
    virtual void RecreateResolutionDependentResources(const SharedResourceManager& resourceManager) override;
    virtual void BuildPipelines() override;

    virtual void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                     FrameRendererContext& previousFrameCtx,
                                     u32 thisFrameNum) override;
    virtual void RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const struct RGExecutionContext& execCtx) override;

    virtual void Setup(::RenderGraphBuilder& builder, const MainPassData& data) override;

    virtual void CreateSharedDescriptorLayout() override;
    // Always want to composite
    virtual bool WantsToRender() const override;
    virtual PassStage GetPassStage() const override { return PassStage::PostProcess; }

protected:
    PSO m_mainPSO;
};
} // namespace RenderPasses
