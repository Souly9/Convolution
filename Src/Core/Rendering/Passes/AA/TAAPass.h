#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/States.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Passes/RenderPass.h"

namespace RenderPasses
{
struct TAAPushConstants
{
    u32 frameIndex;
    u32 resetHistory;
    f32 resolutionX;
    f32 resolutionY;
    f32 currentJitterX;
    f32 currentJitterY;
    f32 previousJitterX;
    f32 previousJitterY;
    f32 zNear;
    f32 zFar;
};

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
};
} // namespace RenderPasses
