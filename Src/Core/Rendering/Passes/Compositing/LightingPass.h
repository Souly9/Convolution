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
    virtual void Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager) override;
    virtual void RecreateResolutionDependentResources(RendererAttachmentInfo& attachmentInfo,
                                                      const SharedResourceManager& resourceManager) override;
    virtual void BuildPipelines() override;

    virtual void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                     FrameRendererContext& previousFrameCtx,
                                     u32 thisFrameNum) override {}
    virtual void Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer) override;
    virtual void CreateSharedDescriptorLayout() override;
    virtual bool WantsToRender() const override;

protected:
    ComputePipeline m_computePipeline;
};
} // namespace RenderPasses
