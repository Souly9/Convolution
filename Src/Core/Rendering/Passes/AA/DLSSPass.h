#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/States.h"
#include "Core/Rendering/Core/Buffer.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Core/Nvidia/StreamlineManager.h"
#include "Core/Rendering/Passes/RenderPass.h"
#include <EASTL/array.h>
#include <EASTL/fixed_vector.h>

namespace RenderPasses
{
// DLSS Super Resolution and Ray Reconstruction; AA::Current() decides which one runs
class DLSSPass : public ConvolutionRenderPass
{
public:
    DLSSPass();
    ~DLSSPass();

    void Init(const SharedResourceManager& resourceManager) override;
    void BuildPipelines() override {}
    void BuildBuffers() override {}
    void CreateSharedDescriptorLayout() override {}

    void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                             FrameRendererContext& previousFrameCtx,
                             u32 thisFrameNum) override {}
    void RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const struct RGExecutionContext& execCtx) override;
    void Setup(::RenderGraphBuilder& builder, const MainPassData& data) override;

    bool WantsToRender() const override;
    QueueType GetQueueType() const override { return QueueType::Compute; }
    PassStage GetPassStage() const override { return PassStage::PostProcess; }

private:
    struct TagStorage
    {
        stltype::fixed_vector<sl::Resource, 16, false> resources;
        stltype::fixed_vector<sl::ResourceTag, 16, false> tags;
    };
    // Per frame slot so the native callback can outlive this frame's recording; fixed capacity keeps tag pointers valid
    stltype::array<TagStorage, FRAMES_IN_FLIGHT> m_tags{};
    bool m_evaluatedLastFrame{false};
    u32 m_lastResetGeneration{0};
};

// Copies the tonemapper exposure into DLSSExposure; a separate node so the graph owns the transfer barrier
class DLSSExposurePass : public ConvolutionRenderPass
{
public:
    DLSSExposurePass();

    void Init(const SharedResourceManager& resourceManager) override {}
    void BuildPipelines() override {}
    void BuildBuffers() override {}
    void CreateSharedDescriptorLayout() override {}

    void RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                             FrameRendererContext& previousFrameCtx,
                             u32 thisFrameNum) override {}
    void RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const struct RGExecutionContext& execCtx) override;
    void Setup(::RenderGraphBuilder& builder, const MainPassData& data) override;

    bool WantsToRender() const override;
    QueueType GetQueueType() const override { return QueueType::Compute; }
    PassStage GetPassStage() const override { return PassStage::PostProcess; }

private:
    // The previous frame may still be copying from its own slot
    stltype::array<StagingBuffer, FRAMES_IN_FLIGHT> m_staging{};
};
} // namespace RenderPasses
