#pragma once
#include "../GenericGeometryPass.h"

namespace RenderPasses
{
class DepthPrePass : public GenericGeometryPass
{
public:
    DepthPrePass();

    virtual void BuildPipelines() override;

protected:
    PSO m_mainPSO;

    // Inherited via GenericGeometryPass
    void BuildBuffers() override;
    void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                             FrameRendererContext& previousFrameCtx,
                             u32 thisFrameNum) override;
    void Setup(::RenderGraphBuilder& builder, const MainPassData& data) override;
    void RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx) override;
    void CreateSharedDescriptorLayout() override;
    void Init(const SharedResourceManager& resourceManager) override;
    void RecreateResolutionDependentResources(const SharedResourceManager& resourceManager) override;
    bool WantsToRender() const override;
};
} // namespace RenderPasses
