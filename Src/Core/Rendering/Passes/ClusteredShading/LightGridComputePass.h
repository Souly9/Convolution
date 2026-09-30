#pragma once
#include "../RenderPass.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Core/Pipeline.h"
#include "../../../../../Shaders/Globals/PushConstants.h"

namespace RenderPasses
{
class LightGridComputePass : public ConvolutionRenderPass
{
public:
    LightGridComputePass();
    ~LightGridComputePass();

    void Init(const SharedResourceManager& resourceManager) override;
    void BuildPipelines() override;
    void BuildBuffers() override;
    void CreateSharedDescriptorLayout() override;

    void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                             FrameRendererContext& previousFrameCtx,
                             u32 thisFrameNum) override {}
    void Setup(::RenderGraphBuilder& builder, const MainPassData& data) override;
    void RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx) override;

    bool WantsToRender() const override
    {
        return true;
    }

    QueueType GetQueueType() const override
    {
        return QueueType::Compute;
    }

    PassStage GetPassStage() const override
    {
        return PassStage::EarlyCompute;
    }

protected:
    ComputePipeline m_lightCullingComputePipeline;
    ClusterPushConstants m_pushConstants;
};
} // namespace RenderPasses
