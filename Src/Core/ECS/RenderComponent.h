#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Component.h"

class RenderComponent : public Component
{
public:
	const stltype::vector<DirectX::XMFLOAT3>& GetVertices() const { return m_vertices; }

protected:
	stltype::vector<DirectX::XMFLOAT3> m_vertices;
};