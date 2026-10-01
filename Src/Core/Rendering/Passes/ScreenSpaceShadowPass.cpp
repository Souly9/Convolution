#include "ScreenSpaceShadowPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Core/Pipeline.h"
#include "PassManager.h"
#include "ScreenSpaceShadowsHelper.h"
#include "Core/Rendering/Core/FrameTransitionRecorder.h"


using namespace RenderPasses;

ScreenSpaceShadowPass::ScreenSpaceShadowPass() : ConvolutionRenderPass("ScreenSpaceShadowPass")
{
    CreateSharedDescriptorLayout();
}

void ScreenSpaceShadowPass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.clear();
    AppendLayoutPreset(DescriptorPresets::Bindless(true));
    AppendLayoutPreset(DescriptorPresets::View());
}

void ScreenSpaceShadowPass::Init(const SharedResourceManager& resourceManager)
{
    ScopedZone("ScreenSpaceShadowPass::Init");

    RecreateResolutionDependentResources(resourceManager);
    BuildPipelines();
}

void ScreenSpaceShadowPass::RecreateResolutionDependentResources(const SharedResourceManager& resourceManager)
{
    ScopedZone("ScreenSpaceShadowPass::RecreateResolutionDependentResources");
    m_pDepthTex = nullptr;
}

void ScreenSpaceShadowPass::BuildPipelines()
{
    ScopedZone("ScreenSpaceShadowPass::BuildPipelines");
    auto compShader = Shader("Shaders/ScreenSpaceShadows.comp.spv", "main");

    ShaderCollection shaders{};
    shaders.pComputeShader = &compShader;

    PipelineInfo pipeInfo{};
    pipeInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;

    PushConstant pushConst;
    pushConst.shaderUsage = ShaderTypeBits::Compute;
    pushConst.offset = 0;
    pushConst.size = sizeof(ScreenSpaceShadowPushConstants);
    pipeInfo.pushConstantInfo.constants.push_back(pushConst);

    m_computePipeline = ComputePipeline(shaders, pipeInfo);
}

void ScreenSpaceShadowPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                                FrameRendererContext& previousFrameCtx,
                                                u32 thisFrameNum)
{
}

bool ScreenSpaceShadowPass::WantsToRender() const
{
    return mathstl::isFlagSet(g_engine.GetApplicationState().GetCurrentApplicationState().renderState.debugFlags, (u32)DebugFlags::SSSEnabled);
}

void ScreenSpaceShadowPass::Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer)
{
}

#include "Core/Rendering/Core/RenderGraph/RGExecutionContext.h"

void ScreenSpaceShadowPass::RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx)
{
    ScopedZone("ScreenSpaceShadowPass::Render");

    mathstl::Vector3 lightDir(0.0f, 1.0f, 0.0f);
    if (!data.csmViews.empty())
    {
        lightDir = -data.csmViews[0].dir;
    }

    const auto* pDepthTex = execCtx.GetTexture(RGResourceID::MainDepth);
    DirectX::XMUINT3 depthExtents = pDepthTex ? pDepthTex->GetInfo().extents
                                              : DirectX::XMUINT3((u32)execCtx.GetRenderResolution().x, (u32)execCtx.GetRenderResolution().y, 1);

    Bend::DispatchList dispatchList = SSSHelper::BuildBendDispatchList(lightDir, data.pViewData->viewProjectionInverse, depthExtents);
    
    StartRenderPassProfilingScope(execCtx.pCmdBuffer);

    m_pushConstants.depthTexIdx = execCtx.GetBindless(RGResourceID::MainDepth);
    m_pushConstants.outputTexIdx = execCtx.GetBindless(RGResourceID::ScreenSpaceShadows);
    m_pushConstants.invDepthTextureSize = mathstl::Vector2(1.0f / depthExtents.x, 1.0f / depthExtents.y);

    m_pushConstants.lightCoordinate = mathstl::Vector4(
        dispatchList.LightCoordinate_Shader[0],
        dispatchList.LightCoordinate_Shader[1],
        dispatchList.LightCoordinate_Shader[2],
        dispatchList.LightCoordinate_Shader[3]
    );

    for (int i = 0; i < dispatchList.DispatchCount; i++)
    {
        const auto& dispatch = dispatchList.Dispatch[i];
        m_pushConstants.waveOffset.x = dispatch.WaveOffset_Shader[0];
        m_pushConstants.waveOffset.y = dispatch.WaveOffset_Shader[1];
        GenericComputeDispatchCmd cmd(&m_computePipeline, dispatch.WaveCount[0], dispatch.WaveCount[1], dispatch.WaveCount[2]);
        cmd.descriptorSets = execCtx.GetDescriptors();
        cmd.SetPushConstants(0, m_pushConstants);

        execCtx.pCmdBuffer->RecordCommand(cmd);
    }
    
    EndRenderPassProfilingScope(execCtx.pCmdBuffer);
}

#include "Core/Rendering/Core/RenderGraph/RenderGraphBuilder.h"

void ScreenSpaceShadowPass::Setup(::RenderGraphBuilder& builder, const MainPassData& data)
{
    builder.DeclareContexts<
        PassCtx::BindlessWithImages,
        PassCtx::View>();

    builder.ReadTexture(RGResourceID::MainDepth, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    auto sss = builder.DeclareStorageTexture(RGResourceID::ScreenSpaceShadows, TexFormat::R8_UNORM, RGSizeClass::RenderResolution);
    builder.WriteStorageImage(sss, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_WRITE);
    builder.SetHasSideEffects();
}
