#pragma once
#include "Core/Global/GlobalDefines.h"

struct SimpleVertex
{
	DirectX::XMFLOAT3 position;
};

struct ColoredVertex : SimpleVertex
{
	DirectX::XMFLOAT3 color;
};

struct CompleteVertex : ColoredVertex
{
	DirectX::XMFLOAT2 texCoord;
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
		TexCoord3,
		TexCoord4,
		TexCoord5,
		TexCoord6,
		TexCoord7
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
	.attributes = stltype::vector<VertexAttributes>{ VertexAttributes::Position, VertexAttributes::Color0, VertexAttributes::TexCoord0 }
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
	{ VertexInputDefines::VertexAttributes::TexCoord0,	    sizeof(DirectX::XMFLOAT2) }
};

static inline const stltype::hash_map<VertexInputDefines::VertexAttributes, u32> g_VertexAttributeBindingMap =
{
	{ VertexInputDefines::VertexAttributes::Position,		0 },
	{ VertexInputDefines::VertexAttributes::Color0,	        0 },
	{ VertexInputDefines::VertexAttributes::TexCoord0,	    0 }
};

static inline const stltype::hash_map<VertexInputDefines::VertexAttributes, u32> g_VertexAttributeLocationMap =
{
	{ VertexInputDefines::VertexAttributes::Position,		0 },
	{ VertexInputDefines::VertexAttributes::Color0,	        1 },
	{ VertexInputDefines::VertexAttributes::TexCoord0,	    2 }
};

#ifdef USE_VULKAN
static inline const stltype::hash_map<VertexInputDefines::VertexAttributes, VkFormat> g_VertexAttributeVkFormatMap =
{
	{ VertexInputDefines::VertexAttributes::Position,		VK_FORMAT_R32G32B32_SFLOAT },
	{ VertexInputDefines::VertexAttributes::Color0,	        VK_FORMAT_R32G32B32_SFLOAT },
	{ VertexInputDefines::VertexAttributes::TexCoord0,	    VK_FORMAT_R32G32_SFLOAT }
};
#endif