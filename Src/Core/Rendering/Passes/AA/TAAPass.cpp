#include "TAAPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/States.h"
#include "Core/Rendering/Core/AntiAliasing.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/Passes/PassManager.h"
#include "Core/Global/Profiling.h"
#include "Core/Rendering/Core/RenderGraph/RenderGraphBuilder.h"
#include "Core/Rendering/Core/RenderGraph/RGExecutionContext.h"

using namespace RenderPasses;

TAAPass::TAAPass() : ConvolutionRenderPass("TAAPass")
{
    CreateSharedDescriptorLayout();
}

TAAPass::~TAAPass()
{
}

void TAAPass::Init(const SharedResourceManager& resourceManager)
{
    ScopedZone("TAAPass::Init");
    BuildPipelines();
}

void TAAPass::BuildPipelines()
{
    auto computeShader = Shader("Shaders/TAA.comp.spv", "main");

    ShaderCollection shaders{};
    shaders.pComputeShader = &computeShader;

    PipelineInfo pipeInfo{};
    pipeInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;

    PushConstant pushConst;
    pushConst.shaderUsage = ShaderTypeBits::Compute;
    pushConst.offset = 0;
    pushConst.size = sizeof(TAAPushConstants);
    pipeInfo.pushConstantInfo.constants.push_back(pushConst);

    m_taaPipeline = ComputePipeline(shaders, pipeInfo);
}

void TAAPass::BuildBuffers()
{
}

bool TAAPass::WantsToRender() const
{
    return AA::Current().temporal == AA::Temporal::TAA;
}

void TAAPass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.clear();
    AppendLayoutPreset(DescriptorPresets::Bindless(true));
    AppendLayoutPreset(DescriptorPresets::View());
    AppendLayoutPreset(DescriptorPresets::GBuffer());
}

void TAAPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                  FrameRendererContext& previousFrameCtx,
                                  u32 thisFrameNum)
{
}

void TAAPass::Setup(::RenderGraphBuilder& builder, const MainPassData& data)
{
    builder.DeclareContexts<
        PassCtx::BindlessWithImages,
        PassCtx::View,
        PassCtx::GBufferCtx>();

    builder.ReadTexture(RGResourceID::GBufferThisFrameColor, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);
    builder.ReadTexture(RGResourceID::MainDepth, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::DEPTH_STENCIL_READ_ONLY_OPTIMAL);
    builder.ReadTexture(RGResourceID::GBufferVelocity, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_READ, ImageLayout::SHADER_READ_ONLY_OPTIMAL);

    // Previous history is sampled via bindless; the composite always returns this frame's output to a sampled layout
    auto history = builder.DeclareStorageTexture(RGResourceID::TAAHistory, TexFormat::R16G16B16A16_FLOAT, RGSizeClass::OutputResolution);
    builder.WriteStorageImage(history, SyncStages::COMPUTE_SHADER, AccessFlags::SHADER_WRITE);
    builder.SetHasSideEffects();
}

void TAAPass::RenderWithGraph(const MainPassData& data, const FrameRendererContext& ctx, const RGExecutionContext& execCtx)
{
    ScopedZone("TAAPass::Render");
    CommandBuffer* pCmdBuffer = execCtx.pCmdBuffer;
    StartRenderPassProfilingScope(pCmdBuffer);

    const auto& renderState = g_engine.GetApplicationState().GetCurrentApplicationState().renderState;
    TAAPushConstants pushConstants{};
    pushConstants.velocityRejectionStart = renderState.taaVelocityRejectionStart;
    pushConstants.velocityRejectionEnd = renderState.taaVelocityRejectionEnd;

    const mathstl::Vector2 resolution = execCtx.GetRenderResolution();
    GenericComputeDispatchCmd cmd(&m_taaPipeline,
                                  (static_cast<u32>(resolution.x) + 7) / 8,
                                  (static_cast<u32>(resolution.y) + 7) / 8,
                                  1);
    cmd.descriptorSets = execCtx.GetDescriptors();
    cmd.SetPushConstants(0, pushConstants);
    pCmdBuffer->RecordCommand(cmd);

    EndRenderPassProfilingScope(pCmdBuffer);
}
