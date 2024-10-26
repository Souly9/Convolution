#pragma once
#include "Core/Global/GlobalDefines.h"
#include "DescriptorLayoutDefines.h"

namespace Bindless
{
	// Main bindless buffer for all textures of the scene
	static inline u32 s_globalBindlessTextureBufferBindingSlot = 400;

	// Maps UBO template type to its binding slot
	enum class BindlessType : u32
	{
		GlobalTextures,
		Custom // Just indicate the class itself will specify all binding slots and so on
	}; 

	static inline stltype::hash_map<BindlessType, u32> s_BindlessTypeToSlot = {
		{ BindlessType::GlobalTextures, s_globalBindlessTextureBufferBindingSlot }
	};
	static inline stltype::hash_map<BindlessType, u32> s_BindlessTypeToCount = {
		{ BindlessType::GlobalTextures, MAX_BINDLESS_TEXTURES }
	};

	static inline DescriptorType ToDescriptorType(BindlessType type)
	{
		switch (type)
		{
			case BindlessType::GlobalTextures:
				return DescriptorType::BindlessTextures;
			default:
				DEBUG_ASSERT(false);
		}

		return DescriptorType::BindlessTextures;
	}

	static inline bool IsBindless(DescriptorType type)
	{
		switch (type)
		{
			case DescriptorType::UniformBuffer:
				return false;
			case DescriptorType::StorageBuffer:
				return false;
			case DescriptorType::CombinedImageSampler:
				return false;
			case DescriptorType::BindlessTextures:
				return true;
			default:
				DEBUG_ASSERT(false);
		}

		return false;
	}
}