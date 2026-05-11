#include "SharedResourceManager.h"

#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/LogDefines.h"
#include "Core/Rendering/Core/MaterialManager.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Passes/PassManager.h"
#include "Defines/GlobalBuffers.h"
#include "Core/Rendering/Vulkan/Utils/VkDescriptorLayoutUtils.h"
#include "Utils/GeometryBufferBuildUtils.h"

void SharedResourceManager::UploadDebugMesh(const Mesh& mesh, u32 thisFrame)
{
    ScopedZone("SharedResourceManager::UploadDebugMesh");
    DEBUG_LOGF("SharedResourceManager: Uploading debug mesh. Vertices: {}, Indices: {}", (u32)mesh.vertices.size(), (u32)mesh.indices.size());
    AsyncQueueHandler::MeshTransfer cmd{};
    cmd.vertices.reserve(mesh.vertices.size());
    cmd.indices.reserve(mesh.indices.size());
    cmd.pBuffersToFill = &m_debugGeometryBuffers;

    u64 vertexBaseOffset = 0;

    {
        SimpleScopedGuard lock(m_geometryStateMutex);

        vertexBaseOffset = m_debugBufferOffsetData.vertBufferOffset;
        cmd.vertexOffset = m_debugBufferOffsetData.vertBufferOffset * sizeof(CompleteVertex);
        cmd.indexOffset = m_debugBufferOffsetData.indexBufferOffset * sizeof(u32);

        MeshResourceData meshData{};
        meshData.indexBufferOffset = m_debugBufferOffsetData.indexBufferOffset;
        meshData.vertBufferOffset = m_debugBufferOffsetData.vertBufferOffset;
        meshData.indexCount = mesh.indices.size();
        meshData.vertCount = mesh.vertices.size();

        m_debugBufferOffsetData.indexBufferOffset += mesh.indices.size();
        m_debugBufferOffsetData.vertBufferOffset += mesh.vertices.size();

        m_debugMeshHandles[&mesh] = meshData;
    }
    Utils::GenerateDrawCommandForMesh(mesh, vertexBaseOffset, cmd.vertices, cmd.indices);
    cmd.frameIdx = thisFrame;

    const Mesh* pMeshPtr = &mesh;
    cmd.onComplete = [this, pMeshPtr]()
                     {
                         DEBUG_LOG("SharedResourceManager: Debug mesh transfer completed. Marking resident.");
                         SimpleScopedGuard lock(m_residencyStateMutex);
                         if (m_residentMeshes.insert(pMeshPtr).second)
                         {
                             m_pendingVisibleMeshes.push_back(pMeshPtr);
                         }
                     };

    g_pQueueHandler->SubmitTransferCommandAsync(cmd);
}

