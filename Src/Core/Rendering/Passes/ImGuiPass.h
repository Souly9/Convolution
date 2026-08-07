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
        builder.ReadTexture(RGResourceID::Swapchain, SyncStages::COLOR_ATTACHMENT_OUTPUT, AccessFlags::COLOR_ATTACHMENT_WRITE, ImageLayout::COLOR_ATTACHMENT_OPTIMAL);
        builder.WriteColorAttachment(RGResourceID::Swapchain, LoadOp::LOAD, StoreOp::STORE);
    }

    virtual void CreateSharedDescriptorLayout() override
    {
    }

    void UpdateImGuiScaling();

    virtual bool WantsToRender() const override;

protected:
    DescriptorPool m_descPool;
    RenderingData m_mainRenderingData;
};
} // namespace RenderPasses
