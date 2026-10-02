#include "SharedResourceManager.h"

#include "Core/Global/GlobalDefines.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/LogDefines.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/Rendering/Core/MaterialManager.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Passes/PassManager.h"
#include "Defines/GlobalBuffers.h"
#include "Core/Rendering/Core/DescriptorUtils/DescriptorLayoutUtils.h"

MeshHandle SharedResourceManager::AppendMesh(const Mesh& mesh,
                                             BufferData& buffers,
                                             BufferStats& offsets,
                                             stltype::hash_map<const Mesh*, MeshHandle>& handles)
{
    AsyncQueueHandler::MeshTransfer transfer{};
    transfer.pMesh = &mesh;
    transfer.pBuffersToFill = &buffers;
    transfer.vertexOffset = offsets.vertBufferOffset * sizeof(CompleteVertex);
    transfer.indexOffset = offsets.indexBufferOffset * sizeof(u32);
    DEBUG_ASSERT(transfer.vertexOffset + mesh.vertices.size() * sizeof(CompleteVertex) <=
                 buffers.GetVertexBuffer().GetInfo().size);
    DEBUG_ASSERT(transfer.indexOffset + mesh.indices.size() * sizeof(u32) <= buffers.GetIndexBuffer().GetInfo().size);
    g_renderer.GetQueueHandler().SubmitTransferCommandAsync(transfer);

    MeshResourceData meshData{};
    meshData.indexBufferOffset = offsets.indexBufferOffset;
    meshData.vertBufferOffset = offsets.vertBufferOffset;
    meshData.indexCount = mesh.indices.size();
    meshData.vertCount = mesh.vertices.size();
    offsets.indexBufferOffset += mesh.indices.size();
    offsets.vertBufferOffset += mesh.vertices.size();
    handles[&mesh] = meshData;
    return meshData;
}

