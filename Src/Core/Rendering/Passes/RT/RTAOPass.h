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

    void Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager) override;
    void BuildPipelines() override;
    void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                             FrameRendererContext& previousFrameCtx,
                             u32 thisFrameNum) override;
    void Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer) override;
    bool WantsToRender() const override;
    void BuildBuffers() override {}

protected:
    void CreateSharedDescriptorLayout() override;

    ComputePipeline m_computePipeline{};
    RTAOPushConstants m_pushConstants{};
};
} // namespace RenderPasses
