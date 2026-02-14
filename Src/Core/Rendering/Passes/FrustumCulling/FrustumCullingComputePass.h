#pragma once
#include "../RenderPass.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Vulkan/VkPipeline.h"

namespace RenderPasses
{
// TODO: Define push constants for frustum culling
struct FrustumCullingPushConstants
{
    // Fill in with necessary data (e.g., view matrix, object count, etc.)
    u32 objectCount;
    // ...
};

// Compute pass for frustum culling
class FrustumCullingComputePass : public ConvolutionRenderPass
{
public:
    FrustumCullingComputePass();
    ~FrustumCullingComputePass();

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

protected:
    ComputePipeline m_cullingPipeline;
    FrustumCullingPushConstants m_pushConstants;
};
} // namespace RenderPasses
