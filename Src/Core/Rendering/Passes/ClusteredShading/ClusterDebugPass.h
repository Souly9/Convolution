#pragma once
#include "Core/Rendering/Core/RenderingData.h"
#include "Core/Rendering/Passes/RenderPass.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/Pipeline.h"

namespace RenderPasses
{
class ClusterDebugPass : public ConvolutionRenderPass
{
public:
    ClusterDebugPass();

    void Init(const SharedResourceManager& resourceManager) override;
    void RecreateResolutionDependentResources(const SharedResourceManager& resourceManager) override;
    void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                             FrameRendererContext& previousFrameCtx,
                             u32 thisFrameNum) override;
    void Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer) {}
    void RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const struct RGExecutionContext& execCtx) override;
    void Setup(::RenderGraphBuilder& builder, const MainPassData& data) override;
    bool WantsToRender() const override;
    PassStage GetPassStage() const override { return PassStage::UI; }

private:
    void BuildPipelines() override;
    void CreateSharedDescriptorLayout() override;
    void BuildBuffers() override;

    PSO m_pipeline;
    stltype::fixed_vector<IndirectDrawCmdBuf, SWAPCHAIN_IMAGES> m_indirectCmdBuffers;
    u32 m_currentFrameIdx{0};
    IndexBuffer m_indexBuffer; // Used to supply indices 0..23 for the cube lines
    VertexBuffer m_dummyVertexBuffer; // Dummy VB for binding requirement
};
} // namespace RenderPasses
