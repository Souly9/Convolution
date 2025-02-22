#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/Defines/VertexDefines.h"

struct AABB
{
	mathstl::Vector3 extents{ 0.f, 0.f, 0.f };
	mathstl::Vector3 center{};

	bool IsValid()
	{
		return extents.LengthSquared() != 0 && center.LengthSquared() != 0;
	}
};

struct Mesh
{
public:
	stltype::vector<CompleteVertex> vertices;
	stltype::vector<u32> indices;
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

	enum class PrimitiveType
	{
		Quad,
		Cube
	};
	Mesh* GetPrimitiveMesh(PrimitiveType type);
private:
	stltype::vector<stltype::unique_ptr<Mesh>> m_meshes;
	stltype::unique_ptr<Mesh> m_pPlanePrimitive;
	stltype::unique_ptr<Mesh> m_pCubePrimitive;
};

extern stltype::unique_ptr<MeshManager> g_pMeshManager;