void SharedResourceManager::Init()
{
    ScopedZone("SharedResourceManager::Init");
    DescriptorPoolCreateInfo info{};
    info.enableBindlessTextureDescriptors = false;
    info.enableStorageBufferDescriptors = true;
    m_descriptorPool.Create(info);

    m_debugBufferOffsetData.vertBufferOffset = 0;
    m_debugBufferOffsetData.indexBufferOffset = 0;

    u64 transBufferSize = UBO::GlobalTransformSSBOSize;

    m_sceneInstanceBuffer = StorageBuffer(UBO::GlobalPerObjectDataSSBOSize, true);
    m_materialBuffer = StorageBuffer(UBO::GlobalMaterialSSBOSize, true);
    m_transformBuffer = StorageBuffer(transBufferSize, true);
    m_prevTransformBuffer = StorageBuffer(transBufferSize, true);
    m_sceneAABBBuffer = StorageBuffer(UBO::GlobalAABBSSBOSize, true);
    m_viewSpaceLightsSSBO = StorageBuffer(UBO::ViewSpaceLightsSSBOSize, true);

    m_sceneInstanceBuffer.SetName("Scene Instance SSBO");
    m_materialBuffer.SetName("Material SSBO");
    m_transformBuffer.SetName("Transform SSBO");
    m_prevTransformBuffer.SetName("Prev Transform SSBO");
    m_sceneAABBBuffer.SetName("Scene AABB SSBO");
    m_viewSpaceLightsSSBO.SetName("View Space Lights SSBO");

    m_sceneInstanceSSBOLayout = DescriptorLayoutUtils::CreateOneDescriptorSetForAll(
        {PipelineDescriptorLayout(UBO::BufferType::TransformSSBO),
         PipelineDescriptorLayout(UBO::BufferType::PrevTransformSSBO),
         PipelineDescriptorLayout(UBO::BufferType::GlobalObjectDataSSBOs),
         PipelineDescriptorLayout(UBO::BufferType::InstanceDataSSBO)});
    
    m_sceneAABBLayout = DescriptorLayoutUtils::CreateOneDescriptorSetForAll(
        {PipelineDescriptorLayout(UBO::BufferType::SceneAABBsSSBO)});
    
    m_viewSpaceLightsLayout = DescriptorLayoutUtils::CreateOneDescriptorSetForAll(
        {PipelineDescriptorLayout(UBO::BufferType::ViewSpaceLightsSSBO)});
    
    m_frameData.resize(FRAMES_IN_FLIGHT);
    m_currentFrameInstanceData.reserve(MAX_ENTITIES);
    UBO::MaterialBuffer materialBuffer{};
    for (u32 i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        m_frameData[i].pSceneInstanceSSBOSet = m_descriptorPool.CreateDescriptorSet(m_sceneInstanceSSBOLayout);
        m_frameData[i].pSceneInstanceSSBOSet->SetBindingSlot(
            UBO::s_UBOTypeToBindingSlot[UBO::BufferType::TransformSSBO]);

        // Initialize all descriptor bindings directly
        // Binding 1: Transform Matrix
        m_frameData[i].pSceneInstanceSSBOSet->WriteSSBOUpdate(m_transformBuffer, s_modelSSBOBindingSlot);
        m_frameData[i].pSceneInstanceSSBOSet->WriteSSBOUpdate(m_prevTransformBuffer, s_prevModelSSBOBindingSlot);
        // Binding 2: Material Data
        m_frameData[i].pSceneInstanceSSBOSet->WriteSSBOUpdate(m_materialBuffer, s_globalMaterialBufferSlot);
        // Binding 3: Instance Data
        m_frameData[i].pSceneInstanceSSBOSet->WriteSSBOUpdate(m_sceneInstanceBuffer, s_globalInstanceDataSSBOSlot);
        
        // AABB Set
        m_frameData[i].pSceneAABBSet = m_descriptorPool.CreateDescriptorSet(m_sceneAABBLayout);
        m_frameData[i].pSceneAABBSet->SetBindingSlot(UBO::s_UBOTypeToBindingSlot[UBO::BufferType::SceneAABBsSSBO]);
        
        // Binding 4: Scene AABBs
        m_frameData[i].pSceneAABBSet->WriteSSBOUpdate(m_sceneAABBBuffer, s_sceneAABBsSSBOBindingSlot);
        
        // View Space Lights Set
        m_frameData[i].pViewSpaceLightsSet = m_descriptorPool.CreateDescriptorSet(m_viewSpaceLightsLayout);
        m_frameData[i].pViewSpaceLightsSet->SetBindingSlot(UBO::s_UBOTypeToBindingSlot[UBO::BufferType::ViewSpaceLightsSSBO]);
        m_frameData[i].pViewSpaceLightsSet->WriteSSBOUpdate(m_viewSpaceLightsSSBO, s_viewSpaceLightsSSBOBindingSlot);
    }
}

