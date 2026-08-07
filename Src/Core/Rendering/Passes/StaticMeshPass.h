#pragma once
#include "GenericGeometryPass.h"
#include "Core/Rendering/Core/CommandBuffer.h"

namespace RenderPasses
{
class StaticMainMeshPass : public GenericGeometryPass
{
public:
    StaticMainMeshPass();

    virtual void BuildBuffers() override;

    virtual void Init(const SharedResourceManager& resourceManager) override;
    virtual void RecreateResolutionDependentResources(const SharedResourceManager& resourceManager) override;
    virtual void BuildPipelines() override;

    virtual void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                     FrameRendererContext& previousFrameCtx,
                                     u32 thisFrameNum) override;

    void Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer) {}
    virtual void RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const struct RGExecutionContext& execCtx) override;

    virtual void CreateSharedDescriptorLayout() override;
    virtual void Setup(::RenderGraphBuilder& builder, const MainPassData& data) override;
    virtual bool WantsToRender() const override;

protected:
    // Every pass should only have one pipeline as we're working with uber shaders + bindless
    PSO m_mainPSO;
};
} // namespace RenderPasses
