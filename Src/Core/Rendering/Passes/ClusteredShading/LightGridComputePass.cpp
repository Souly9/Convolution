#include "LightGridComputePass.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/Defines/BindingSlots.h"
#include "Core/Rendering/Core/Utils/DescriptorLayoutUtils.h"
#include "Core/Rendering/Passes/PassManager.h"

#define ViewSet         0
#define LightClusterSet 1
#define ClusterGridSet  2

RenderPasses::LightGridComputePass::LightGridComputePass() : ConvolutionRenderPass("LightGridComputePass")
{
    CreateSharedDescriptorLayout();
}

void RenderPasses::LightGridComputePass::Init(RendererAttachmentInfo& attachmentInfo,
                                              const SharedResourceManager& resourceManager)
{
    ScopedZone("LightGridComputePass::Init");
    BuildBuffers();
    BuildPipelines();
}

void RenderPasses::LightGridComputePass::BuildBuffers()
{
    ScopedZone("LightGridComputePass::BuildBuffers");

    m_clusterGridBuffer = StorageBuffer(UBO::ClusterGridSSBOSize, true);
    m_clusterGridBuffer.SetName("ClusterGridSSBO");
}

void RenderPasses::LightGridComputePass::BuildPipelines()
{
    ScopedZone("LightGridComputePass::BuildPipelines");

    auto computeShader = Shader("Shaders/ClusteredLightCulling.comp.spv", "main");

    ShaderCollection shaders{};
    shaders.pVertShader = nullptr;
    shaders.pFragShader = nullptr;
    shaders.pComputeShader = &computeShader;

    // Create descriptor set for ClusterGrid (Set 2) - pass local buffer
    PipelineDescriptorLayout clusterGridLayout{};
    clusterGridLayout.type = DescriptorType::StorageBuffer;
    clusterGridLayout.setIndex = ClusterGridSet;
    clusterGridLayout.bindingSlot = s_clusterGridSSBOBindingSlot;
    clusterGridLayout.shaderStagesToBind = ShaderTypeBits::Compute;

    m_computeDescLayout = DescriptorLaytoutUtils::CreateOneDescriptorSetForAll({clusterGridLayout});

    DescriptorPoolCreateInfo fieldInfo{};
    fieldInfo.enableStorageBufferDescriptors = true;
    m_descPool.Create(fieldInfo);

    m_pComputeDescSet = m_descPool.CreateDescriptorSet(m_computeDescLayout.GetRef());
    m_pComputeDescSet->SetBindingSlot(s_clusterGridSSBOBindingSlot);
    m_pComputeDescSet->WriteSSBOUpdate(m_clusterGridBuffer);

    PipelineInfo pipeInfo{};
    pipeInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;

    PushConstant pushConst;
    pushConst.shaderUsage = ShaderTypeBits::Compute;
    pushConst.offset = 0;
    pushConst.size = sizeof(ClusterPushConstants);
    pipeInfo.pushConstantInfo.constants.push_back(pushConst);

    m_pComputePipeline = new ComputePipeline(shaders, pipeInfo);
}

void RenderPasses::LightGridComputePass::CreateSharedDescriptorLayout()
{
    // Set 0: View UBO
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View, ViewSet));

    // Set 1: Light cluster data (TileArraySSBO)
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TileArraySSBO, LightClusterSet));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::LightUniformsUBO, LightClusterSet));

    // Set 2: Cluster grid SSBO (pass-local, but layout in shared for pipeline creation)
    PipelineDescriptorLayout clusterGridLayout{};
    clusterGridLayout.type = DescriptorType::StorageBuffer;
    clusterGridLayout.setIndex = ClusterGridSet;
    clusterGridLayout.bindingSlot = s_clusterGridSSBOBindingSlot;
    clusterGridLayout.shaderStagesToBind = ShaderTypeBits::Compute;
    m_sharedDescriptors.emplace_back(clusterGridLayout);
}

void RenderPasses::LightGridComputePass::UpdateClusterGrid(const DirectX::XMINT3& clusterCount,
                                                           f32 nearPlane,
                                                           f32 farPlane)
{
    m_pushConstants.clusterCount = clusterCount;
    m_pushConstants.nearFar = mathstl::Vector4(nearPlane, farPlane, 0.0f, 0.0f);
    m_clusterGridDirty = true;
}

void RenderPasses::LightGridComputePass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                                             FrameRendererContext& previousFrameCtx,
                                                             u32 thisFrameNum)
{
    // Update cluster grid if dirty
    if (m_clusterGridDirty)
    {
        // TODO: Upload cluster grid AABBs when fully implemented
        m_clusterGridDirty = false;
    }
}

void RenderPasses::LightGridComputePass::Render(const MainPassData& data,
                                                FrameRendererContext& ctx,
                                                CommandBuffer* pCmdBuffer)
{
    ScopedZone("LightGridComputePass::Render");

    if (!m_pComputePipeline)
        return;

    auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    m_pushConstants.clusterCount = renderState.clusterCount;

    // Update stats for debug display
    const u32 totalClusters = renderState.clusterCount.x * renderState.clusterCount.y * renderState.clusterCount.z;
    g_pApplicationState->RegisterUpdateFunction([totalClusters](ApplicationState& state)
                                                { state.renderState.totalClusterCount = totalClusters; });

    // Dispatch: one workgroup per cluster slice (Z), threads handle X*Y (8x8 workgroup)
    const u32 workgroupsX = (m_pushConstants.clusterCount.x + 7) / 8;
    const u32 workgroupsY = (m_pushConstants.clusterCount.y + 7) / 8;
    const u32 workgroupsZ = m_pushConstants.clusterCount.z;

    GenericComputeDispatchCmd cmd(m_pComputePipeline, workgroupsX, workgroupsY, workgroupsZ);
    cmd.descriptorSets = {ctx.mainViewUBODescriptor, ctx.tileArraySSBODescriptor, m_pComputeDescSet};
    cmd.SetPushConstants(0, m_pushConstants);

    pCmdBuffer->RecordCommand(cmd);
}