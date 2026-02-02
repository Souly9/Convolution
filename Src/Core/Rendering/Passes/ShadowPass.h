#pragma once
#include "GenericGeometryPass.h"

namespace RenderPasses
{
class CSMPass : public GenericGeometryPass
{
public:
    CSMPass();

    virtual void BuildBuffers() override;

    virtual void Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager) override;
    virtual void BuildPipelines() override;

    virtual void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                     FrameRendererContext& previousFrameCtx,
                                     u32 thisFrameNum) override;

    virtual void Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer) override;

    virtual void CreateSharedDescriptorLayout() override;
    virtual bool WantsToRender() const override;

    // Sets cascade count and rebuilds pipeline if changed
    void SetCascadeCount(u32 cascades);

protected:
    stltype::vector<mathstl::Matrix> ComputeLightViewProjMatrices(u32 cascades,
                                                                  f32 fov,
                                                                  f32 mainCamNear,
                                                                  f32 mainCamFar,
                                                                  mathstl::Vector2 mainFrustumExtents,
                                                                  const mathstl::Matrix& mainCamView,
                                                                  const mathstl::Vector3& lightDir,
                                                                  stltype::array<f32, 16>& splits);
    stltype::vector<mathstl::Vector4> ComputeFrustumCornersWS(const mathstl::Matrix& proj, const mathstl::Matrix& view);

    // Every pass should only have one pipeline as we're working with uber shaders + bindless
    PSO m_mainPSO;
    IndirectDrawCommandBuffer m_indirectCmdBuffer;
    IndirectDrawCountBuffer m_indirectCountBuffer;
    // Current cascade count for viewMask calculation
    u32 m_cascadeCount{3};
};
} // namespace RenderPasses