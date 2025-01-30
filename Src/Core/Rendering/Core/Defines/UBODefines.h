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
		LightUniformsUBO,
		GenericUBO,
		GenericSSBO,
		GlobalObjectDataSSBOs, 
		TransformSSBO,
		PerPassObjectSSBO,
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
		{ BufferType::GlobalObjectDataSSBOs, s_globalObjectDataBindingSlot },
		{ BufferType::TileArraySSBO, s_tileArrayBindingSlot },
		{ BufferType::PerPassObjectSSBO, s_perPassObjectDataBindingSlot },
		{ BufferType::TransformSSBO, s_modelSSBOBindingSlot },
		{ BufferType::LightUniformsUBO, s_globalLightUniformsBindingSlot }

	};

	static inline stltype::hash_map<BufferType, ShaderTypeBits> s_UBOTypeToShaderStages = {
		{ BufferType::View, ShaderTypeBits::Vertex },
		{ BufferType::RenderPass, ShaderTypeBits::Vertex },
		{ BufferType::GlobalObjectDataSSBOs, ShaderTypeBits::All },
		{ BufferType::TileArraySSBO, ShaderTypeBits::All },
		{ BufferType::PerPassObjectSSBO, ShaderTypeBits::All },
		{ BufferType::TransformSSBO, ShaderTypeBits::All },
		{ BufferType::LightUniformsUBO, ShaderTypeBits::All }

	};
}