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
    virtual void Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager) override;
    virtual void RecreateResolutionDependentResources(RendererAttachmentInfo& attachmentInfo,
                                                      const SharedResourceManager& resourceManager) override;
    virtual void BuildPipelines() override;

    virtual void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                     FrameRendererContext& previousFrameCtx,
                                     u32 thisFrameNum) override;
    virtual void Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer) override;
    virtual void CreateSharedDescriptorLayout() override;
    // Always want to composite
    virtual bool WantsToRender() const override;

protected:
    PSO m_mainPSO;
};
} // namespace RenderPasses
