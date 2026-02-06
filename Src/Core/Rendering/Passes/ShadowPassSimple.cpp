#include "ShadowPass.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/RenderingTypeDefs.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Vulkan/VkGlobals.h"
#include "EASTL/algorithm.h"
#include "SimpleMath/SimpleMath.h"
#include "Utils/RenderPassUtils.h"

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
    m_indirectCmdBuffer = IndirectDrawCommandBuffer(1000);

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
    // Derive layer mask from current cascade count
    cmdBegin.depthLayerMask = 0;
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
    pCmdBuffer->RecordCommand(cmdBegin);
    BinRenderDataCmd geomBufferCmd(sceneGeometryBuffers.GetVertexBuffer(), sceneGeometryBuffers.GetIndexBuffer());
    pCmdBuffer->RecordCommand(geomBufferCmd);

    const auto mainCamView = data.mainView;
    stltype::vector<stltype::vector<mathstl::Matrix>> matrices;
    matrices.reserve(data.csmViews.size());
    if (data.csmViews.empty() == false)
    {
        const auto& csmView = data.csmViews[0];
        stltype::array<f32, 16> splits{};
        UBO::ShadowmapViewUBO uboData;
        uboData.lightViewProjMatrices = ComputeLightViewProjMatrices(data.cascades,
                                                                    mainCamView.fov,
                                                                    ctx.zNear,
                                                                    ctx.zFar,
                                                                    mathstl::Vector2(extents.x, extents.y),
                                               csmView.dir,
                                               splits,
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
    }

    pCmdBuffer->RecordCommand(EndRenderingCmd{});
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

stltype::array<mathstl::Matrix, 16> CSMPass::ComputeLightViewProjMatrices(u32 cascades,
                                                                          f32 fov,
                                                                          f32 mainCamNear,
                                                                          f32 mainCamFar,
                                                                          mathstl::Vector2 mainFrustumExtents,
                                                                          const mathstl::Vector3& lightDir,
                                                                          stltype::array<f32, 16>& splits,
                                                                          u32 shadowMapSize)
{
    stltype::array<mathstl::Matrix, 16> lightViewProjMatrices;
    
    auto lightLocalDir = lightDir;
    lightLocalDir.Normalize();

    // 1. Calculate Frustum Center
    // (Unused camera math removed)

    mathstl::Vector3 target = mathstl::Vector3(0.0f, 0.0f, 0.0f);

    // 2. Light View Matrix
    // Look at the frustum center
    // Eye is backed up along the light direction
    float standoff = 300.0f; 
    mathstl::Vector3 eye = target - lightLocalDir * standoff;

    auto upVec = mathstl::Vector3(0.0f, 1.0f, 0.0f);
    // If light is parallel to up, pick another up
    if (std::abs(lightLocalDir.Dot(upVec)) > 0.99f)
        upVec = mathstl::Vector3(0.0f, 0.0f, 1.0f);
    
    // Create View Matrix
    const auto lightView = mathstl::Matrix::CreateLookAt(eye, target, upVec);

    // 3. Orthographic Projection
    // "Same view area" - we'll try to match a reasonable viewport size at a distance
    // Since we can't match perspective with ortho, we'll make the box large enough
    float boxSize = 25.0f; 
    float minX = -boxSize;
    float maxX = boxSize;
    float minY = -boxSize;
    float maxY = boxSize;
    
    // Z-Range
    // We need to see everything from the eye to past the target
    // Standoff is distance to target. We need to go past target by at least boxSize or similar
    float nearPlane = 0.1f; 
    float farPlane = standoff + 400.0f; // Sufficient depth

    auto lightProj = mathstl::Matrix::CreateOrthographicOffCenter(minX, maxX, minY, maxY, nearPlane, farPlane);

    // 4. Fix Upside-Down Shadows (Vulkan Clip Space Y-Flip)
    lightProj._22 *= -1.0f;
    //lightProj._11 *= -1.0f;
    // Combine View * Proj
    lightViewProjMatrices[0] = lightView * lightProj;

    // Fill dummy splits for shader compatibility
    splits.fill(farPlane); 

    return lightViewProjMatrices;
}

/*
// REMOVED: ComputeFrustumCornersWS is no longer needed but kept in class for now if interface demands it, 
// otherwise we can leave it empty or unused.
*/
stltype::vector<mathstl::Vector4> CSMPass::ComputeFrustumCornersWS(const mathstl::Matrix& proj,
                                                                   const mathstl::Matrix& view)
{
    // Unused in Static Map mode
    return {};
}


} // namespace RenderPasses
