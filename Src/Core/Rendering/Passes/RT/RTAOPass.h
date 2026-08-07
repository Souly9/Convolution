#pragma once
#include "RTComputePassBase.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/States.h"
#include "../../../../Shaders/Globals/PushConstants.h"
#include "../PassManager.h"

namespace RenderPasses
{
class RTAOPass : public RTComputePassBase
{
public:
    RTAOPass();

    void Init(const SharedResourceManager& resourceManager) override;
    void BuildPipelines() override;
    void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                             FrameRendererContext& previousFrameCtx,
                             u32 thisFrameNum) override;
    void RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const struct RGExecutionContext& execCtx) override;
    void Setup(::RenderGraphBuilder& builder, const MainPassData& data) override;
    bool WantsToRender() const override;
    void BuildBuffers() override {}

protected:
    void CreateSharedDescriptorLayout() override;

    ComputePipeline m_computePipeline{};
    RTAOPushConstants m_pushConstants{};
};
} // namespace RenderPasses
