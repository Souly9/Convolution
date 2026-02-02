#pragma once
#include "../PassManager.h"
#include "../RenderPass.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"

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
class LightGridComputePass : public ConvolutionRenderPass
{
public:
    LightGridComputePass();

    void Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager) override;
    void BuildPipelines() override;
    void BuildBuffers() override;
    void CreateSharedDescriptorLayout() override;

    void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                             FrameRendererContext& previousFrameCtx,
                             u32 thisFrameNum) override;
    void Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer) override;

    bool WantsToRender() const override
    {
        return true;
    }

    void UpdateClusterGrid(const DirectX::XMINT3& clusterCount, f32 nearPlane, f32 farPlane);

protected:
    ComputePipeline* m_pComputePipeline{nullptr};

    // Cluster grid buffers
    StorageBuffer m_clusterGridBuffer;

    // Descriptor management
    DescriptorPool m_descPool;
    DescriptorSetLayout m_computeDescLayout;
    DescriptorSet* m_pComputeDescSet{nullptr};

    ClusterPushConstants m_pushConstants;
    bool m_clusterGridDirty{true};
};
} // namespace RenderPasses