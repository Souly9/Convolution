#pragma once
#include "Core/ECS/Entity.h"
#include "Core/Global/Typedefs.h"
#include "Core/Rendering/Core/AABB.h"
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/SceneGraph/Mesh.h"
#include <EASTL/bitset.h>
#include <EASTL/hash_map.h>
#include <EASTL/vector.h>

namespace RenderPasses
{

struct EntityMeshData
{
    Mesh* pMesh;
    Material* pMaterial;
    AABB aabb;
    MeshHandle meshResourceHandle;
    u32 instanceDataIdx;

    ECS::EntityID entityID;
    u32 subMeshIdx;

    EntityMeshData(ECS::EntityID id, u32 subIdx, Mesh* pM, Material* pMat, const AABB& box, bool isDebug)
        : entityID(id), subMeshIdx(subIdx), pMesh(pM), pMaterial(pMat), aabb{box}
    {
        flags[s_isDebugMeshFlag] = isDebug;
    }

    bool IsDebugMesh() const { return flags[s_isDebugMeshFlag] || flags[s_isDebugWireframeMesh]; }
    bool IsInstanced() const { return flags[s_isInstancedFlag]; }
    bool IsDebugWireframeMesh() const { return flags[s_isDebugWireframeMesh]; }
    bool SetDebugMesh() { return flags[s_isDebugMeshFlag] = true; }
    bool SetInstanced() { return flags[s_isInstancedFlag] = true; }
    bool SetDebugWireframeMesh() { return flags[s_isDebugWireframeMesh] = true; }

    bool DidGeometryChange(const EntityMeshData& other) const { return pMesh != other.pMesh; }

protected:
    static inline u8 s_isDebugMeshFlag     = 0;
    static inline u8 s_isInstancedFlag     = 1;
    static inline u8 s_isDebugWireframeMesh = 2;
    stltype::bitset<8> flags{};
};

struct PassMeshData
{
    EntityMeshData meshData;
    u32 transformIdx;
    u32 perObjectDataIdx;
};

struct PassGeometryData
{
    stltype::vector<PassMeshData> staticMeshPassData;
};

using EntityMeshDataMap      = stltype::hash_map<u64, stltype::vector<EntityMeshData>>;
using EntityDebugMeshDataMap = stltype::hash_map<u64, EntityMeshData>;
using TransformSystemData    = stltype::vector<stltype::pair<ECS::EntityID, mathstl::Matrix>>;
using EntityTransformData    = stltype::pair<stltype::vector<ECS::EntityID>, stltype::vector<DirectX::XMFLOAT4X4>>;
using EntityMaterialMap      = stltype::hash_map<u64, stltype::vector<Material*>>;

} // namespace RenderPasses
