#pragma once
#include "../GenericGeometryPass.h"

namespace RenderPasses
{
// Renders a full screen quad and composits the gbuffer textures as well as the UI output texture into the swapchain for
// presentation Want to move this into a compute shader in the future
class LightGridComputePass : public GenericGeometryPass
{
public:
    LightGridComputePass();
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
    IndirectDrawCommandBuffer m_indirectCmdBuffer;
    IndirectDrawCountBuffer m_indirectCountBuffer;
};
} // namespace RenderPasses