void SharedResourceManager::Init()
{
    ScopedZone("SharedResourceManager::Init");
    DescriptorPoolCreateInfo info{};
    info.enableBindlessTextureDescriptors = false;
    info.enableStorageBufferDescriptors = true;
    m_descriptorPool.Create(info);

    // Debug meshes are copied into fixed buffers at their own offsets
    m_debugGeometryBuffers.SetVertexBuffer(VertexBuffer(s_debugGeometryVertexCapacity * sizeof(CompleteVertex)));
    m_debugGeometryBuffers.SetIndexBuffer(IndexBuffer(s_debugGeometryIndexCapacity * sizeof(u32)));

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
    for (u32 i = 0; i < FRAMES_IN_FLIGHT; ++i)
    {
        m_clusterStatsReadback[i] = StorageBuffer(sizeof(u32), false);
        m_clusterStatsReadback[i].SetName("Cluster Stats Readback " + stltype::to_string(i));
        m_pClusterStatsMapped[i] = static_cast<u32*>(m_clusterStatsReadback[i].MapMemory());
        *m_pClusterStatsMapped[i] = 0;
    }

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

void SharedResourceManager::BeginSceneGeometry(u64 sceneVertexBytes, u64 sceneIndexBytes)
{
    ScopedZone("SharedResourceManager::BeginSceneGeometry");

    // The primitives always sit at the start, fullscreen passes draw them from these buffers
    const auto& meshes = g_engine.GetMeshManager().GetMeshes();
    u64 vertexBytes = sceneVertexBytes;
    u64 indexBytes = sceneIndexBytes;
    for (u32 i = 0; i < MeshManager::PRIMITIVE_MESH_COUNT; ++i)
    {
        vertexBytes += meshes[i]->vertices.size() * sizeof(CompleteVertex);
        indexBytes += meshes[i]->indices.size() * sizeof(u32);
    }
    DEBUG_LOGF("SharedResourceManager: Scene geometry buffers. Vertex bytes: {}, Index bytes: {}",
               (u32)vertexBytes,
               (u32)indexBytes);

    // BufferData frees the old buffers once the frames in flight are done with them
    m_sceneGeometryBuffers.SetVertexBuffer(VertexBuffer(vertexBytes));
    m_sceneGeometryBuffers.SetIndexBuffer(IndexBuffer(indexBytes));

    SimpleScopedGuard lock(m_geometryStateMutex);
    m_bufferOffsetData = {};
    m_meshHandles.clear();
    for (u32 i = 0; i < MeshManager::PRIMITIVE_MESH_COUNT; ++i)
        AppendMesh(*meshes[i], m_sceneGeometryBuffers, m_bufferOffsetData, m_meshHandles);
}

void SharedResourceManager::ClearGeometryCaches()
{
    // Back to the primitives only, the old scene's meshes are about to be destroyed
    BeginSceneGeometry(0, 0);
    {
        SimpleScopedGuard lock(m_geometryStateMutex);
        m_debugMeshHandles.clear();
        m_debugBufferOffsetData = {};
    }
    // A reservation made before the switch belongs to the old scene
    m_reservationGeneration = g_engine.GetMeshManager().GetSceneGeometryReservation().generation;
    m_currentFrameInstanceData.clear();
    m_masterInstanceVisibility.clear();
}

void SharedResourceManager::UpdateInstanceDataSSBO(stltype::vector<RenderPasses::PassMeshData>& meshes)
{
    ScopedZone("SharedResourceManager::UpdateInstanceDataSSBO");
    auto& instanceData = m_currentFrameInstanceData;
    instanceData.clear();
    instanceData.reserve(meshes.size());

    m_masterInstanceVisibility.clear();
    m_masterInstanceVisibility.reserve(meshes.size());

    // A new scene reserved its geometry, size the buffers before its first mesh is appended
    const auto& reservation = g_engine.GetMeshManager().GetSceneGeometryReservation();
    if (reservation.generation != m_reservationGeneration)
    {
        m_reservationGeneration = reservation.generation;
        BeginSceneGeometry(reservation.vertexBytes, reservation.indexBytes);
    }

    for (auto& meshData : meshes)
    {
        auto& data = instanceData.emplace_back();
        const Mesh* pMesh = meshData.meshData.pMesh;
        MeshHandle handle;
        {
            // Meshes are uploaded the first time they show up
            const bool isDebug = meshData.meshData.IsDebugMesh();
            auto& handles = isDebug ? m_debugMeshHandles : m_meshHandles;
            SimpleScopedGuard lock(m_geometryStateMutex);
            if (auto it = handles.find(pMesh); it != handles.end())
                handle = it->second;
            else
                handle = AppendMesh(*pMesh,
                                    isDebug ? m_debugGeometryBuffers : m_sceneGeometryBuffers,
                                    isDebug ? m_debugBufferOffsetData : m_bufferOffsetData,
                                    handles);
        }

        data.drawData = handle;
        data.aabbCenterTransIdx = mathstl::Vector4(meshData.meshData.aabb.center);
        data.aabbExtentsMatIdx = mathstl::Vector4(meshData.meshData.aabb.extents);
        data.SetMaterialIdx(g_renderer.GetMaterialManager().GetMaterialIdx(meshData.meshData.pMaterial));
        data.SetTransformIdx(meshData.transformIdx);
        data.SetEntityID(static_cast<u32>(meshData.meshData.entityID));

        meshData.meshData.meshResourceHandle = data.drawData;
        meshData.meshData.instanceDataIdx = (u32)instanceData.size() - 1;

        data.SetVisible(true);
        m_masterInstanceVisibility.push_back(1u);
    }
    UploadInstanceDataSSBO();
}

MeshHandle SharedResourceManager::GetMeshHandle(const Mesh* pMesh) const
{
    SimpleScopedGuard lock(m_geometryStateMutex);
    auto it = m_meshHandles.find(pMesh);
    if (it != m_meshHandles.end())
    {
        return it->second;
    }
    return MeshResourceData{};
}

void SharedResourceManager::UpdateTransformRange(const stltype::vector<DirectX::XMFLOAT4X4>& transformBuffer,
                                                 u32 startIdx,
                                                 u32 count)
{
    ScopedZone("SharedResourceManager::UpdateTransformRange");
    if (count == 0) return;
    g_renderer.GetQueueHandler().SubmitTransferCommandAsync(
        AsyncQueueHandler::SSBOTransfer{&transformBuffer[startIdx],
                                        static_cast<u32>(count * sizeof(DirectX::XMFLOAT4X4)),
                                        &m_transformBuffer,
                                        static_cast<u32>(startIdx * sizeof(DirectX::XMFLOAT4X4))});
}

void SharedResourceManager::UpdatePrevTransformRange(const stltype::vector<DirectX::XMFLOAT4X4>& transformBuffer,
                                                     u32 startIdx,
                                                     u32 count)
{
    ScopedZone("SharedResourceManager::UpdatePrevTransformRange");
    if (count == 0) return;
    g_renderer.GetQueueHandler().SubmitTransferCommandAsync(
        AsyncQueueHandler::SSBOTransfer{&transformBuffer[startIdx],
                                        static_cast<u32>(count * sizeof(DirectX::XMFLOAT4X4)),
                                        &m_prevTransformBuffer,
                                        static_cast<u32>(startIdx * sizeof(DirectX::XMFLOAT4X4))});
}

void SharedResourceManager::UpdateSceneAABBRange(const stltype::vector<AABB>& aabbBuffer, u32 startIdx, u32 count)
{
    ScopedZone("SharedResourceManager::UpdateSceneAABBRange");
    if (count == 0) return;
    g_renderer.GetQueueHandler().SubmitTransferCommandAsync(
        AsyncQueueHandler::SSBOTransfer{&aabbBuffer[startIdx],
                                        static_cast<u32>(count * sizeof(AABB)),
                                        &m_sceneAABBBuffer,
                                        static_cast<u32>(startIdx * sizeof(AABB))});
}

void SharedResourceManager::UpdateGlobalMaterialBuffer(const UBO::MaterialBuffer& materialBuffer)
{
    ScopedZone("SharedResourceManager::UpdateGlobalMaterialBuffer");
    u32 materialCount = (u32)materialBuffer.size() > 0 ? (u32)materialBuffer.size() : 1;
    u32 byteSize = materialCount * sizeof(Material);

    g_renderer.GetQueueHandler().SubmitTransferCommandAsync(
        AsyncQueueHandler::SSBOTransfer{materialBuffer.data(), byteSize, &m_materialBuffer});
}

void SharedResourceManager::UploadInstanceDataSSBO()
{
    ScopedZone("SharedResourceManager::UploadInstanceDataSSBO");

    if (m_currentFrameInstanceData.empty())
        return;
    g_renderer.GetQueueHandler().SubmitTransferCommandAsync(AsyncQueueHandler::SSBOTransfer{
        m_currentFrameInstanceData.data(),
        static_cast<u32>(m_currentFrameInstanceData.size() * sizeof(m_currentFrameInstanceData[0])),
        &m_sceneInstanceBuffer});
}