void SharedResourceManager::UploadSceneGeometry(const stltype::vector<stltype::unique_ptr<Mesh>>& meshes)
{
    ScopedZone("SharedResourceManager::UploadSceneGeometry");
    {
        SimpleScopedGuard lock(m_residencyStateMutex);
        m_residentMeshes.clear();
        m_pendingVisibleMeshes.clear();
        m_pendingRayTracingMeshes.clear();
        m_meshToInstanceIdx.clear();
    }

    u64 vertexCount = 0;
    u64 indexCount = 0;
    for (const auto& pMesh : meshes)
    {
        vertexCount += pMesh->vertices.size();
        indexCount += pMesh->indices.size();
    }
    DEBUG_LOGF("SharedResourceManager: Uploading scene geometry. Total vertices: {}, Total indices: {}, Mesh count: {}", 
               (u32)vertexCount, (u32)indexCount, (u32)meshes.size());

    AsyncQueueHandler::MeshTransfer cmd{};
    stltype::vector<CompleteVertex>& vertices = cmd.vertices;
    vertices.reserve(vertexCount);
    stltype::vector<u32> indices = cmd.indices;
    indices.reserve(indexCount);
    cmd.pBuffersToFill = &m_sceneGeometryBuffers;

    {
        SimpleScopedGuard lock(m_geometryStateMutex);

        m_bufferOffsetData.vertexCount = vertexCount;
        m_bufferOffsetData.indexCount = indexCount;
        m_bufferOffsetData.vertBufferOffset = 0;
        m_bufferOffsetData.indexBufferOffset = 0;
        m_meshHandles.clear();
        m_meshHandles.reserve(meshes.size());

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
                *pMesh.get(), m_bufferOffsetData.vertBufferOffset, cmd.vertices, cmd.indices);
            m_bufferOffsetData.indexBufferOffset += pMesh->indices.size();
            m_bufferOffsetData.vertBufferOffset += pMesh->vertices.size();

            m_meshHandles[pMesh.get()] = meshData;
        }
    }
    cmd.frameIdx = 0;
    
    stltype::vector<const Mesh*> meshPtrs;
    meshPtrs.reserve(meshes.size());
    for (const auto& pMesh : meshes)
    {
        meshPtrs.push_back(pMesh.get());
    }

    cmd.onComplete = [this, meshPtrs = stltype::move(meshPtrs)]()
                     {
                         DEBUG_LOG("SharedResourceManager: Scene geometry transfer completed. Marking meshes resident.");
                         SimpleScopedGuard lock(m_residencyStateMutex);
                         for (const auto* pMesh : meshPtrs)
                         {
                             if (m_residentMeshes.insert(pMesh).second)
                             {
                                 m_pendingVisibleMeshes.push_back(pMesh);
                                 m_pendingRayTracingMeshes.push_back(pMesh);
                             }
                         }
                     };

    g_pQueueHandler->SubmitTransferCommandAsync(cmd);
}

void SharedResourceManager::UpdateInstanceDataSSBO(stltype::vector<RenderPasses::PassMeshData>& meshes,
                                                   u32 thisFrameNum)
{
    ScopedZone("SharedResourceManager::UpdateInstanceDataSSBO");
    auto& instanceData = m_currentFrameInstanceData;
    instanceData.clear();
    instanceData.reserve(meshes.size());

    {
        SimpleScopedGuard lock(m_residencyStateMutex);
        m_meshToInstanceIdx.clear();
    }

    for (auto& meshData : meshes)
    {
        auto& data = instanceData.emplace_back();
        MeshHandle handle;

        if (meshData.meshData.IsDebugMesh())
        {
            bool hasHandle = false;
            {
                SimpleScopedGuard lock(m_geometryStateMutex);
                if (const auto& it = m_debugMeshHandles.find(meshData.meshData.pMesh); it != m_debugMeshHandles.end())
                {
                    handle = it->second;
                    hasHandle = true;
                }
            }

            if (!hasHandle)
            {
                UploadDebugMesh(*meshData.meshData.pMesh, thisFrameNum);
                SimpleScopedGuard lock(m_geometryStateMutex);
                handle = m_debugMeshHandles.at(meshData.meshData.pMesh);
            }
        }
        else
        {
            SimpleScopedGuard lock(m_geometryStateMutex);
            handle = m_meshHandles.at(meshData.meshData.pMesh); // If this fails we have a problem either way
        }

        data.drawData = handle;
        data.aabbCenterTransIdx = mathstl::Vector4(meshData.meshData.aabb.center);
        data.aabbExtentsMatIdx = mathstl::Vector4(meshData.meshData.aabb.extents);
        data.SetMaterialIdx(g_pMaterialManager->GetMaterialIdx(meshData.meshData.pMaterial));
        data.SetTransformIdx(meshData.transformIdx);

        meshData.meshData.meshResourceHandle = data.drawData;
        meshData.meshData.instanceDataIdx = (u32)instanceData.size() - 1;

        // Visibility set from residency
        {
            SimpleScopedGuard lock(m_residencyStateMutex);
            data.SetVisible(m_residentMeshes.count(meshData.meshData.pMesh) > 0);
            m_meshToInstanceIdx[meshData.meshData.pMesh].push_back(meshData.meshData.instanceDataIdx);
        }
    }
    AsyncQueueHandler::SSBOTransfer transfer;
    transfer.pData = m_currentFrameInstanceData.data();
    transfer.size = static_cast<u32>(m_currentFrameInstanceData.size() * sizeof(m_currentFrameInstanceData[0]));
    transfer.offset = 0;
    transfer.pDescriptor = nullptr;
    transfer.pSSBO = &m_sceneInstanceBuffer;
    transfer.dstBinding = s_globalInstanceDataSSBOSlot;
    transfer.frameIdx = thisFrameNum;
    DEBUG_LOGF("SharedResourceManager: Updating instance data SSBO. Entry count: {}, Size: {} bytes", 
               (u32)m_currentFrameInstanceData.size(), transfer.size);
    g_pQueueHandler->SubmitTransferCommandAsync(transfer);
    g_pQueueHandler->DispatchAllRequests();
}

