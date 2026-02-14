#include "ShadowPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/LogDefines.h"
#include "Core/Global/Profiling.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Global/Utils/MathFunctions.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include "EASTL/algorithm.h"
#include "SimpleMath/SimpleMath.h"
#include "Utils/RenderPassUtils.h"
#include <cfloat>


namespace RenderPasses
{
void CSMPass::BuildBuffers()
{
}

CSMPass::CSMPass() : GenericGeometryPass("ShadowPass")
{
    SetVertexInputDescriptions(VertexInputDefines::VertexAttributeTemplates::Complete);
    CreateSharedDescriptorLayout();
}

void CSMPass::Init(RendererAttachmentInfo& attachmentInfo, const SharedResourceManager& resourceManager)
{
    ScopedZone("ShadowPass::Init");

    const auto csmFormat = attachmentInfo.directionalLightShadowMap.format;
    const auto cascadeAttachment = CreateDefaultDepthAttachment(csmFormat, LoadOp::CLEAR, nullptr);
    m_mainRenderingData.depthAttachment = cascadeAttachment;

    InitBaseData(attachmentInfo);
    m_indirectCmdBuffer = IndirectDrawCommandBuffer(1000000);

    BuildPipelines();
}

void CSMPass::BuildPipelines()
{
    ScopedZone("ShadowPass::BuildPipelines");

    auto mainVert = Shader("Shaders/DirLightCSM.vert.spv", "main");
    auto mainFrag = Shader("Shaders/DirLightCSM.frag.spv", "main");

    PipelineInfo info{};
    // info.descriptorSetLayout.pipelineSpecificDescriptors.emplace_back();
    // info.pushConstantInfo.constants.push_back({ ShaderTypeBits::Vertex, 0, sizeof(mathstl::Matrix) * 3 });
    info.descriptorSetLayout.sharedDescriptors = m_sharedDescriptors;
    info.attachmentInfos =
        CreateAttachmentInfo({m_mainRenderingData.colorAttachments}, m_mainRenderingData.depthAttachment);
    // Compute viewMask from cascade count: (1 << cascades) - 1 gives bitmask for all layers
    info.viewMask = (1 << m_cascadeCount) - 1;
    info.rasterizerInfo.cullmode = Cullmode::Back;
    m_mainPSO = PSO(
        ShaderCollection{&mainVert, &mainFrag}, PipeVertInfo{m_vertexInputDescription, m_attributeDescriptions}, info);
}

void CSMPass::RebuildInternalData(const stltype::vector<PassMeshData>& meshes,
                                  FrameRendererContext& previousFrameCtx,
                                  u32 thisFrameNum)
{
    ScopedZone("ShadowPass::Rebuild");

    m_indirectCmdBuffer.EmptyCmds();
    u32 instanceOffset = 0;
    stltype::vector<u32> instanceDataIndices;
    instanceDataIndices.reserve(meshes.size());
    for (const auto& mesh : meshes)
    {
        if (mesh.meshData.IsDebugMesh())
            continue;
        const auto& meshHandle = mesh.meshData.meshResourceHandle;

        m_indirectCmdBuffer.AddIndexedDrawCmd(meshHandle.indexCount,
                                              1, // TODO: instanced rendering
                                              meshHandle.indexBufferOffset,
                                              meshHandle.vertBufferOffset,
                                              instanceOffset);
        instanceDataIndices.emplace_back(mesh.meshData.instanceDataIdx);
        ++instanceOffset;
    }
    RebuildPerObjectBuffer(instanceDataIndices);
    m_indirectCmdBuffer.FillCmds();
    // m_needsBufferSync = true;
}

void RenderPasses::CSMPass::Render(const MainPassData& data, FrameRendererContext& ctx, CommandBuffer* pCmdBuffer)
{
    ScopedZone("ShadowPass::Render");

    auto& sceneGeometryBuffers = data.pResourceManager->GetSceneGeometryBuffers();
    const auto currentFrame = ctx.imageIdx;
    UpdateContextForFrame(currentFrame);
    const auto& passCtx = m_perObjectFrameContexts[currentFrame];

    const auto ex = data.directionalLightShadowMap.pTexture->GetInfo().extents;
    const DirectX::XMINT2 extents(ex.x, ex.y);
    m_mainRenderingData.depthAttachment.SetTexture(data.directionalLightShadowMap.pTexture);

    stltype::vector<ColorAttachment> colorAttachments;
    BeginRenderingCmd cmdBegin{&m_mainPSO, colorAttachments, &m_mainRenderingData.depthAttachment};

    cmdBegin.depthLayerMask = (1 << m_cascadeCount) - 1;
    cmdBegin.extents = extents;

    GenericIndirectDrawCmd cmd{&m_mainPSO, m_indirectCmdBuffer};
    cmd.drawCount = m_indirectCmdBuffer.GetDrawCmdNum();

    if (data.bufferDescriptors.empty())
        cmd.descriptorSets = {g_pTexManager->GetBindlessDescriptorSet()};
    else
    {
        const auto transformSSBOSet = data.bufferDescriptors.at(UBO::DescriptorContentsType::GlobalInstanceData);
        const auto texArraySet = data.bufferDescriptors.at(UBO::DescriptorContentsType::BindlessTextureArray);
        cmd.descriptorSets = {texArraySet,
                              ctx.mainViewUBODescriptor,
                              transformSSBOSet,
                              passCtx.m_perObjectDescriptor,
                              ctx.shadowViewUBODescriptor};
    }
    cmdBegin.drawCmdBuffer = &m_indirectCmdBuffer;
    StartRenderPassProfilingScope(pCmdBuffer);
    if (data.csmViews.empty() == false)
    {
        pCmdBuffer->RecordCommand(cmdBegin);
        BinRenderDataCmd geomBufferCmd(sceneGeometryBuffers.GetVertexBuffer(), sceneGeometryBuffers.GetIndexBuffer());
        pCmdBuffer->RecordCommand(geomBufferCmd);

        const auto& csmView = data.csmViews[0];
        stltype::array<f32, 16> splits{};

        UBO::ShadowmapViewUBO uboData;
        const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
        f32 aspectRatio = data.mainView.viewport.width / data.mainView.viewport.height;
        ComputeLightViewProjMatrices(data.cascades,
                                     ctx.zNear,
                                     ctx.zFar,
                                     renderState.csmLambda,
                                     data.mainView.fov,
                                     aspectRatio,
                                     data.mainCamViewMatrix,
                                     data.mainCamInvViewProj,
                                     csmView.dir,
                                     splits,
                                     uboData.lightViewProjMatrices,
                                     (u32)extents.x);

        uboData.cascadeCount = (s32)data.cascades;
        for (u32 i = 0; i < 4; ++i)
        {
            uboData.cascadeSplits[i] =
                mathstl::Vector4(splits[i * 4 + 0], splits[i * 4 + 1], splits[i * 4 + 2], splits[i * 4 + 3]);
        }

        memcpy(ctx.pMappedShadowViewUBO, &uboData, sizeof(UBO::ShadowmapViewUBO));
        ctx.shadowViewUBODescriptor->WriteBufferUpdate(*ctx.pShadowViewUBO, s_shadowmapViewUBOBindingSlot);
        pCmdBuffer->RecordCommand(cmd);

        pCmdBuffer->RecordCommand(EndRenderingCmd{});
    }
    EndRenderPassProfilingScope(pCmdBuffer);
}

void CSMPass::CreateSharedDescriptorLayout()
{
    // Don't need them but also doesn't hurt, might help GPU to better cache the first two which are needed basically
    // everywhere else
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(Bindless::BindlessType::GlobalArrayTextures, 0));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::View, 1));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::TransformSSBO, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::GlobalObjectDataSSBOs, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::InstanceDataSSBO, 2));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::PerPassObjectSSBO, 3));
    m_sharedDescriptors.emplace_back(PipelineDescriptorLayout(UBO::BufferType::ShadowmapViewUBO, 4));
}

