#pragma once
#include "GenericGeometryPass.h"

namespace RenderPasses
{
class DebugShapePass : public GenericGeometryPass
{
public:
    DebugShapePass();

    virtual void BuildBuffers() override;

    virtual void Init(const SharedResourceManager& resourceManager) override;
    virtual void RecreateResolutionDependentResources(const SharedResourceManager& resourceManager) override;
    virtual void BuildPipelines() override;

    virtual void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                     FrameRendererContext& previousFrameCtx,
                                     u32 thisFrameNum) override;

    virtual void RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const struct RGExecutionContext& execCtx) override;

    virtual void CreateSharedDescriptorLayout() override;
    virtual void Setup(::RenderGraphBuilder& builder, const MainPassData& data) override;

    virtual bool WantsToRender() const override;
    virtual PassStage GetPassStage() const override { return PassStage::MainGeometry; }

protected:
    PSO m_solidDebugObjectsPSO;
    PSO m_wireframeDebugObjectsPSO;

    stltype::fixed_vector<IndirectDrawCmdBuf, SWAPCHAIN_IMAGES> m_indirectCmdBuffersWireframe;
};
} // namespace RenderPasses
