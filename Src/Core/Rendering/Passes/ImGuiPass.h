#pragma once
#include "Core/ECS/Components/RenderComponent.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Core/StaticFunctions.h"
#include "Core/Rendering/Core/Synchronization.h"
#include "PassManager.h"

namespace RenderPasses
{
class ImGuiPass : public ConvolutionRenderPass
{
public:
    ImGuiPass();

    virtual void BuildBuffers() override
    {
    }

    virtual void Init(const SharedResourceManager& resourceManager) override;
    virtual void RecreateResolutionDependentResources(const SharedResourceManager& resourceManager) override;

    virtual void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                     FrameRendererContext& previousFrameCtx,
                                     u32 thisFrameNum) override
    {
    }

    virtual void RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const struct RGExecutionContext& execCtx) override;

    virtual void Setup(::RenderGraphBuilder& builder, const MainPassData& data) override
    {
        builder.SetHasSideEffects();
        builder.ReadTexture(RGResourceID::GBufferAlbedo, SyncStages::FRAGMENT_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        builder.ReadTexture(RGResourceID::GBufferNormal, SyncStages::FRAGMENT_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        builder.ReadTexture(RGResourceID::GBufferUVMat, SyncStages::FRAGMENT_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        builder.ReadTexture(RGResourceID::GBufferVelocity, SyncStages::FRAGMENT_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        builder.ReadTexture(RGResourceID::GBufferThisFrameColor, SyncStages::FRAGMENT_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        builder.ReadTexture(RGResourceID::TemporalResolve, SyncStages::FRAGMENT_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        builder.ReadTexture(RGResourceID::GBufferPostAAColor, SyncStages::FRAGMENT_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        builder.ReadTexture(RGResourceID::BloomMip0, SyncStages::FRAGMENT_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        builder.ReadTexture(RGResourceID::ScreenSpaceShadows, SyncStages::FRAGMENT_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        builder.ReadTexture(RGResourceID::GBufferRoughness, SyncStages::FRAGMENT_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        builder.ReadTexture(RGResourceID::GBufferEntityID, SyncStages::FRAGMENT_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        builder.ReadTexture(RGResourceID::GBufferDebug, SyncStages::FRAGMENT_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        builder.ReadTexture(RGResourceID::RTReflections, SyncStages::FRAGMENT_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
        builder.ReadTexture(RGResourceID::RTAOOutput, SyncStages::FRAGMENT_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);

        builder.ReadTexture(RGResourceID::Swapchain, SyncStages::COLOR_ATTACHMENT_OUTPUT, AccessFlags::COLOR_ATTACHMENT_WRITE, ImageLayout::COLOR_ATTACHMENT_OPTIMAL);
        builder.WriteColorAttachment(RGResourceID::Swapchain, LoadOp::LOAD, StoreOp::STORE);
    }

    virtual void CreateSharedDescriptorLayout() override
    {
    }

    void UpdateImGuiScaling();

    virtual bool WantsToRender() const override;
    virtual PassStage GetPassStage() const override { return PassStage::UI; }

protected:
    DescriptorPool m_descPool;
};
} // namespace RenderPasses