bool CSMPass::WantsToRender() const
{
    return NeedToRender(m_indirectCmdBuffer);
}

void CSMPass::SetCascadeCount(u32 cascades)
{
    if (m_cascadeCount != cascades)
    {
        m_cascadeCount = cascades;
        BuildPipelines();
    }
}

stltype::array<mathstl::Matrix, 16> CSMPass::ComputeLightViewProjMatrices(
    u32 cascades,
    f32 mainCamNear,
    f32 mainCamFar,
    f32 lambda,
    f32 fov,
    f32 aspectRatio,
    const mathstl::Matrix& view,
    const mathstl::Matrix& invViewProj,
    const mathstl::Vector3& lightDir,
    stltype::array<f32, 16>& splits,
    stltype::array<mathstl::Matrix, 16>& lightViewProjMatrices,
    u32 shadowMapSize)
{
    ScopedZone("ShadowPass::Construct LightViewProjMatrices");
    float clipRange = (mainCamFar - mainCamNear);
    float ratio = (mainCamFar / mainCamNear);

    stltype::vector<float> cascadeSplits(cascades);
    for (u32 i = 0; i < cascades; i++)
    {
        float p = (i + 1) / static_cast<float>(cascades);
        float log = mainCamNear * mathstl::pow(ratio, p);
        float uniform = mainCamNear + clipRange * p;
        float d = lambda * (log - uniform) + uniform;
        cascadeSplits[i] = d;
    }

    mathstl::Vector3 lightDirection = lightDir;
    lightDirection.Normalize();
    lightDirection = -lightDirection;

    mathstl::Vector3 up = (std::abs(lightDirection.Dot(mathstl::Vector3(0, 1, 0))) > 0.999f)
                              ? mathstl::Vector3(0, 0, 1)
                              : mathstl::Vector3(0, 1, 0);

    // NDC frustum corners (Vulkan: Z 0..1, XY -1..1)
    constexpr mathstl::Vector3 ndcCorners[8] = {
        {-1, 1, 0},
        {1, 1, 0},
        {1, -1, 0},
        {-1, -1, 0},
        {-1, 1, 1},
        {1, 1, 1},
        {1, -1, 1},
        {-1, -1, 1},
    };

    float lastSplitDist = mainCamNear;
    for (u32 i = 0; i < cascades; i++)
    {
        float splitDist = cascadeSplits[i];
        // Use exact Near/Far for this cascade slice
        float sliceNear = lastSplitDist;
        float sliceFar = splitDist;

        mathstl::Matrix projMat = mathstl::Matrix::CreatePerspectiveFieldOfView(
            DirectX::XMConvertToRadians(fov), aspectRatio, mathstl::max(sliceNear, 0.000001f), sliceFar);
        mathstl::Matrix viewProj = view * projMat;
        auto viewProjInv = viewProj.Invert();

        mathstl::Vector3 frustumCornersWS[8];
        for (u32 j = 0; j < 8; j++)
        {
            frustumCornersWS[j] = mathstl::Vector3::Transform(ndcCorners[j], viewProjInv);
        }

        mathstl::Vector3 center = mathstl::Vector3::Zero;
        for (u32 j = 0; j < 8; j++)
            center += frustumCornersWS[j];
        center *= (1.0f / 8.0f);

        const auto radius = (frustumCornersWS[0] - frustumCornersWS[6]).Length() * 0.5f;
        // Create a temporary view matrix to transform center to light space
        mathstl::Vector3 eye = -(lightDirection);
        mathstl::Matrix lightView = mathstl::Matrix::CreateLookAt(mathstl::Vector3(0, 0, 0), eye, up);

        float shadowMapSizef = static_cast<float>(shadowMapSize);
        float worldUnitsPerTexel = (shadowMapSizef / (2.0f * radius));
        lightView = mathstl::Matrix::CreateScale(worldUnitsPerTexel) * lightView;
        // Transform center to light view space
        mathstl::Vector3 centerLightSpace = mathstl::Vector3::Transform(center, lightView);

        // Snap to texel grid
        centerLightSpace.x = std::floor(centerLightSpace.x);
        centerLightSpace.y = std::floor(centerLightSpace.y);

        center = mathstl::Vector3::Transform(centerLightSpace, lightView.Invert());

        // Re-create view matrix with snapped center
        eye = center - (lightDirection * radius * 2.0f);
        lightView = mathstl::Matrix::CreateLookAt(eye, center, up);

        mathstl::Matrix lightProj =
            mathstl::Matrix::CreateOrthographicOffCenter(-radius, radius, -radius, radius, -radius * 3, radius * 3);

        lightViewProjMatrices[i] = lightView * lightProj;
        lastSplitDist = splitDist;
    }

    // Fill split depths for the shader (absolute view-space distances)
    for (u32 i = 0; i < cascades; ++i)
    {
        splits[i] = cascadeSplits[i];
    }
    for (u32 i = cascades; i < 16; ++i)
    {
        splits[i] = FLT_MAX;
    }

    return lightViewProjMatrices;
}

} // namespace RenderPasses
