#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/States.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Passes/RenderPass.h"
#include "../../../../../Shaders/Globals/PushConstants.h"

namespace RenderPasses
{
class TAAPass : public ConvolutionRenderPass
{
public:
    TAAPass();
    ~TAAPass();

    void Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager) override;
    void BuildPipelines() override;
    void BuildBuffers() override;
    void CreateSharedDescriptorLayout() override;

    void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                             FrameRendererContext& previousFrameCtx,
                             u32 thisFrameNum) override;
    void Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer) override;

    bool WantsToRender() const override;

private:

    ComputePipeline m_taaPipeline;
    TAAPushConstants m_pushConstants;
    AntialiasingType m_lastAAType{AntialiasingType::None};
    u32 m_lastDebugMode{0};
};
} // namespace RenderPasses