MeshHandle SharedResourceManager::UploadMesh(const Mesh& mesh)
{
    return {};
}

MeshHandle SharedResourceManager::GetMeshHandle(const Mesh* pMesh) const
{
    SimpleScopedGuard lock(m_geometryStateMutex);
    return m_meshHandles.find(pMesh)->second;
}

void SharedResourceManager::WriteInstanceSSBODescriptorUpdate(u32 targetFrame)
{
    ScopedZone("SharedResourceManager::WriteInstanceSSBODescriptorUpdate");
    targetFrame %= m_frameData.size();
    m_frameData[targetFrame].pSceneInstanceSSBOSet->WriteSSBOUpdate(m_transformBuffer, s_modelSSBOBindingSlot);
    m_frameData[targetFrame].pSceneInstanceSSBOSet->WriteSSBOUpdate(m_prevTransformBuffer, s_prevModelSSBOBindingSlot);
    m_frameData[targetFrame].pSceneInstanceSSBOSet->WriteSSBOUpdate(m_sceneInstanceBuffer,
                                                                    s_globalInstanceDataSSBOSlot);
    m_frameData[targetFrame].pSceneAABBSet->WriteSSBOUpdate(m_sceneAABBBuffer, s_sceneAABBsSSBOBindingSlot);
    m_frameData[targetFrame].pSceneInstanceSSBOSet->WriteSSBOUpdate(m_materialBuffer, s_globalMaterialBufferSlot);
}

void SharedResourceManager::UpdateTransformBuffer(const stltype::vector<DirectX::XMFLOAT4X4>& transformBuffer,
                                                  u32 thisFrame,
                                                  u32 updateCount)
{
    ScopedZone("SharedResourceManager::UpdateTransformBuffer");
    u64 transferSize = updateCount > 0 ? updateCount * sizeof(DirectX::XMFLOAT4X4) : transformBuffer.size() * sizeof(DirectX::XMFLOAT4X4);
    if (transferSize == 0) return;
    
    AsyncQueueHandler::SSBOTransfer transfer;
    transfer.pData = (void*)transformBuffer.data();
    transfer.size = static_cast<u32>(transferSize);
    transfer.offset = 0;
    transfer.pDescriptor = nullptr;
    transfer.pSSBO = &m_transformBuffer;
    transfer.dstBinding = s_modelSSBOBindingSlot;
    transfer.frameIdx = thisFrame;
    g_pQueueHandler->SubmitTransferCommandAsync(transfer);
}

void SharedResourceManager::UpdateTransformRange(const stltype::vector<DirectX::XMFLOAT4X4>& transformBuffer,
                                                 u32 startIdx,
                                                 u32 count,
                                                 u32 thisFrame)
{
    ScopedZone("SharedResourceManager::UpdateTransformRange");
    if (count == 0) return;
    const u64 offset = startIdx * sizeof(DirectX::XMFLOAT4X4);
    const u64 transferSize = count * sizeof(DirectX::XMFLOAT4X4);
    AsyncQueueHandler::SSBOTransfer transfer;
    transfer.pData = (void*)&transformBuffer[startIdx];
    transfer.size = static_cast<u32>(transferSize);
    transfer.offset = offset;
    transfer.pDescriptor = nullptr;
    transfer.pSSBO = &m_transformBuffer;
    transfer.dstBinding = s_modelSSBOBindingSlot;
    transfer.frameIdx = thisFrame;
    g_pQueueHandler->SubmitTransferCommandAsync(transfer);
}

void SharedResourceManager::UpdatePrevTransformRange(const stltype::vector<DirectX::XMFLOAT4X4>& transformBuffer,
                                                     u32 startIdx,
                                                     u32 count,
                                                     u32 thisFrame)
{
    ScopedZone("SharedResourceManager::UpdatePrevTransformRange");
    if (count == 0) return;
    const u64 offset = startIdx * sizeof(DirectX::XMFLOAT4X4);
    const u64 transferSize = count * sizeof(DirectX::XMFLOAT4X4);
    AsyncQueueHandler::SSBOTransfer transfer;
    transfer.pData = (void*)&transformBuffer[startIdx];
    transfer.size = static_cast<u32>(transferSize);
    transfer.offset = offset;
    transfer.pDescriptor = nullptr;
    transfer.pSSBO = &m_prevTransformBuffer;
    transfer.dstBinding = s_prevModelSSBOBindingSlot;
    transfer.frameIdx = thisFrame;
    g_pQueueHandler->SubmitTransferCommandAsync(transfer);
}

