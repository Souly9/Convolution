#pragma once
#include "../RenderPass.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Vulkan/VkPipeline.h"

namespace RenderPasses
{
// Push constants for dynamic cluster parameters
struct ClusterPushConstants
{
    DirectX::XMINT3 clusterCount;
    u32 numLights;
    mathstl::Vector4 nearFar; // x=near, y=far, zw=unused
};

// Compute pass that culls lights into a clustered 3D grid
// AABB generation is handled by SClusterAABB ECS system
// Buffer management is handled by PassManager
class LightGridComputePass : public ConvolutionRenderPass
{
public:
    LightGridComputePass();
    ~LightGridComputePass();

    void Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager) override;
    void BuildPipelines() override;
    void BuildBuffers() override {}
    void CreateSharedDescriptorLayout() override;

    void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                             FrameRendererContext& previousFrameCtx,
                             u32 thisFrameNum) override {}
    void Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer) override;

    bool WantsToRender() const override
    {
        return true;
    }

    QueueType GetQueueType() const override
    {
        return QueueType::Compute;
    }

protected:
    ComputePipeline m_lightCullingComputePipeline;
    ComputePipeline m_clusterComputePipeline;
    ClusterPushConstants m_pushConstants;
};
} // namespace RenderPasses