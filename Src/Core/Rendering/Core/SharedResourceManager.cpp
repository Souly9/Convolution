#include "SharedResourceManager.h"
#include "../Passes/Utils/RenderPassUtils.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/MaterialManager.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Passes/PassManager.h"
#include "Defines/GlobalBuffers.h"
#include "Utils/DescriptorLayoutUtils.h"
#include "Utils/GeometryBufferBuildUtils.h"

void SharedResourceManager::Init()
{
    m_bufferUpdateMutex.lock();
    DescriptorPoolCreateInfo info{};
    info.enableBindlessTextureDescriptors = false;
    info.enableStorageBufferDescriptors = true;
    m_descriptorPool.Create(info);

    u64 transBufferSize = UBO::GlobalTransformSSBOSize;

    m_sceneInstanceBuffer = StorageBuffer(UBO::GlobalPerObjectDataSSBOSize, true);
    m_materialBuffer = StorageBuffer(UBO::GlobalMaterialSSBOSize, true);
    m_transformBuffer = StorageBuffer(transBufferSize, true);

    m_sceneInstanceBuffer.SetName("Scene Instance SSBO");
    m_materialBuffer.SetName("Material SSBO");
    m_transformBuffer.SetName("Transform SSBO");

    m_sceneInstanceSSBOLayout = DescriptorLaytoutUtils::CreateOneDescriptorSetForAll(
        {PipelineDescriptorLayout(UBO::BufferType::TransformSSBO),
         PipelineDescriptorLayout(UBO::BufferType::GlobalObjectDataSSBOs),
         PipelineDescriptorLayout(UBO::BufferType::InstanceDataSSBO)});
    m_frameData.resize(FRAMES_IN_FLIGHT);
    m_currentFrameInstanceData.reserve(MAX_ENTITIES);
    UBO::MaterialBuffer materialBuffer{};
    for (u32 i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        m_frameData[i].pSceneInstanceSSBOSet = m_descriptorPool.CreateDescriptorSet(m_sceneInstanceSSBOLayout);
        m_frameData[i].pSceneInstanceSSBOSet->SetBindingSlot(
            UBO::s_UBOTypeToBindingSlot[UBO::BufferType::TransformSSBO]);
        {
            AsyncQueueHandler::SSBOTransfer transfer{.data = m_currentFrameInstanceData.data(),
                                                     .size = MAX_ENTITIES * sizeof(m_currentFrameInstanceData[0]),
                                                     .offset = 0,
                                                     .pDescriptorSet = m_frameData[i].pSceneInstanceSSBOSet,
                                                     .pStorageBuffer = &m_sceneInstanceBuffer,
                                                     .dstBinding = s_globalInstanceDataSSBOSlot};
            g_pQueueHandler->SubmitTransferCommandAsync(transfer);
        }
        {
            AsyncQueueHandler::SSBOTransfer transfer{.data = (void*)materialBuffer.data(),
                                                     .size = UBO::GlobalMaterialSSBOSize,
                                                     .offset = 0,
                                                     .pDescriptorSet = m_frameData[i].pSceneInstanceSSBOSet,
                                                     .pStorageBuffer = &m_materialBuffer,
                                                     .dstBinding = s_globalMaterialBufferSlot};
            g_pQueueHandler->SubmitTransferCommandAsync(transfer);
        }
    }
    m_bufferUpdateMutex.unlock();
}

void SharedResourceManager::UploadSceneGeometry(const stltype::vector<stltype::unique_ptr<Mesh>>& meshes)
{
    ScopedZone("SharedResourceManager::UploadSceneGeometry");
    m_bufferUpdateMutex.lock();

    // Just see how much memory we need
    m_bufferOffsetData.vertexCount = 0;
    m_bufferOffsetData.indexCount = 0;
    m_bufferOffsetData.instanceCount = 0;
    for (const auto& pMesh : meshes)
    {
        m_bufferOffsetData.vertexCount += pMesh->vertices.size();
        m_bufferOffsetData.indexCount += pMesh->indices.size();
    }

    AsyncQueueHandler::MeshTransfer cmd{};
    stltype::vector<CompleteVertex>& vertices = cmd.vertices;
    vertices.reserve(m_bufferOffsetData.vertexCount);
    stltype::vector<u32> indices = cmd.indices;
    indices.reserve(m_bufferOffsetData.indexCount);
    cmd.pBuffersToFill = &m_sceneGeometryBuffers;

    m_bufferOffsetData.vertBufferOffset = 0;
    m_bufferOffsetData.indexBufferOffset = 0;
    m_bufferOffsetData.instanceBufferIdx = 0;
    m_meshHandles.clear();
    m_meshHandles.reserve(meshes.size());

    u32 instanceCount = 0;
    for (const auto& pMesh : meshes)
    {
        if (const auto& it = m_meshHandles.find(pMesh.get()); it != m_meshHandles.end())
        {
            continue;
        }
        MeshResourceData meshData{};
        meshData.indexBufferOffset = m_bufferOffsetData.indexBufferOffset;
        meshData.vertBufferOffset = m_bufferOffsetData.vertBufferOffset;
        meshData.indexCount = pMesh->indices.size();
        meshData.vertCount = pMesh->vertices.size();

        Utils::GenerateDrawCommandForMesh(
            *pMesh.get(), m_bufferOffsetData.indexBufferOffset, cmd.vertices, cmd.indices);
        m_bufferOffsetData.indexBufferOffset += pMesh->indices.size();
        m_bufferOffsetData.vertBufferOffset += pMesh->vertices.size();

        m_meshHandles[pMesh.get()] = meshData;
    }

    g_pQueueHandler->SubmitTransferCommandAsync(cmd);

    m_bufferUpdateMutex.unlock();
}

