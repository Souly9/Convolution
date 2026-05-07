#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/States.h"
#include "Core/Rendering/Passes/RenderPass.h"
#include "../../../../Shaders/Globals/PushConstants.h"
#include "../PassManager.h"

namespace RenderPasses
{
class RTReflectionsPass : public ConvolutionRenderPass
{
public:
    RTReflectionsPass();

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
    void CreateTLASDescriptorResources();

    ComputePipeline m_computePipeline{};
    DescriptorPool m_descriptorPool{};
    DescriptorSetLayout m_tlasDescriptorLayout{};
    stltype::fixed_vector<DescriptorSet::Ptr, SWAPCHAIN_IMAGES> m_tlasDescriptors{SWAPCHAIN_IMAGES};
    RTReflectionsPushConstants m_pushConstants{};
};
} // namespace RenderPasses
