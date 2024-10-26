#pragma once
#include "Core/Global/GlobalDefines.h"
#include "DescriptorLayoutDefines.h"

namespace UBO
{
	static inline u32 s_viewBindingSlot = 300;
	static inline u32 s_renderPassBindingSlot = 301;

	// Maps UBO template type to its binding slot
	enum class UBOType : u32
	{
		View,
		RenderPass,
		Custom // Just indicate the class itself will specify all binding slots and so on
	};

	static inline stltype::hash_map<UBOType, u32> s_UBOTypeToBindingSlot = {
		{ UBOType::View, s_viewBindingSlot },
		{ UBOType::RenderPass, s_renderPassBindingSlot }
	};

	static inline stltype::hash_map<UBOType, ShaderTypeBits> s_UBOTypeToShaderStages = {
		{ UBOType::View, ShaderTypeBits::Vertex },
		{ UBOType::RenderPass, ShaderTypeBits::Vertex }
	};

	struct ViewUBO
	{
		DirectX::XMFLOAT4X4 view;
		DirectX::XMFLOAT4X4 projection;
		DirectX::XMFLOAT4X4 model;
	};

	struct RenderPassUBO
	{
		DirectX::XMFLOAT4X4 model;

	};

	


}