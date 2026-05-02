#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/States.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Passes/RenderPass.h"
#include "../../../../Shaders/Globals/PushConstants.h"
#include "../PassManager.h"

namespace RenderPasses
{
class RTDebugViewPass : public ConvolutionRenderPass
{
public:
    RTDebugViewPass();

    void Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager) override;
    void BuildPipelines() override;
    void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                             FrameRendererContext& previousFrameCtx,
                             u32 thisFrameNum) override;
    void Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer) override;
    bool WantsToRender() const override;
    void BuildBuffers() override {}

    BindlessTextureHandle GetOutputBindlessHandle() const { return m_outputBindlessHandle; }

protected:
    void CreateSharedDescriptorLayout() override;
    void CreateTLASDescriptorResources();
    void RecreateOutputTexture();

    ComputePipeline m_computePipeline{};
    DescriptorPool m_descriptorPool{};
    DescriptorSetLayout m_tlasDescriptorLayout{};
    stltype::fixed_vector<DescriptorSet::Ptr, SWAPCHAIN_IMAGES> m_tlasDescriptors{SWAPCHAIN_IMAGES};
    Texture* m_pOutputTexture{nullptr};
    TextureHandle m_outputTextureHandle{0};
    BindlessTextureHandle m_outputBindlessHandle{0};
    bool m_outputInitialized{false};
    RTDebugViewPushConstants m_pushConstants{};
};
} // namespace RenderPasses
