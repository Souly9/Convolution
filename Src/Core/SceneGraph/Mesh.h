#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/AABB.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"
#include "Core/Rendering/Core/Defines/VertexDefines.h"

struct Mesh
{
public:
    static inline constexpr u32 InvalidRTMeshId = ~0u;

    Mesh() = default;
    Mesh(stltype::vector<CompleteVertex> inVertices, stltype::vector<u32> inIndices)
        : vertices(stltype::move(inVertices)), indices(stltype::move(inIndices))
    {
    }

    stltype::vector<CompleteVertex> vertices;
    stltype::vector<u32> indices;
    AABB boundingBox{};
    u32 rtMeshId{InvalidRTMeshId};
    u32 rtMeshGeneration{0};
};

class MeshManager
{
public:
    MeshManager();
    ~MeshManager() = default;

    Mesh* AllocateMesh(u32 vertexCount, u32 indexCount)
    {
        m_meshes.push_back(stltype::make_unique<Mesh>());
        auto* pMesh = m_meshes.back().get();
        AllocateRTMeshIdentity(*pMesh);
        pMesh->vertices.reserve(vertexCount);
        pMesh->indices.reserve(indexCount);
        return pMesh;
    }

    AABB CalcAABB(const mathstl::Vector3& min, const mathstl::Vector3& max, const Mesh* pMesh = nullptr)
    {
        AABB aabb{};
        const auto extents = (max - min) * 0.5f;
        aabb.extents = mathstl::Vector4(extents.x, extents.y, extents.z, 0.0f);
        const auto center = min + extents;
        aabb.center = mathstl::Vector4(center.x, center.y, center.z, 0.0f);
        if (pMesh)
            m_meshAABBs.emplace(pMesh, aabb);
        return aabb;
    }

    const stltype::hash_map<const Mesh*, AABB>& GetMeshAABBs() const
    {
        return m_meshAABBs;
    }

    void Flush();

    enum class PrimitiveType
    {
        Quad,
        Cube,
        FullscreenTriangle
    };
    Mesh* GetPrimitiveMesh(PrimitiveType type);

    const stltype::vector<stltype::unique_ptr<Mesh>>& GetMeshes() const
    {
        return m_meshes;
    }

private:
    void AllocateRTMeshIdentity(Mesh& mesh);
    void ReleaseRTMeshIdentity(const Mesh& mesh);

    stltype::vector<stltype::unique_ptr<Mesh>> m_meshes;
    stltype::hash_map<const Mesh*, AABB> m_meshAABBs;
    Mesh* m_pPlanePrimitive;
    Mesh* m_pCubePrimitive;
    Mesh* m_pFullscreenTrianglePrimitive;
    stltype::vector<u32> m_rtMeshGenerations;
    stltype::vector<u32> m_freeRTMeshIds;
};

extern stltype::unique_ptr<MeshManager> g_pMeshManager;

static inline constexpr MeshManager::PrimitiveType s_lightDebugMeshPrimitive = MeshManager::PrimitiveType::Cube;
