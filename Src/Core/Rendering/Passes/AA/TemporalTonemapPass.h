#pragma once

#include "Core/Rendering/Passes/RenderPass.h"

namespace RenderPasses
{
class TemporalTonemapPass : public ConvolutionRenderPass
{
public:
    TemporalTonemapPass();
    ~TemporalTonemapPass() override;

    void Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager) override;
    void BuildPipelines() override;
    void BuildBuffers() override;
    bool WantsToRender() const override;
    void CreateSharedDescriptorLayout() override;
    void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                             FrameRendererContext& previousFrameCtx,
                             u32 thisFrameNum) override;
    void Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer) override;

private:
    ComputePipeline m_pipeline{};
};
} // namespace RenderPasses
