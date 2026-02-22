#include "ScreenSpaceShadowPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "PassManager.h"
#include "ScreenSpaceShadowsHelper.h"


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


    m_pDepthTex = static_cast<const Texture*>(attachmentInfo.depthAttachment.GetTexture());

    BuildPipelines();
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
    pushConst.size = sizeof(PushConstants);
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
    return g_pApplicationState->GetCurrentApplicationState().renderState.sssEnabled;
}

void ScreenSpaceShadowPass::Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer)
{
    ScopedZone("ScreenSpaceShadowPass::Render");
    
    if (data.csmViews.empty()) return;
    
    // Use the directional light from the csmViews
    const mathstl::Vector3& lightDir = -data.csmViews[0].dir;

    auto depthExtents = m_pDepthTex->GetInfo().extents;

    Bend::DispatchList dispatchList = SSSHelper::BuildBendDispatchList(lightDir, data.mainCamInvViewProj, depthExtents);
    
    StartRenderPassProfilingScope(pCmdBuffer);

    m_pushConstants.depthTexIdx = data.depthBufferBindlessHandle;
    m_pushConstants.outputTexIdx = data.screenSpaceShadows;
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
        m_pushConstants.waveOffset[0] = dispatch.WaveOffset_Shader[0];
        m_pushConstants.waveOffset[1] = dispatch.WaveOffset_Shader[1];
        GenericComputeDispatchCmd cmd(&m_computePipeline, dispatch.WaveCount[0], dispatch.WaveCount[1], dispatch.WaveCount[2]);
        cmd.descriptorSets.push_back(texArraySet);
        cmd.descriptorSets.push_back(imageArraySet);
        cmd.SetPushConstants(0, m_pushConstants);

        pCmdBuffer->RecordCommand(cmd);
    }
    
    EndRenderPassProfilingScope(pCmdBuffer);
}
