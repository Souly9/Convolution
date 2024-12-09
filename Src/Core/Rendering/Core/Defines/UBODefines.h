#pragma once
#include "Core/Global/GlobalDefines.h"
#include "DescriptorLayoutDefines.h"
#include "BindingSlots.h"

namespace UBO
{
	// Maps UBO template type to its binding slot
	enum class BufferType : u32
	{
		View,
		RenderPass,
		GenericUBO,
		GenericSSBO,
		GlobalTransformSSBO,
		TileArraySSBO,
		Custom // Just indicate the class itself will specify all binding slots and so on
	};

	static inline DescriptorType GetDescriptorType(BufferType type)
	{
		if ((u32)type >= (u32)BufferType::GenericSSBO)
			return DescriptorType::StorageBuffer;
		else
			return DescriptorType::UniformBuffer;
	}
	static inline stltype::hash_map<BufferType, u32> s_UBOTypeToBindingSlot = {
		{ BufferType::View, s_viewBindingSlot },
		{ BufferType::RenderPass, s_renderPassBindingSlot },
		{ BufferType::GlobalTransformSSBO, s_modelSSBOBindingSlot },
		{ BufferType::TileArraySSBO, s_tileArrayBindingSlot }

	};

	static inline stltype::hash_map<BufferType, ShaderTypeBits> s_UBOTypeToShaderStages = {
		{ BufferType::View, ShaderTypeBits::Vertex },
		{ BufferType::RenderPass, ShaderTypeBits::Vertex },
		{ BufferType::GlobalTransformSSBO, ShaderTypeBits::All },
		{ BufferType::TileArraySSBO, ShaderTypeBits::Fragment }

	};
}