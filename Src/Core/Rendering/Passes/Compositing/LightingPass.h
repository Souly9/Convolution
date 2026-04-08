#pragma once
#include "../GenericGeometryPass.h"

namespace RenderPasses
{
// Renders a full screen quad and performs lighting calculations using GBuffer textures
class LightingPass : public GenericGeometryPass
{
public:
    LightingPass();
    virtual void BuildBuffers() override
    {
    }
    virtual void Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager) override;
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