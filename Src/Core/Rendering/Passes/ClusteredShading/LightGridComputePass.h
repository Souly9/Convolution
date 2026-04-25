#pragma once
#include "../RenderPass.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Vulkan/VkPipeline.h"
#include "../../../../../Shaders/Globals/PushConstants.h"

namespace RenderPasses
{
class LightGridComputePass : public ConvolutionRenderPass
{
public:
    LightGridComputePass();
    ~LightGridComputePass();

    void Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager) override;
    void BuildPipelines() override;
    void BuildBuffers() override;
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
    ClusterPushConstants m_pushConstants;
};
} // namespace RenderPasses
