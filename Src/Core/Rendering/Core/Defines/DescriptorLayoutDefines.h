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
	u32 setIndex;
	u32 bindingSlot;
	ShaderTypeBits shaderStagesToBind;
	u32 descriptorCount{ 1 };

	PipelineDescriptorLayout() = default;

	PipelineDescriptorLayout(UBO::BufferType typeTemplate, u32 setIdx = 0) // setIdx can be left empty if this just used to simplify layout creation for descriptorpools
	{
		DEBUG_ASSERT(UBO::s_UBOTypeToBindingSlot.find(typeTemplate) != UBO::s_UBOTypeToBindingSlot.end());

		type = GetDescriptorType(typeTemplate);
		bindingSlot = UBO::s_UBOTypeToBindingSlot.at(typeTemplate);
		shaderStagesToBind = (ShaderTypeBits)UBO::s_UBOTypeToShaderStages.at(typeTemplate);
		setIndex = setIdx;
	}

	PipelineDescriptorLayout(Bindless::BindlessType typeTemplate, u32 setIdx = 0) : PipelineDescriptorLayout(typeTemplate, ShaderTypeBits::All, setIdx)
	{
	}

	PipelineDescriptorLayout(Bindless::BindlessType typeTemplate, ShaderTypeBits stages, u32 setIdx)
	{
		DEBUG_ASSERT(Bindless::s_BindlessTypeToSlot.find(typeTemplate) != Bindless::s_BindlessTypeToSlot.end());

		type = ToDescriptorType(typeTemplate);
		bindingSlot = Bindless::s_BindlessTypeToSlot.at(typeTemplate);
		descriptorCount = Bindless::s_BindlessTypeToCount.at(typeTemplate);
		shaderStagesToBind = stages;
		setIndex = setIdx;
	}

	inline bool IsBindless() const { return Bindless::IsBindless(type); }
};