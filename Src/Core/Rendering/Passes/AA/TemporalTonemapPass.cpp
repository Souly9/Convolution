#include "TemporalTonemapPass.h"
#include "Core/Global/Profiling.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Passes/PassManager.h"

using namespace RenderPasses;

TemporalTonemapPass::TemporalTonemapPass() : ConvolutionRenderPass("TemporalTonemapPass")
{
    CreateSharedDescriptorLayout();
}

TemporalTonemapPass::~TemporalTonemapPass()
{
}

void TemporalTonemapPass::Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager)
{
    (void)attachmentInfo;
    (void)resourceManager;
    BuildPipelines();
}

void TemporalTonemapPass::BuildPipelines()
{
    auto computeShader = Shader("Shaders/TemporalTonemap.comp.spv", "main");

    ShaderCollection shaders{};
    shaders.pComputeShader = &computeShader;

    PipelineInfo pipeInfo{};
    pipeInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;

    m_pipeline = ComputePipeline(shaders, pipeInfo);
}

void TemporalTonemapPass::BuildBuffers()
{
}

bool TemporalTonemapPass::WantsToRender() const
{
    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    return renderState.aaType == AntialiasingType::TAA_SMAA || renderState.aaType == AntialiasingType::DLSS;
}

void TemporalTonemapPass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.clear();
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalArrayTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalImages, 1));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GBufferUBO, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::ShadowmapUBO, 2));
}

void TemporalTonemapPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                             FrameRendererContext& previousFrameCtx,
                                             u32 thisFrameNum)
{
    (void)meshes;
    (void)previousFrameCtx;
    (void)thisFrameNum;
}

void TemporalTonemapPass::Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer)
{
    (void)ctx;
    ScopedZone("TemporalTonemapPass::Render");
    StartRenderPassProfilingScope(pCmdBuffer);

    const u32 groupCountX = (static_cast<u32>(data.renderState.renderResolution.x) + 7) / 8;
    const u32 groupCountY = (static_cast<u32>(data.renderState.renderResolution.y) + 7) / 8;

    GenericComputeDispatchCmd cmd(&m_pipeline, groupCountX, groupCountY, 1);
    cmd.descriptorSets = {data.bufferDescriptors.at(UBO::DescriptorContentsType::BindlessTextureArray),
                          data.bufferDescriptors.at(UBO::DescriptorContentsType::BindlessImageArray),
                          data.bufferDescriptors.at(UBO::DescriptorContentsType::GBuffer)};
    pCmdBuffer->RecordCommand(cmd);

    EndRenderPassProfilingScope(pCmdBuffer);
}
