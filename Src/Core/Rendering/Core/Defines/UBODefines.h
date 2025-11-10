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
		ShadowmapViewUBO,
		RenderPass,
		LightUniformsUBO,
		GBufferUBO, // Gives access to all gbuffer textures as bindless textures
		ShadowmapUBO, // Contains all bindless handles for the scene shadowmaps
		GenericUBO,

		GenericSSBO,
		// SSBOs only below here
		GlobalObjectDataSSBOs, 
		TransformSSBO,
		PerPassObjectSSBO,
		TileArraySSBO,
		InstanceDataSSBO,
		Custom // Just indicate the class itself will specify all binding slots and so on
	};

	enum class DescriptorContentsType
	{
		LightData,
		GlobalInstanceData,
		GBuffer,
		BindlessTextureArray
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
		{ BufferType::GlobalObjectDataSSBOs, s_globalMaterialBufferSlot },
		{ BufferType::TileArraySSBO, s_tileArrayBindingSlot },
		{ BufferType::PerPassObjectSSBO, s_perPassObjectDataBindingSlot },
		{ BufferType::InstanceDataSSBO, s_globalInstanceDataSSBOSlot },
		{ BufferType::TransformSSBO, s_modelSSBOBindingSlot },
		{ BufferType::LightUniformsUBO, s_globalLightUniformsBindingSlot },
		{ BufferType::GBufferUBO, s_globalGbufferPostProcessUBOSlot },
		{ BufferType::ShadowmapUBO, s_shadowmapUBOBindingSlot },
		{ BufferType::ShadowmapViewUBO, s_shadowmapViewUBOBindingSlot } 

	};

	static inline stltype::hash_map<BufferType, ShaderTypeBits> s_UBOTypeToShaderStages = {
		{ BufferType::View, ShaderTypeBits::All },
		{ BufferType::RenderPass, ShaderTypeBits::Vertex },
		{ BufferType::GlobalObjectDataSSBOs, ShaderTypeBits::All },
		{ BufferType::TileArraySSBO, ShaderTypeBits::All },
		{ BufferType::PerPassObjectSSBO, ShaderTypeBits::All },
		{ BufferType::InstanceDataSSBO, ShaderTypeBits::All },
		{ BufferType::TransformSSBO, ShaderTypeBits::All },
		{ BufferType::LightUniformsUBO, ShaderTypeBits::All },
		{ BufferType::GBufferUBO, ShaderTypeBits::All },
		{ BufferType::ShadowmapUBO, ShaderTypeBits::All },
		{ BufferType::ShadowmapViewUBO, ShaderTypeBits::All }

	};
}