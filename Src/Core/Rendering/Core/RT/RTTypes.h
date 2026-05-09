#pragma once
#include "Core/Rendering/Core/AccelerationStructure.h"
#include "Core/Rendering/Core/Buffer.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/SceneGraph/Mesh.h"
#include "../../../../../Shaders/Globals/RT.h"
#include <cstring>

namespace RT
{
enum class BLASState : u8
{
    Uninitialized = 0,
    WaitingForRasterBuffers,
    QueuedForBuild,
    Building,
    Ready,
    Failed,
};

enum class TLASState : u8
{
    Uninitialized = 0,
    Building,
    Ready,
    Failed,
};

struct BLASRecord
{
    u32 meshId{Mesh::InvalidRTMeshId};
    u32 generation{0};
    const Mesh* pMesh{nullptr};
    MeshHandle rasterHandle{};
    BLASState state{BLASState::Uninitialized};
    AccelerationStructure accelerationStructure{};
    GenericBuffer storageBuffer{};
    GenericBuffer scratchBuffer{};
    u64 scratchSize{0};
    u64 deviceAddress{0};
    u32 primitiveCount{0};
    AccelerationStructureBuildFlags buildFlags{AccelerationStructureBuildFlags::None};
};

struct RTInstanceRecord
{
    u64 instanceKey{0};
    u32 meshId{Mesh::InvalidRTMeshId};
    u32 blasGeneration{0};
    u32 transformIndex{0};
    u32 instanceDataIdx{0};
    ECS::EntityID entityId{0};
    u32 subMeshIdx{0};
    u32 flags{0};
    AccelerationStructureInstanceData instanceData{};

    bool operator==(const RTInstanceRecord& other) const
    {
        return instanceKey == other.instanceKey &&
               meshId == other.meshId &&
               blasGeneration == other.blasGeneration &&
               transformIndex == other.transformIndex &&
               instanceDataIdx == other.instanceDataIdx &&
               entityId == other.entityId &&
               subMeshIdx == other.subMeshIdx &&
               flags == other.flags &&
               memcmp(&instanceData, &other.instanceData, sizeof(instanceData)) == 0;
    }
};

struct TLASFrameData
{
    TLASState state{TLASState::Uninitialized};
    AccelerationStructure accelerationStructure{};
    GenericBuffer storageBuffer{};
    GenericBuffer scratchBuffer{};
    GenericBuffer instanceBuffer{};
    GenericBuffer hitDataBuffer{};
    u64 scratchSize{0};
    u32 lastBuiltInstanceCount{0};
};
} // namespace RT
