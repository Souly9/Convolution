#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Defines/VertexDefines.h"
#include "Core/Rendering/Core/Defines/GlobalBuffers.h"

struct Mesh
{
public:
	stltype::vector<CompleteVertex> vertices;
	stltype::vector<u32> indices;
	AABB boundingBox{};
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
		pMesh->vertices.reserve(vertexCount);
		pMesh->indices.reserve(indexCount);
		return pMesh;
	}

	AABB CalcAABB(const mathstl::Vector3& min, const mathstl::Vector3& max, const Mesh* pMesh = nullptr)
	{
		AABB aabb{};
		aabb.extents = (max - min) * 0.5f;
		const auto center = min + aabb.extents;
		aabb.extents += center;
		if (pMesh)
			m_meshAABBs.emplace(pMesh, aabb);
		return aabb;
	}

	const stltype::hash_map<const Mesh*, AABB>& GetMeshAABBs() const { return m_meshAABBs; }

	enum class PrimitiveType
	{
		Quad,
		Cube
	};
	Mesh* GetPrimitiveMesh(PrimitiveType type);

	const stltype::vector<stltype::unique_ptr<Mesh>>& GetMeshes() const { return m_meshes; }
private:
	stltype::vector<stltype::unique_ptr<Mesh>> m_meshes;
	stltype::hash_map<const Mesh*, AABB> m_meshAABBs;
	stltype::unique_ptr<Mesh> m_pPlanePrimitive;
	stltype::unique_ptr<Mesh> m_pCubePrimitive;
};

extern stltype::unique_ptr<MeshManager> g_pMeshManager;

static inline constexpr MeshManager::PrimitiveType s_lightDebugMeshPrimitive = MeshManager::PrimitiveType::Cube;