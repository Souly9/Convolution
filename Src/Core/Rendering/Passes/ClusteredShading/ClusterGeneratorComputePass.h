#pragma once
#include "../RenderPass.h"
#include "LightGridComputePass.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Core/Pipeline.h"

namespace RenderPasses
{
class ClusterGeneratorComputePass : public ConvolutionRenderPass
{
public:
    ClusterGeneratorComputePass();
    ~ClusterGeneratorComputePass() = default;

    void Init(const SharedResourceManager& resourceManager) override;
    void BuildPipelines() override;
    void BuildBuffers() override;
    void CreateSharedDescriptorLayout() override;

    void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                             FrameRendererContext& previousFrameCtx,
                             u32 thisFrameNum) override {}
    void Setup(::RenderGraphBuilder& builder, const MainPassData& data) override;
    void RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx) override;

    bool WantsToRender() const override { return true; }
    QueueType GetQueueType() const override { return QueueType::Compute; }
    PassStage GetPassStage() const override { return PassStage::EarlyCompute; }

protected:
    ComputePipeline m_pipeline;
    ClusterPushConstants m_pushConstants;

    // The view-space cluster AABBs only depend on the projection and the cluster counts
    struct GridKey
    {
        DirectX::XMINT3 clusterCount{0, 0, 0};
        f32 zNear{0.0f};
        f32 zFar{0.0f};
        f32 fovY{0.0f};
        f32 aspect{0.0f};

        bool operator==(const GridKey& o) const
        {
            return clusterCount.x == o.clusterCount.x && clusterCount.y == o.clusterCount.y &&
                   clusterCount.z == o.clusterCount.z && zNear == o.zNear && zFar == o.zFar && fovY == o.fovY &&
                   aspect == o.aspect;
        }
    };
    GridKey m_builtGridKey{};
    bool m_hasBuiltGrid{false};
};
} // namespace RenderPasses
