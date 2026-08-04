#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/States.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Passes/RenderPass.h"
#include "../../../../../Shaders/Globals/PushConstants.h"

namespace RenderPasses
{
class BloomPass : public ConvolutionRenderPass
{
public:
    BloomPass();
    ~BloomPass();

    void Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager) override;
    void BuildPipelines() override;
    void BuildBuffers() override {}
    void CreateSharedDescriptorLayout() override;

    void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                             FrameRendererContext& previousFrameCtx,
                             u32 thisFrameNum) override {}
    void Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer) override;
    void RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const struct RGExecutionContext& execCtx) override;
    void Setup(::RenderGraphBuilder& builder, const MainPassData& data) override;

    bool WantsToRender() const override;
    QueueType GetQueueType() const override { return QueueType::Compute; }

private:
    ComputePipeline m_downsamplePipeline;
    ComputePipeline m_upsamplePipeline;
    BloomPushConstants m_pushConstants;
    TextureHandle m_hLens1{0};
    TextureHandle m_hLens2{0};
};
} // namespace RenderPasses
