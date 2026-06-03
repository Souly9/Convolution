#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/States.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Passes/RenderPass.h"
#include "Core/Rendering/Passes/GenericGeometryPass.h"

namespace RenderPasses
{

class SMAAPass : public GenericGeometryPass
{
public:
    SMAAPass();
    ~SMAAPass();

    void Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager) override;
    void BuildPipelines() override;
    void BuildBuffers() override {}
    void CreateSharedDescriptorLayout() override;

    void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                             FrameRendererContext& previousFrameCtx,
                             u32 thisFrameNum) override;
    void Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer) override;

    bool WantsToRender() const override;

private:
    PSO m_edgePSO;
    PSO m_blendPSO;
    PSO m_neighborhoodPSO;

    
    
    BindlessTextureHandle m_searchTexBindless{0};
    BindlessTextureHandle m_areaTexBindless{0};
    u32 m_currentFrameIdx{0};
    bool m_outputWritten{true};
};

} // namespace RenderPasses
