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
		{ BufferType::GlobalTransformSSBO, s_modelSSBOBindingSlot }

	};

	static inline stltype::hash_map<BufferType, ShaderTypeBits> s_UBOTypeToShaderStages = {
		{ BufferType::View, ShaderTypeBits::Vertex },
		{ BufferType::RenderPass, ShaderTypeBits::Vertex },
		{ BufferType::GlobalTransformSSBO, ShaderTypeBits::All }

	};

	static constexpr u64 GlobalTransformSSBOSize = sizeof(DirectX::XMFLOAT4X4) * MAX_ENTITIES;

	// Contains all model matrices of the scene, mainly because we assume we will perform some kind of rendering operation on every entity in this renderer
	struct GlobalTransformSSBO
	{
		stltype::vector<DirectX::XMFLOAT4X4> modelMatrices{};
	};

	struct ViewUBO
	{
		DirectX::XMFLOAT4X4 view;
		DirectX::XMFLOAT4X4 projection;
	};

	struct RenderPassUBO
	{
		stltype::vector<u32> entityIndices{};
	};

	


}