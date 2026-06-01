#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/States.h"
#include "Core/Rendering/Passes/RenderPass.h"
#include "Core/Rendering/Passes/GenericGeometryPass.h"
#include "../PassManager.h"

namespace RenderPasses
{
class RTCompositePass : public GenericGeometryPass
{
public:
    RTCompositePass();

    void Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager) override;
    void RecreateResolutionDependentResources(RendererAttachmentInfo& attachmentInfo,
                                              const SharedResourceManager& resourceManager) override;
    void BuildPipelines() override;
    void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                             FrameRendererContext& previousFrameCtx,
                             u32 thisFrameNum) override;
    void Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer) override;
    bool WantsToRender() const override;
    void BuildBuffers() override {}

protected:
    void CreateSharedDescriptorLayout() override;

    PSO m_mainPSO;
};
} // namespace RenderPasses
