#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/Profiling.h"
#include "Core/Global/ThreadBase.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"
#include "Core/Rendering/Core/RenderingIncludes.h"
#include "Core/Rendering/Vulkan/VkDescriptorSetLayout.h"
#include "Core/SceneGraph/Mesh.h"

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
    void Init();

    // Mainly for batch uploading all the scene geometry at scene load time, no
    // debug geometry
    void UploadSceneGeometry(const stltype::vector<stltype::unique_ptr<Mesh>>& meshes);

    // Whenever the scene is updated we need to update the instance data
    // This function is used for non-mesh updates, I assume the scene mesh data
    // itself won't update much Changing a material/scaling a mesh will be way
    // more common hence this separation
    void UpdateInstanceDataSSBO(stltype::vector<RenderPasses::PassMeshData>& meshes, u32 thisFrameNum);

    MeshHandle UploadMesh(const Mesh& mesh);
    MeshHandle GetMeshHandle(const Mesh* pMesh) const;

    void UploadDebugMesh(const Mesh& mesh);

    void WriteInstanceSSBODescriptorUpdate(u32 targetFrame);

    void UpdateTransformBuffer(const stltype::vector<DirectX::XMFLOAT4X4>& transformBuffer, u32 thisFrame);
    void UpdateSceneAABBBuffer(const stltype::vector<AABB>& aabbBuffer, u32 thisFrame);
    void UpdateGlobalMaterialBuffer(const UBO::MaterialBuffer& materialBuffer, u32 thisFrame);

    DescriptorSet* GetInstanceSSBODescriptorSet(u32 frameIdx)
    {
        return m_frameData[frameIdx].pSceneInstanceSSBOSet;
    }

    DescriptorSet* GetSceneAABBSSBODescriptorSet(u32 frameIdx)
    {
        return m_frameData[frameIdx].pSceneAABBSet;
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
        u64 instanceBufferIdx{0};
        u64 vertexCount{0};
        u64 indexCount{0};
        u64 instanceCount{0};
    };

    const BufferStats& GetBufferOffsetData() const
    {
        return m_bufferOffsetData;
    }

private:
    void UpdateInstanceBuffer(const Mesh& mesh);
    void UpdateSceneGeometryBuffer(const Mesh& mesh);

    ProfiledLockable(CustomMutex, m_bufferUpdateMutex);

    BufferData m_sceneGeometryBuffers;
    // Seperating the debug stuff to update it easier and so on, not sure about it
    // though...
    BufferData m_debugGeometryBuffers;
    StorageBuffer m_transformBuffer;
    StorageBuffer m_sceneInstanceBuffer;
    StorageBuffer m_sceneAABBBuffer;
    StorageBuffer m_materialBuffer;

    DescriptorSetLayoutVulkan m_sceneInstanceSSBOLayout;
    DescriptorSetLayoutVulkan m_sceneAABBLayout;
    DescriptorPool m_descriptorPool;
    struct FrameData
    {
        DescriptorSet* pSceneInstanceSSBOSet;
        DescriptorSet* pSceneAABBSet;
    };
    stltype::fixed_vector<FrameData, SWAPCHAIN_IMAGES, false> m_frameData;

    
    BufferStats m_bufferOffsetData;
    BufferStats m_debugBufferOffsetData;

    // Duplicating it on cpu side for more efficient processing
    stltype::vector<UBO::InstanceData> m_currentFrameInstanceData;

    stltype::hash_map<const Mesh*, MeshHandle> m_meshHandles;
    stltype::hash_map<const Mesh*, MeshHandle> m_debugMeshHandles;
};