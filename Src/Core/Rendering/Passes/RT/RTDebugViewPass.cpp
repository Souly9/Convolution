#include "RTDebugViewPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Rendering/Core/Defines/BindingSlots.h"
#include "Core/Rendering/Core/RT/RTSceneManager.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/Core/SharedResourceManager.h"
#include "Core/Rendering/Vulkan/Utils/VkDescriptorLayoutUtils.h"
#include "Core/Rendering/Vulkan/VkTextureManager.h"

using namespace RenderPasses;

RTDebugViewPass::RTDebugViewPass() : ConvolutionRenderPass("RTDebugViewPass")
{
    CreateSharedDescriptorLayout();
}

void RTDebugViewPass::CreateSharedDescriptorLayout()
{
    m_sharedDescriptors.clear();
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalImages, 1));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View, 2));

    PipelineDescriptorLayout tlasLayout{};
    tlasLayout.type = DescriptorType::AccelerationStructure;
    tlasLayout.bindingSlot = s_rtSceneASBindingSlot;
    tlasLayout.shaderStagesToBind = ShaderTypeBits::Compute;
    tlasLayout.setIndex = 3;
    m_sharedDescriptors.push_back(tlasLayout);
}

void RTDebugViewPass::CreateTLASDescriptorResources()
{
    m_descriptorPool = DescriptorPool();
    m_descriptorPool.Create({.enableBindlessTextureDescriptors = false,
                             .enableStorageBufferDescriptors = false,
                             .enableAccelerationStructureDescriptors = true,
                             .freeDescriptorSet = true});
    m_descriptorPool.SetName("RT Debug Descriptor Pool");

    PipelineDescriptorLayout tlasLayout{};
    tlasLayout.type = DescriptorType::AccelerationStructure;
    tlasLayout.bindingSlot = s_rtSceneASBindingSlot;
    tlasLayout.shaderStagesToBind = ShaderTypeBits::Compute;
    tlasLayout.setIndex = 0;
    m_tlasDescriptorLayout = DescriptorLayoutUtils::CreateOneDescriptorSetForAll({tlasLayout});
    m_tlasDescriptorLayout.SetName("RT Debug TLAS Layout");

    for (u32 i = 0; i < SWAPCHAIN_IMAGES; ++i)
    {
        m_tlasDescriptors[i] = m_descriptorPool.CreateDescriptorSet(m_tlasDescriptorLayout);
        m_tlasDescriptors[i]->SetBindingSlot(s_rtSceneASBindingSlot);
        m_tlasDescriptors[i]->SetName("RT Debug TLAS Descriptor Set " + stltype::to_string(i));
    }
}

void RTDebugViewPass::RecreateOutputTexture()
{
    if (m_outputTextureHandle != 0)
    {
        g_pTexManager->FreeTexture(m_outputTextureHandle);
        m_outputTextureHandle = 0;
        m_pOutputTexture = nullptr;
        m_outputBindlessHandle = 0;
    }

    const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
    DynamicTextureRequest request{};
    request.extents = DirectX::XMUINT3(static_cast<u32>(renderState.renderResolution.x),
                                       static_cast<u32>(renderState.renderResolution.y),
                                       1);
    request.handle = g_pTexManager->GenerateHandle();
    request.format = TexFormat::R16G16B16A16_FLOAT;
    request.usage = Usage::Storage | Usage::Sampled;
    request.isPersistent = true;
    request.AddName("RT Debug View");

    m_outputTextureHandle = request.handle;
    m_pOutputTexture = static_cast<Texture*>(g_pTexManager->CreateTextureImmediate(request));
    m_outputBindlessHandle = g_pTexManager->MakeTextureBindless(request.handle, true);
    m_outputInitialized = false;
}

void RTDebugViewPass::Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager)
{
    ScopedZone("RTDebugViewPass::Init");
    (void)attachmentInfo;
    (void)resourceManager;
    CreateTLASDescriptorResources();
    BuildPipelines();
}

void RTDebugViewPass::BuildPipelines()
{
    auto computeShader = Shader("Shaders/RTDebugView.comp.spv", "main");

    ShaderCollection shaders{};
    shaders.pComputeShader = &computeShader;

    PipelineInfo pipeInfo{};
    pipeInfo.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;

    PushConstant pushConst{};
    pushConst.shaderUsage = ShaderTypeBits::Compute;
    pushConst.offset = 0;
    pushConst.size = sizeof(RTDebugViewPushConstants);
    pipeInfo.pushConstantInfo.constants.push_back(pushConst);

    m_computePipeline = ComputePipeline(shaders, pipeInfo);
}

void RTDebugViewPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                          FrameRendererContext& previousFrameCtx,
                                          u32 thisFrameNum)
{
    (void)meshes;
    (void)previousFrameCtx;
    (void)thisFrameNum;
    RecreateOutputTexture();
}

bool RTDebugViewPass::WantsToRender() const
{
    const auto& rtState = g_pApplicationState->GetCurrentApplicationState().renderState.rt;
    return rtState.enabled && rtState.debugViewEnabled;
}

void RTDebugViewPass::Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer)
{
    if (m_pOutputTexture == nullptr || data.pRTSceneManager == nullptr)
        return;

    StartRenderPassProfilingScope(pCmdBuffer);

    const bool hasReadyTLAS = data.pRTSceneManager->HasReadyTLAS(ctx.imageIdx);
    const ImageLayout currentLayout =
        m_outputInitialized ? ImageLayout::SHADER_READ_ONLY_OPTIMAL : ImageLayout::UNDEFINED;

    if (!hasReadyTLAS)
    {
        ImageLayoutTransitionCmd toClearLayout(m_pOutputTexture);
        toClearLayout.oldLayout = currentLayout;
        toClearLayout.newLayout = ImageLayout::TRANSFER_DST_OPTIMAL;
        VkTextureManager::SetLayoutBarrierMasks(toClearLayout, toClearLayout.oldLayout, toClearLayout.newLayout);
        pCmdBuffer->RecordCommand(toClearLayout);

        ClearColorImageCmd clearCmd(m_pOutputTexture);
        clearCmd.color.float32[0] = 0.0f;
        clearCmd.color.float32[1] = 0.0f;
        clearCmd.color.float32[2] = 0.0f;
        clearCmd.color.float32[3] = 1.0f;
        pCmdBuffer->RecordCommand(clearCmd);

        ImageLayoutTransitionCmd toReadLayout(m_pOutputTexture);
        toReadLayout.oldLayout = ImageLayout::TRANSFER_DST_OPTIMAL;
        toReadLayout.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
        VkTextureManager::SetLayoutBarrierMasks(toReadLayout, toReadLayout.oldLayout, toReadLayout.newLayout);
        pCmdBuffer->RecordCommand(toReadLayout);

        m_outputInitialized = true;
        EndRenderPassProfilingScope(pCmdBuffer);
        return;
    }

    const RT::TLASFrameData* pTLASFrameData = data.pRTSceneManager->GetTLASFrameData(ctx.imageIdx);
    if (pTLASFrameData == nullptr)
    {
        EndRenderPassProfilingScope(pCmdBuffer);
        return;
    }

    m_tlasDescriptors[ctx.imageIdx]->WriteAccelerationStructureUpdate(pTLASFrameData->accelerationStructure,
                                                                      s_rtSceneASBindingSlot);

    ImageLayoutTransitionCmd toGeneralLayout(m_pOutputTexture);
    toGeneralLayout.oldLayout = currentLayout;
    toGeneralLayout.newLayout = ImageLayout::GENERAL;
    VkTextureManager::SetLayoutBarrierMasks(toGeneralLayout, toGeneralLayout.oldLayout, toGeneralLayout.newLayout);
    pCmdBuffer->RecordCommand(toGeneralLayout);

    const auto& rtState = g_pApplicationState->GetCurrentApplicationState().renderState.rt;
    m_pushConstants.outputTexIdx = m_outputBindlessHandle;
    m_pushConstants.debugMode = rtState.debugMode;
    m_pushConstants.maxRayDistance = ctx.zFar;

    const u32 groupCountX = (static_cast<u32>(data.renderState.renderResolution.x) + 7) / 8;
    const u32 groupCountY = (static_cast<u32>(data.renderState.renderResolution.y) + 7) / 8;

    GenericComputeDispatchCmd dispatchCmd(&m_computePipeline, groupCountX, groupCountY, 1);
    dispatchCmd.descriptorSets = {data.bufferDescriptors.at(UBO::DescriptorContentsType::BindlessTextureArray),
                                  data.bufferDescriptors.at(UBO::DescriptorContentsType::BindlessImageArray),
                                  data.mainView.descriptorSet,
                                  m_tlasDescriptors[ctx.imageIdx]};
    dispatchCmd.SetPushConstants(0, m_pushConstants);
    pCmdBuffer->RecordCommand(dispatchCmd);

    ImageLayoutTransitionCmd toReadLayout(m_pOutputTexture);
    toReadLayout.oldLayout = ImageLayout::GENERAL;
    toReadLayout.newLayout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;
    VkTextureManager::SetLayoutBarrierMasks(toReadLayout, toReadLayout.oldLayout, toReadLayout.newLayout);
    pCmdBuffer->RecordCommand(toReadLayout);

    m_outputInitialized = true;
    EndRenderPassProfilingScope(pCmdBuffer);
}
