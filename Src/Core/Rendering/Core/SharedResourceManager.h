#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/Profiling.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"
#include "Core/Rendering/Core/Buffer.h"
#include "Core/Rendering/Core/RenderingData.h"
#include "Core/SceneGraph/Mesh.h"
#include "Core/Rendering/Core/DescriptorSetLayout.h"
#include "Core/Rendering/Core/DescriptorPool.h"
#include "Core/Rendering/Core/CpuPreprocess/Frustum.h"
#include "Core/Rendering/Passes/PassManagerDefines.h"

namespace RenderPasses
{
struct PassMeshData;
};
// Main class to manage scene-wide resources like vertex/index buffers and other
// things needed for our gpu driven pipeline All scene geometry is uploaded into
// one giant buffer for now, managed by this manager who gives out mesh handles
// to the rest of the engine Handles contain all the info to find the meshes and
// their materials in the buffers Want to split the buffer to allow streaming of
// meshes eventually... Mainly communicating with the PassManager
class SharedResourceManager
{
public:
    // Debug meshes live in fixed buffers
    static constexpr u32 s_debugGeometryVertexCapacity = 256 * 1024;
    static constexpr u32 s_debugGeometryIndexCapacity = 1024 * 1024;

    void Init();

    void ClearGeometryCaches();

    // Whenever the scene is updated we need to update the instance data
    // This function is used for non-mesh updates, I assume the scene mesh data
    // itself won't update much Changing a material/scaling a mesh will be way
    // more common hence this separation
    void UpdateInstanceDataSSBO(stltype::vector<RenderPasses::PassMeshData>& meshes, u32 thisFrameNum);
    void UploadInstanceDataSSBO(u32 frameIdx);

    stltype::vector<UBO::InstanceData>& GetInstanceData() { return m_currentFrameInstanceData; }
    const stltype::vector<UBO::InstanceData>& GetInstanceData() const { return m_currentFrameInstanceData; }
    const stltype::vector<u8>& GetMasterInstanceVisibility() const { return m_masterInstanceVisibility; }

    MeshHandle GetMeshHandle(const Mesh* pMesh) const;

    void WriteInstanceSSBODescriptorUpdate(u32 targetFrame);

    void UpdateTransformBuffer(const stltype::vector<DirectX::XMFLOAT4X4>& transformBuffer, u32 thisFrame, u32 updateCount = 0);
    void UpdateTransformRange(const stltype::vector<DirectX::XMFLOAT4X4>& transformBuffer,
                              u32 startIdx,
                              u32 count,
                              u32 thisFrame);
    void UpdatePrevTransformRange(const stltype::vector<DirectX::XMFLOAT4X4>& transformBuffer,
                                  u32 startIdx,
                                  u32 count,
                                  u32 thisFrame);
    void UpdateSceneAABBBuffer(const stltype::vector<AABB>& aabbBuffer, u32 thisFrame, u32 updateCount = 0);
    void UpdateSceneAABBRange(const stltype::vector<AABB>& aabbBuffer, u32 startIdx, u32 count, u32 thisFrame);
    void UpdateGlobalMaterialBuffer(const UBO::MaterialBuffer& materialBuffer, u32 thisFrame);

    DescriptorSet::Ptr GetInstanceSSBODescriptorSet(u32 frameIdx)
    {
        return m_frameData[frameIdx % m_frameData.size()].pSceneInstanceSSBOSet;
    }

    DescriptorSet::Ptr GetSceneAABBSSBODescriptorSet(u32 frameIdx)
    {
        return m_frameData[frameIdx % m_frameData.size()].pSceneAABBSet;
    }

    DescriptorSet::Ptr GetViewSpaceLightsDescriptorSet(u32 frameIdx)
    {
        return m_frameData[frameIdx % m_frameData.size()].pViewSpaceLightsSet;
    }

    StorageBuffer& GetViewSpaceLightsSSBO()
    {
        return m_viewSpaceLightsSSBO;
    }
    // Per frame slot, valid once the slot's previous frame has finished
    StorageBuffer& GetClusterStatsReadback(u32 frameIdx)
    {
        return m_clusterStatsReadback[frameIdx];
    }
    u32 ReadClusterLightTotal(u32 frameIdx) const
    {
        return *m_pClusterStatsMapped[frameIdx];
    }


    const BufferData& GetSceneGeometryBuffers() const
    {
        return m_sceneGeometryBuffers;
    }
    BufferData& GetSceneGeometryBuffers()
    {
        return m_sceneGeometryBuffers;
    }

    const BufferData& GetDebugGeometryBuffers() const
    {
        return m_debugGeometryBuffers;
    }
    BufferData& GetDebugGeometryBuffers()
    {
        return m_debugGeometryBuffers;
    }

    struct BufferStats
    {
        u64 vertBufferOffset{0};
        u64 indexBufferOffset{0};
    };

private:
    // Allocates the scene geometry buffers (the primitives are added on top) and starts a new set of mesh handles
    void BeginSceneGeometry(u64 sceneVertexBytes, u64 sceneIndexBytes);
    // Queues the mesh for upload at the next offset of the buffers, caller holds m_geometryStateMutex
    MeshHandle AppendMesh(const Mesh& mesh,
                          BufferData& buffers,
                          BufferStats& offsets,
                          stltype::hash_map<const Mesh*, MeshHandle>& handles);

    BufferData m_sceneGeometryBuffers;
    // Seperating the debug stuff to update it easier and so on, not sure about it
    // though...
    BufferData m_debugGeometryBuffers;
    StorageBuffer m_transformBuffer;
    StorageBuffer m_prevTransformBuffer;
    StorageBuffer m_sceneInstanceBuffer;
    StorageBuffer m_sceneAABBBuffer;
    StorageBuffer m_materialBuffer;
    StorageBuffer m_viewSpaceLightsSSBO;
    stltype::array<StorageBuffer, FRAMES_IN_FLIGHT> m_clusterStatsReadback;
    stltype::array<u32*, FRAMES_IN_FLIGHT> m_pClusterStatsMapped{};

    DescriptorSetLayout m_sceneInstanceSSBOLayout;
    DescriptorSetLayout m_sceneAABBLayout;
    DescriptorSetLayout m_viewSpaceLightsLayout;
    DescriptorPool m_descriptorPool;
    struct FrameData
    {
        DescriptorSet::Ptr pSceneInstanceSSBOSet;
        DescriptorSet::Ptr pSceneAABBSet;
        DescriptorSet::Ptr pViewSpaceLightsSet;
    };
    stltype::fixed_vector<FrameData, SWAPCHAIN_IMAGES, false> m_frameData;

    
    BufferStats m_bufferOffsetData;
    BufferStats m_debugBufferOffsetData;
    u32 m_reservationGeneration{0};

    // Duplicating it on cpu side for more efficient processing
    stltype::vector<UBO::InstanceData> m_currentFrameInstanceData;
    stltype::vector<u8> m_masterInstanceVisibility;

    stltype::hash_map<const Mesh*, MeshHandle> m_meshHandles;
    stltype::hash_map<const Mesh*, MeshHandle> m_debugMeshHandles;

    mutable ProfiledLockable(CustomMutex, m_geometryStateMutex);

public:
    StorageBuffer& GetInstanceBuffer() { return m_sceneInstanceBuffer; }
};
