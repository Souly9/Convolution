#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/States.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Passes/RenderPass.h"

namespace RenderPasses
{
class XeSSPass : public ConvolutionRenderPass
{
public:
    XeSSPass();
    ~XeSSPass();

    void Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager) override;
    void BuildPipelines() override {}
    void BuildBuffers() override {}
    void CreateSharedDescriptorLayout() override {}

    void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                             FrameRendererContext& previousFrameCtx,
                             u32 thisFrameNum) override {}
    void Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer) override;

    bool WantsToRender() const override;

private:
    mutable bool m_wasActive{false};
};
} // namespace RenderPasses
