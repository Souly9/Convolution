#include "Core/Rendering/Core/Defines/GlobalBuffers.h"
#include "Mesh.h"

MeshManager::MeshManager()
{
	m_meshes.reserve(MAX_MESHES);

	// Plane primitive
	{
		stltype::vector<CompleteVertex> vertexData =
		{
			CompleteVertex{ DirectX::XMFLOAT3{-0.5f, -0.5f, 0.0f}, DirectX::XMFLOAT3{0.0f, 0.0f, 1.0f}, DirectX::XMFLOAT2{1.f, 0.f} },
			CompleteVertex{ DirectX::XMFLOAT3{.5f, -.5f, 0.0f}, DirectX::XMFLOAT3{0.0f, 0.0f, 1.0f}, DirectX::XMFLOAT2{0.f, 0.f} },
			CompleteVertex{ DirectX::XMFLOAT3{0.5f, 0.5f, 0.0f}, DirectX::XMFLOAT3{0.0f, 0.0f, 1.0f}, DirectX::XMFLOAT2{0.f, 1.f} },
			CompleteVertex{ DirectX::XMFLOAT3{-0.5f, 0.5f, 0.0f}, DirectX::XMFLOAT3{0.0f, 0.0f, 1.0f}, DirectX::XMFLOAT2{1.f, 1.f} }
		};
		const stltype::vector<u32> indices = {
			0, 1, 2, 2, 3, 0
		};

		m_pPlanePrimitive = stltype::make_unique<Mesh>(vertexData, indices);
	}
}

Mesh* MeshManager::GetPrimitiveMesh(PrimitiveType type)
{
	if(type == PrimitiveType::Quad)
		return m_pPlanePrimitive.get();
	return nullptr;
}
