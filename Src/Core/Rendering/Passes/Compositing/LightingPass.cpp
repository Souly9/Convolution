#include "LightingPass.h"
#include "Core/Rendering/Core/Pipeline.h"
#include "Core/Rendering/Core/RenderGraph/PassContext.h"
#include "Core/Rendering/Core/RenderGraph/RenderGraphBuilder.h"
#include "Core/Rendering/Vulkan/VkTextureManager.h"

using namespace RenderPasses;

using LightingContext = PassContextPack<
    PassCtx::BindlessWithImages,
    PassCtx::View,
    PassCtx::GlobalInstance,
    PassCtx::LightCluster,
    PassCtx::GBufferCtx>;

LightingPass::LightingPass() : ConvolutionRenderPass("LightingPass")
{
    CreateSharedDescriptorLayout();
}

void LightingPass::Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager)
{
    ScopedZone("LightingPass::Init");
    RecreateResolutionDependentResources(attachmentInfo, resourceManager);
    BuildPipelines();
}

void LightingPass::RecreateResolutionDependentResources(RendererAttachmentInfo& attachmentInfo,
                                                        const SharedResourceManager& resourceManager)
{
}

void LightingPass::BuildPipelines()
{
    ScopedZone("LightingPass::BuildPipelines");
    auto compShader = Shader("Shaders/LightingPass.comp.spv", "main");

    ShaderCollection shaders{};
    shaders.pComputeShader = &compShader;

    PipelineInfo pipeInfo{};
    pipeInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;

    m_computePipeline = ComputePipeline(shaders, pipeInfo);
}

void LightingPass::Setup(::RenderGraphBuilder& builder, const MainPassData& data)
{
    builder.DeclareContexts<
        PassCtx::BindlessWithImages,
        PassCtx::View,
        PassCtx::GlobalInstance,
        PassCtx::LightCluster,
        PassCtx::GBufferCtx>();

    builder.ReadTexture(RGResourceID::MainDepth, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    builder.ReadTexture(RGResourceID::GBufferAlbedo, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    builder.ReadTexture(RGResourceID::GBufferNormal, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    builder.ReadTexture(RGResourceID::GBufferUVMat, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    builder.ReadTexture(RGResourceID::ScreenSpaceShadows, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);

    auto sceneColor = builder.DeclareStorageTexture(RGResourceID::GBufferThisFrameColor, TexFormat::R16G16B16A16_FLOAT, RGSizeClass::RenderResolution);
    builder.WriteStorageImage(sceneColor, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_WRITE);
    builder.SetHasSideEffects();
}

void LightingPass::RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx)
{
    ScopedZone("LightingPass::Render");
    StartRenderPassProfilingScope(execCtx.pCmdBuffer);

    const u32 groupCountX = (static_cast<u32>(data.renderState.renderResolution.x) + 7) / 8;
    const u32 groupCountY = (static_cast<u32>(data.renderState.renderResolution.y) + 7) / 8;
    const u32 groupCountZ = 1;

    GenericComputeDispatchCmd cmd(&m_computePipeline, groupCountX, groupCountY, groupCountZ);
    cmd.descriptorSets = execCtx.GetDescriptors();

    execCtx.pCmdBuffer->RecordCommand(cmd);
    EndRenderPassProfilingScope(execCtx.pCmdBuffer);
}

void LightingPass::Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer)
{
    ScopedZone("LightingPass::Render");
    StartRenderPassProfilingScope(pCmdBuffer);

    const u32 groupCountX = (static_cast<u32>(data.renderState.renderResolution.x) + 7) / 8;
    const u32 groupCountY = (static_cast<u32>(data.renderState.renderResolution.y) + 7) / 8;
    const u32 groupCountZ = 1;

    GenericComputeDispatchCmd cmd(&m_computePipeline, groupCountX, groupCountY, groupCountZ);

    if (!data.bufferDescriptors.empty())
    {
        const auto transformSSBOSet = data.bufferDescriptors.at(UBO::DescriptorContentsType::GlobalInstanceData);
        const auto combinedBindlessSet = DescriptorSet::Cast(g_pTexManager->GetCombinedBindlessDescriptorSet());
        const auto tileArraySSBOSet = data.bufferDescriptors.at(UBO::DescriptorContentsType::LightData);
        const auto gbufferUBO = data.bufferDescriptors.at(UBO::DescriptorContentsType::GBuffer);

        cmd.descriptorSets = {combinedBindlessSet,
                              data.mainView.descriptorSet,
                              transformSSBOSet,
                              tileArraySSBOSet,
                              gbufferUBO};
    }

    pCmdBuffer->RecordCommand(cmd);
    EndRenderPassProfilingScope(pCmdBuffer);
}

void LightingPass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.clear();
    AppendLayoutPreset(DescriptorPresets::Bindless(true));
    AppendLayoutPreset(DescriptorPresets::View());
    AppendLayoutPreset(DescriptorPresets::GlobalInstanceData());
    AppendLayoutPreset(DescriptorPresets::LightCluster(3));
    AppendLayoutPreset(DescriptorPresets::GBuffer(4));
}

bool LightingPass::WantsToRender() const
{
    return true;
}
