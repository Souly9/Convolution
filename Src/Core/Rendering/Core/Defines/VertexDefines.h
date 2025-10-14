#pragma once
#include "../RenderingTypeDefs.h"

struct MinVertex
{
	mathstl::Vector3 position;
};
struct SimpleVertex : public MinVertex
{
	mathstl::Vector3 normal;
};

struct CompleteVertex : SimpleVertex
{
	mathstl::Vector2 texCoord;
};

namespace VertexInputDefines
{
	enum class VertexAttributeTemplates
	{
		None,
		Complete,
		PositionOnly
	};

	enum class VertexAttributes
	{
		Position,
		Normal,
		Tangent,
		Bitangent,
		Color0,
		Color1,
		TexCoord0,
		TexCoord1,
		TexCoord2,
	};

	struct VertexAttributeInfo
	{
		stltype::vector<VertexAttributes> attributes;
	};
}
namespace ConcreteVIS
{
	using namespace VertexInputDefines;
	static inline const VertexAttributeInfo g_completeVertexInputInfo = VertexAttributeInfo
	{
	.attributes = stltype::vector<VertexAttributes>{ VertexAttributes::Position, VertexAttributes::Normal, VertexAttributes::TexCoord0 }
	};

	static inline const VertexAttributeInfo g_positionOnlyVertexInputInfo = VertexAttributeInfo
	{
	.attributes = stltype::vector<VertexAttributes>{ VertexAttributes::Position }
	};
}

static inline const stltype::hash_map<VertexInputDefines::VertexAttributeTemplates, VertexInputDefines::VertexAttributeInfo> g_VertexInputToRenderDefs =
	{
		{ VertexInputDefines::VertexAttributeTemplates::Complete,		ConcreteVIS::g_completeVertexInputInfo },
		{ VertexInputDefines::VertexAttributeTemplates::PositionOnly,	ConcreteVIS::g_positionOnlyVertexInputInfo }
	};

static inline const stltype::hash_map<VertexInputDefines::VertexAttributes, u32> g_VertexAttributeSizeMap =
{
	{ VertexInputDefines::VertexAttributes::Position,		sizeof(DirectX::XMFLOAT3) },
	{ VertexInputDefines::VertexAttributes::Color0,	        sizeof(DirectX::XMFLOAT3) },
	{ VertexInputDefines::VertexAttributes::TexCoord0,	    sizeof(DirectX::XMFLOAT2) },
	{ VertexInputDefines::VertexAttributes::Normal,	        sizeof(DirectX::XMFLOAT3) }
};

static inline const stltype::hash_map<VertexInputDefines::VertexAttributes, u32> g_VertexAttributeBindingMap =
{
	{ VertexInputDefines::VertexAttributes::Position,		0 },
	{ VertexInputDefines::VertexAttributes::Color0,	        0 },
	{ VertexInputDefines::VertexAttributes::TexCoord0,	    0 },
	{ VertexInputDefines::VertexAttributes::Normal,	        0 }
};

static inline const stltype::hash_map<VertexInputDefines::VertexAttributes, u32> g_VertexAttributeLocationMap =
{
	{ VertexInputDefines::VertexAttributes::Position,		0 },
	{ VertexInputDefines::VertexAttributes::Color0,	        8 },
	{ VertexInputDefines::VertexAttributes::TexCoord0,	    2 },
	{ VertexInputDefines::VertexAttributes::Normal,         1 }
};

#ifdef USE_VULKAN
static inline const stltype::hash_map<VertexInputDefines::VertexAttributes, VkFormat> g_VertexAttributeVkFormatMap =
{
	{ VertexInputDefines::VertexAttributes::Position,		TEXFORMAT(R32G32B32_SFLOAT) },
	{ VertexInputDefines::VertexAttributes::Color0,	        TEXFORMAT(R32G32B32_SFLOAT) },
	{ VertexInputDefines::VertexAttributes::TexCoord0,	    TEXFORMAT(R32G32_SFLOAT) },
	{ VertexInputDefines::VertexAttributes::Normal,	        TEXFORMAT(R32G32B32_SFLOAT) }
};
#endif