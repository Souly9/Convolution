#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/States.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Passes/RenderPass.h"
#include "../../../../../Shaders/Globals/PushConstants.h"

namespace RenderPasses
{
class TAAPass : public ConvolutionRenderPass
{
public:
    TAAPass();
    ~TAAPass();

    void Init(const SharedResourceManager& resourceManager) override;
    void BuildPipelines() override;
    void BuildBuffers() override;
    void CreateSharedDescriptorLayout() override;

    void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                             FrameRendererContext& previousFrameCtx,
                             u32 thisFrameNum) override;
    void RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const struct RGExecutionContext& execCtx) override;
    void Setup(::RenderGraphBuilder& builder, const MainPassData& data) override;

    bool WantsToRender() const override;
    QueueType GetQueueType() const override { return QueueType::Compute; }
    PassStage GetPassStage() const override { return PassStage::PostProcess; }

private:

    ComputePipeline m_taaPipeline;
    TAAPushConstants m_pushConstants;
    AntialiasingType m_lastAAType{AntialiasingType::None};
    u32 m_lastDebugMode{0};
    u32 m_resetFramesRemaining{0};
};
} // namespace RenderPasses