void SharedResourceManager::UpdateSceneAABBBuffer(const stltype::vector<AABB>& aabbBuffer, u32 thisFrame, u32 updateCount)
{
    ScopedZone("SharedResourceManager::UpdateSceneAABBBuffer");
    u64 transferSize = updateCount > 0 ? updateCount * sizeof(AABB) : aabbBuffer.size() * sizeof(AABB);
    if (transferSize == 0) return;

    AsyncQueueHandler::SSBOTransfer transfer;
    transfer.pData = (void*)aabbBuffer.data();
    transfer.size = static_cast<u32>(transferSize);
    transfer.offset = 0;
    transfer.pDescriptor = nullptr;
    transfer.pSSBO = &m_sceneAABBBuffer;
    transfer.dstBinding = s_sceneAABBsSSBOBindingSlot;
    transfer.frameIdx = thisFrame;
    g_pQueueHandler->SubmitTransferCommandAsync(transfer);
}

void SharedResourceManager::UpdateSceneAABBRange(const stltype::vector<AABB>& aabbBuffer,
                                                 u32 startIdx,
                                                 u32 count,
                                                 u32 thisFrame)
{
    ScopedZone("SharedResourceManager::UpdateSceneAABBRange");
    if (count == 0) return;
    const u64 offset = startIdx * sizeof(AABB);
    const u64 transferSize = count * sizeof(AABB);
    AsyncQueueHandler::SSBOTransfer transfer;
    transfer.pData = (void*)&aabbBuffer[startIdx];
    transfer.size = static_cast<u32>(transferSize);
    transfer.offset = offset;
    transfer.pDescriptor = nullptr;
    transfer.pSSBO = &m_sceneAABBBuffer;
    transfer.dstBinding = s_sceneAABBsSSBOBindingSlot;
    transfer.frameIdx = thisFrame;
    g_pQueueHandler->SubmitTransferCommandAsync(transfer);
}

void SharedResourceManager::UpdateGlobalMaterialBuffer(const UBO::MaterialBuffer& materialBuffer, u32 thisFrame)
{
    ScopedZone("SharedResourceManager::UpdateGlobalMaterialBuffer");
    u32 materialCount = (u32)materialBuffer.size() > 0 ? (u32)materialBuffer.size() : 1;
    u32 byteSize = materialCount * sizeof(Material);
    
    AsyncQueueHandler::SSBOTransfer transfer;
    transfer.pData = (void*)materialBuffer.data();
    transfer.size = byteSize;
    transfer.offset = 0;
    transfer.pDescriptor = nullptr;
    transfer.pSSBO = &m_materialBuffer;
    transfer.dstBinding = s_globalMaterialBufferSlot;
    transfer.frameIdx = thisFrame;
    g_pQueueHandler->SubmitTransferCommandAsync(transfer);
}

stltype::vector<u32> SharedResourceManager::PopPendingVisibleInstanceIndices()
{
    ScopedZone("SharedResourceManager::PopPendingVisibleInstanceIndices");
    stltype::vector<u32> indices;
    stltype::hash_set<u32> seenInstanceIndices;
    SimpleScopedGuard lock(m_residencyStateMutex);
    for (const auto* pMesh : m_pendingVisibleMeshes)
    {
        auto it = m_meshToInstanceIdx.find(pMesh);
        if (it != m_meshToInstanceIdx.end())
        {
            for (u32 instanceIdx : it->second)
            {
                if (seenInstanceIndices.insert(instanceIdx).second)
                {
                    indices.push_back(instanceIdx);
                }
            }
        }
    }
    m_pendingVisibleMeshes.clear();
    return indices;
}

stltype::vector<const Mesh*> SharedResourceManager::PopPendingResidentMeshesForRayTracing()
{
    ScopedZone("SharedResourceManager::PopPendingResidentMeshesForRayTracing");
    SimpleScopedGuard lock(m_residencyStateMutex);
    stltype::vector<const Mesh*> meshes = stltype::move(m_pendingRayTracingMeshes);
    m_pendingRayTracingMeshes.clear();
    return meshes;
}
