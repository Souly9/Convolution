#pragma once
#include "Core/Rendering/Core/RenderingData.h"
#include "Core/Rendering/Passes/RenderPass.h"
#include "Core/Rendering/Vulkan/VkPipeline.h"

namespace RenderPasses
{
class ClusterDebugPass : public ConvolutionRenderPass
{
public:
    ClusterDebugPass();

    void Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager) override;
    void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                             FrameRendererContext& previousFrameCtx,
                             u32 thisFrameNum) override;
    void Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer) override;
    bool WantsToRender() const override;

private:
    void BuildPipelines() override;
    void CreateSharedDescriptorLayout();
    void BuildBuffers() override;

    PSO m_pipeline;
    IndirectDrawCommandBuffer m_indirectCmdBuffer;
    IndexBufferVulkan m_indexBuffer; // Used to supply indices 0..23 for the cube lines
    VertexBufferVulkan m_dummyVertexBuffer; // Dummy VB for binding requirement
    RenderingData m_mainRenderingData;
};
} // namespace RenderPasses
