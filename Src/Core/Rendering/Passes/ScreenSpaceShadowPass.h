#pragma once
#include "Core/Rendering/Passes/RenderPass.h"
#include "../../../../Shaders/Globals/PushConstants.h"

namespace RenderPasses
{
class ScreenSpaceShadowPass : public ConvolutionRenderPass
{
public:
    ScreenSpaceShadowPass();

    virtual void Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager) override;
    virtual void RecreateResolutionDependentResources(RendererAttachmentInfo& attachmentInfo,
                                                      const SharedResourceManager& resourceManager) override;
    virtual void BuildPipelines() override;
    virtual void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                     FrameRendererContext& previousFrameCtx,
                                     u32 thisFrameNum) override;
    virtual void Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer) override;
    virtual bool WantsToRender() const override;
    virtual void BuildBuffers() override {}

protected:
    void CreateSharedDescriptorLayout() override;

    ComputePipeline m_computePipeline;
    const Texture* m_pDepthTex{nullptr};

    ScreenSpaceShadowPushConstants m_pushConstants;
};
} // namespace RenderPasses
