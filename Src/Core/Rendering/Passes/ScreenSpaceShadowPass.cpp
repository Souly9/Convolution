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
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalArrayTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalImages, 1));
}

void ScreenSpaceShadowPass::Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager)
{
    ScopedZone("ScreenSpaceShadowPass::Init");

    RecreateResolutionDependentResources(attachmentInfo, resourceManager);
    BuildPipelines();
}

void ScreenSpaceShadowPass::RecreateResolutionDependentResources(RendererAttachmentInfo& attachmentInfo,
                                                                 const SharedResourceManager& resourceManager)
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
    return mathstl::isFlagSet(g_pApplicationState->GetCurrentApplicationState().renderState.debugFlags, (u32)DebugFlags::SSSEnabled);
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
                                              : DirectX::XMUINT3((u32)data.renderState.renderResolution.x, (u32)data.renderState.renderResolution.y, 1);

    Bend::DispatchList dispatchList = SSSHelper::BuildBendDispatchList(lightDir, data.mainCamInvViewProj, depthExtents);
    
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

    auto texArraySet = data.bufferDescriptors.at(UBO::DescriptorContentsType::BindlessTextureArray);
    auto imageArraySet = data.bufferDescriptors.at(UBO::DescriptorContentsType::BindlessImageArray);

    for (int i = 0; i < dispatchList.DispatchCount; i++)
    {
        const auto& dispatch = dispatchList.Dispatch[i];
        m_pushConstants.waveOffset.x = dispatch.WaveOffset_Shader[0];
        m_pushConstants.waveOffset.y = dispatch.WaveOffset_Shader[1];
        GenericComputeDispatchCmd cmd(&m_computePipeline, dispatch.WaveCount[0], dispatch.WaveCount[1], dispatch.WaveCount[2]);
        cmd.descriptorSets.push_back(texArraySet);
        cmd.descriptorSets.push_back(imageArraySet);
        cmd.SetPushConstants(0, m_pushConstants);

        execCtx.pCmdBuffer->RecordCommand(cmd);
    }
    
    EndRenderPassProfilingScope(execCtx.pCmdBuffer);
}

#include "Core/Rendering/Core/RenderGraph/RenderGraphBuilder.h"

void ScreenSpaceShadowPass::Setup(::RenderGraphBuilder& builder, const MainPassData& data)
{
    builder.ReadTexture(RGResourceID::MainDepth, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    auto sss = builder.DeclareStorageTexture(RGResourceID::ScreenSpaceShadows, TexFormat::R8_UNORM, RGSizeClass::RenderResolution);
    builder.WriteStorageImage(sss, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_WRITE);
    builder.SetHasSideEffects();
}
