#pragma once
#include "Core/Global/GlobalDefines.h"

enum class ShaderTypeBits : u32
{
	Vertex = 0x00000001,
	Geometry = 0x00000010,
	Fragment = 0x00000100,
	Compute = 0x00001000,
	All = 0x00010000
};

enum class DescriptorType
{
	UniformBuffer,
	StorageBuffer,
	CombinedImageSampler,
	BindlessTextures
};

#include "UBODefines.h"
#include "BindlessTexturesDefines.h"

struct PipelineDescriptorLayout
{
	DescriptorType type;
	u32 bindingSlot;
	ShaderTypeBits shaderStagesToBind;
	u32 descriptorCount{ 1 };

	PipelineDescriptorLayout(UBO::UBOType typeTemplate)
	{
		DEBUG_ASSERT(UBO::s_UBOTypeToBindingSlot.find(typeTemplate) != UBO::s_UBOTypeToBindingSlot.end());

		type = DescriptorType::UniformBuffer;
		bindingSlot = UBO::s_UBOTypeToBindingSlot.at(typeTemplate);
		shaderStagesToBind = (ShaderTypeBits)UBO::s_UBOTypeToShaderStages.at(typeTemplate);
	}
	PipelineDescriptorLayout(Bindless::BindlessType typeTemplate) : PipelineDescriptorLayout(typeTemplate, ShaderTypeBits::All)
	{
	}

	PipelineDescriptorLayout(Bindless::BindlessType typeTemplate, ShaderTypeBits stages)
	{
		DEBUG_ASSERT(Bindless::s_BindlessTypeToSlot.find(typeTemplate) != Bindless::s_BindlessTypeToSlot.end());

		type = ToDescriptorType(typeTemplate);
		bindingSlot = Bindless::s_BindlessTypeToSlot.at(typeTemplate);
		shaderStagesToBind = stages;
		descriptorCount = Bindless::s_BindlessTypeToCount.at(typeTemplate);
	}

	inline bool IsBindless() const { return Bindless::IsBindless(type); }
};