void SharedResourceManager::UpdateInstanceDataSSBO(stltype::vector<RenderPasses::PassMeshData>& meshes,
                                                   u32 thisFrameNum)
{
    ScopedZone("SharedResourceManager::UpdateInstanceDataSSBO");
    m_bufferUpdateMutex.lock();

    auto& instanceData = m_currentFrameInstanceData;
    instanceData.clear();
    instanceData.reserve(m_bufferOffsetData.instanceCount);

    for (auto& meshData : meshes)
    {
        auto& data = instanceData.emplace_back();
        data.drawData = m_meshHandles.at(meshData.meshData.pMesh); // If this fails we have a problem either way
        data.aabbCenterTransIdx = mathstl::Vector4(meshData.meshData.aabb.center);
        data.aabbExtentsMatIdx = mathstl::Vector4(meshData.meshData.aabb.extents);
        data.SetMaterialIdx(g_pMaterialManager->GetMaterialIdx(meshData.meshData.pMaterial));
        data.SetTransformIdx(meshData.transformIdx);

        meshData.meshData.meshResourceHandle = data.drawData;
        meshData.meshData.instanceDataIdx = (u32)instanceData.size() - 1;
    }
    auto pDescriptor = m_frameData[thisFrameNum].pSceneInstanceSSBOSet;
    AsyncQueueHandler::SSBOTransfer transfer{.data = m_currentFrameInstanceData.data(),
                                             .size = m_currentFrameInstanceData.size() *
                                                     sizeof(m_currentFrameInstanceData[0]),
                                             .offset = 0,
                                             .pDescriptorSet = pDescriptor,
                                             .pStorageBuffer = &m_sceneInstanceBuffer,
                                             .dstBinding = s_globalInstanceDataSSBOSlot};
    g_pQueueHandler->SubmitTransferCommandAsync(transfer);
    g_pQueueHandler->DispatchAllRequests();
    m_bufferUpdateMutex.unlock();
}

MeshHandle SharedResourceManager::UploadMesh(const Mesh& mesh)
{
    return {};
}

MeshHandle SharedResourceManager::GetMeshHandle(const Mesh* pMesh) const
{
    return m_meshHandles.find(pMesh)->second;
}

void SharedResourceManager::WriteInstanceSSBODescriptorUpdate(u32 targetFrame)
{
    m_frameData[targetFrame].pSceneInstanceSSBOSet->WriteSSBOUpdate(m_transformBuffer, s_modelSSBOBindingSlot);
    m_frameData[targetFrame].pSceneInstanceSSBOSet->WriteSSBOUpdate(m_sceneInstanceBuffer,
                                                                    s_globalInstanceDataSSBOSlot);
    m_frameData[targetFrame].pSceneInstanceSSBOSet->WriteSSBOUpdate(m_materialBuffer, s_globalMaterialBufferSlot);
}

void SharedResourceManager::UpdateTransformBuffer(const stltype::vector<DirectX::XMFLOAT4X4>& transformBuffer,
                                                  u32 thisFrame)
{
    auto pDescriptor = m_frameData[thisFrame].pSceneInstanceSSBOSet;
    AsyncQueueHandler::SSBOTransfer transfer{.data = (void*)transformBuffer.data(),
                                             .size = UBO::GlobalTransformSSBOSize,
                                             .offset = 0,
                                             .pDescriptorSet = pDescriptor,
                                             .pStorageBuffer = &m_transformBuffer,
                                             .dstBinding = s_modelSSBOBindingSlot};
    g_pQueueHandler->SubmitTransferCommandAsync(transfer);
}

void SharedResourceManager::UpdateGlobalMaterialBuffer(const UBO::MaterialBuffer& materialBuffer, u32 thisFrame)
{
    auto pDescriptor = m_frameData[thisFrame].pSceneInstanceSSBOSet;
    AsyncQueueHandler::SSBOTransfer transfer{.data = (void*)materialBuffer.data(),
                                             .size = UBO::GlobalMaterialSSBOSize,
                                             .offset = 0,
                                             .pDescriptorSet = pDescriptor,
                                             .pStorageBuffer = &m_materialBuffer,
                                             .dstBinding = s_globalMaterialBufferSlot};
    g_pQueueHandler->SubmitTransferCommandAsync(transfer);
}
