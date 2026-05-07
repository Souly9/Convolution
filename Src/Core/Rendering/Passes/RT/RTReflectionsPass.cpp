#include "RTReflectionsPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Rendering/Core/Defines/BindingSlots.h"
#include "Core/Rendering/Core/RT/RTSceneManager.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Vulkan/Utils/VkDescriptorLayoutUtils.h"

using namespace RenderPasses;

RTReflectionsPass::RTReflectionsPass() : ConvolutionRenderPass("RTReflectionsPass")
{
    CreateSharedDescriptorLayout();
}

void RTReflectionsPass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.clear();
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalArrayTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalImages, 1));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TransformSSBO, 3));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::PrevTransformSSBO, 3));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GlobalObjectDataSSBOs, 3));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::InstanceDataSSBO, 3));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GBufferUBO, 4));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::ShadowmapUBO, 4));

    PipelineDescriptorLayout tlasLayout{};
    tlasLayout.type = DescriptorType::AccelerationStructure;
    tlasLayout.bindingSlot = s_rtSceneASBindingSlot;
    tlasLayout.shaderStagesToBind = ShaderTypeBits::Compute;
    tlasLayout.setIndex = 5;
    m_sharedDescriptors.push_back(tlasLayout);
}

void RTReflectionsPass::CreateTLASDescriptorResources()
{
    m_descriptorPool = DescriptorPool();
    m_descriptorPool.Create({.enableBindlessTextureDescriptors = false,
                             .enableStorageBufferDescriptors = false,
                             .enableAccelerationStructureDescriptors = true,
                             .freeDescriptorSet = true});
    m_descriptorPool.SetName("RT Reflections Descriptor Pool");

    PipelineDescriptorLayout tlasLayout{};
    tlasLayout.type = DescriptorType::AccelerationStructure;
    tlasLayout.bindingSlot = s_rtSceneASBindingSlot;
    tlasLayout.shaderStagesToBind = ShaderTypeBits::Compute;
    tlasLayout.setIndex = 0;
    m_tlasDescriptorLayout = DescriptorLayoutUtils::CreateOneDescriptorSetForAll({tlasLayout});
    m_tlasDescriptorLayout.SetName("RT Reflections TLAS Layout");

    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
    {
        m_tlasDescriptors[i] = m_descriptorPool.CreateDescriptorSet(m_tlasDescriptorLayout);
        m_tlasDescriptors[i]->SetBindingSlot(s_rtSceneASBindingSlot);
        m_tlasDescriptors[i]->SetName("RT Reflections TLAS Descriptor Set " + stltype::to_string(i));
    }
}

void RTReflectionsPass::Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager)
{
    ScopedZone("RTReflectionsPass::Init");
    (void)attachmentInfo;
    (void)resourceManager;
    CreateTLASDescriptorResources();
    BuildPipelines();
}

void RTReflectionsPass::BuildPipelines()
{
    auto computeShader = Shader("Shaders/RTReflections.comp.spv", "main");

    ShaderCollection shaders{};
    shaders.pComputeShader = &computeShader;

    PipelineInfo pipeInfo{};
    pipeInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;

    PushConstant pushConst{};
    pushConst.shaderUsage = ShaderTypeBits::Compute;
    pushConst.offset = 0;
    pushConst.size = sizeof(RTReflectionsPushConstants);
    pipeInfo.pushConstantInfo.constants.push_back(pushConst);

    m_computePipeline = ComputePipeline(shaders, pipeInfo);
}

void RTReflectionsPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                            FrameRendererContext& previousFrameCtx,
                                            u32 thisFrameNum)
{
    (void)meshes;
    (void)previousFrameCtx;
    (void)thisFrameNum;
}

bool RTReflectionsPass::WantsToRender() const
{
    const auto& rtState = g_pApplicationState->GetCurrentApplicationState().renderState.rt;
    return rtState.enabled && rtState.reflectionsEnabled;
}

void RTReflectionsPass::Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer)
{
    if (data.pRTSceneManager == nullptr ||
        data.rtReflectionsTextureHandle == 0 ||
        data.rtReflectedSceneColorTextureHandle == 0)
    {
        return;
    }

    StartRenderPassProfilingScope(pCmdBuffer);

    const bool hasReadyTLAS = data.pRTSceneManager->HasReadyTLAS(ctx.imageIdx);
    if (!hasReadyTLAS)
    {
        EndRenderPassProfilingScope(pCmdBuffer);
        return;
    }

    {
        const RT::TLASFrameData* pTLASFrameData = data.pRTSceneManager->GetTLASFrameData(ctx.imageIdx);
        if (pTLASFrameData == nullptr)
        {
            EndRenderPassProfilingScope(pCmdBuffer);
            return;
        }
        else
        {
            m_tlasDescriptors[ctx.imageIdx]->WriteAccelerationStructureUpdate(pTLASFrameData->accelerationStructure,
                                                                              s_rtSceneASBindingSlot);
        }
    }

    const auto& rtState = g_pApplicationState->GetCurrentApplicationState().renderState.rt;
    m_pushConstants.reflectionsTexIdx = data.rtReflectionsTextureHandle;
    m_pushConstants.reflectedSceneColorTexIdx = data.rtReflectedSceneColorTextureHandle;
    m_pushConstants.inputSceneColorTexIdx = data.temporalResources.currentColorHandle;
    m_pushConstants.debugMode = static_cast<u32>(rtState.reflectionsDebugMode);
    m_pushConstants.maxRayDistance = ctx.zFar;
    m_pushConstants.reflectionIntensity = 1.0f;
    m_pushConstants.hasReadyTLAS = hasReadyTLAS ? 1u : 0u;

    const u32 groupCountX = (static_cast<u32>(data.renderState.renderResolution.x) + 7) / 8;
    const u32 groupCountY = (static_cast<u32>(data.renderState.renderResolution.y) + 7) / 8;

    GenericComputeDispatchCmd dispatchCmd(&m_computePipeline, groupCountX, groupCountY, 1);
    dispatchCmd.descriptorSets = {data.bufferDescriptors.at(UBO::DescriptorContentsType::BindlessTextureArray),
                                  data.bufferDescriptors.at(UBO::DescriptorContentsType::BindlessImageArray),
                                  data.mainView.descriptorSet,
                                  data.bufferDescriptors.at(UBO::DescriptorContentsType::GlobalInstanceData),
                                  data.bufferDescriptors.at(UBO::DescriptorContentsType::GBuffer),
                                  m_tlasDescriptors[ctx.imageIdx]};
    dispatchCmd.SetPushConstants(0, m_pushConstants);
    pCmdBuffer->RecordCommand(dispatchCmd);

    EndRenderPassProfilingScope(pCmdBuffer);
}
