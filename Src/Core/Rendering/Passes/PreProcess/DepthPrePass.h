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
    IndirectDrawCommandBuffer m_indirectCmdBuffer;
    IndirectDrawCountBuffer m_indirectCountBuffer;

    // Inherited via GenericGeometryPass
    void BuildBuffers() override;
    void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                             FrameRendererContext& previousFrameCtx,
                             u32 thisFrameNum) override;
    void Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer) override;
    void CreateSharedDescriptorLayout() override;
    void Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager) override;
    bool WantsToRender() const override;
};
} // namespace RenderPasses