#pragma once
#include "Core/Rendering/Core/Pipeline.h"
#include "Core/Rendering/Core/Texture.h"
#include "Core/Rendering/Core/Buffer.h"
#include "Core/Rendering/Core/Defines/UBODefines.h"

inline VkPolygonMode Conv(const PolygonMode& m)
{
	switch (m)
	{
		case(PolygonMode::Fill) :
		{
			return VK_POLYGON_MODE_FILL;
		}
		case(PolygonMode::Point):
		{
			return VK_POLYGON_MODE_POINT;
		}
		case(PolygonMode::Line):
		{
			return VK_POLYGON_MODE_LINE;
		}
		default:
		{
			ASSERT(false);
			break;
		}
	}
	return VK_POLYGON_MODE_FILL;
}

inline VkPrimitiveTopology Conv(const Topology& m)
{
	switch (m)
	{
		case Topology::Points:
			return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
			break;
		case Topology::Lines:
			return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
			break;
		case Topology::LineStrip:
			return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
			break;
		case Topology::TriangleList:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			break;
		case Topology::TriangleStrip:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
			break;
	}

	return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

inline VkCullModeFlagBits Conv(const Cullmode& m)
{
	switch (m)
	{
		case Cullmode::None:
			return VK_CULL_MODE_NONE;
			break;
		case Cullmode::Front:
			return VK_CULL_MODE_FRONT_BIT;
			break;
		case Cullmode::Back:
			return VK_CULL_MODE_BACK_BIT;
			break;
		case Cullmode::Both:
			return VK_CULL_MODE_FRONT_AND_BACK;
			break;
	}

	return VK_CULL_MODE_FRONT_BIT;
}

inline VkFrontFace Conv(const FrontFace& m)
{
	switch (m)
	{
		case FrontFace::CounterClockwise:
			return VK_FRONT_FACE_COUNTER_CLOCKWISE;
			break;
		case FrontFace::Clockwise:
			return VK_FRONT_FACE_CLOCKWISE;
			break;
	}

	return VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

inline VkAttachmentLoadOp Conv(const LoadOp& m)
{
	switch (m)
	{
		case LoadOp::LOAD:
			return VK_ATTACHMENT_LOAD_OP_LOAD;
			break;
		case LoadOp::CLEAR:
			return VK_ATTACHMENT_LOAD_OP_CLEAR;
			break;
		case LoadOp::IDC:
			return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			break;
	}
	DEBUG_ASSERT(false);
	return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

inline VkAttachmentStoreOp Conv(const StoreOp& m)
{
	switch (m)
	{
		case StoreOp::STORE:
			return VK_ATTACHMENT_STORE_OP_STORE;
			break;
		case StoreOp::IDC:
			return VK_ATTACHMENT_STORE_OP_DONT_CARE;
			break;
	}

	return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

inline VkImageLayout Conv(const ImageLayout& m)
{
	switch (m)
	{
		case ImageLayout::UNDEFINED:
			return VK_IMAGE_LAYOUT_UNDEFINED;
			break;
		case ImageLayout::TRANSFER_SRC_OPTIMAL:
			return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			break;
		case ImageLayout::TRANSFER_DST_OPTIMAL:
			return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			break;
		case ImageLayout::SHADER_READ_OPTIMAL:
			return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			break;
		case ImageLayout::PRESENT:
			return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
			break;
		case ImageLayout::COLOR_ATTACHMENT:
			return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			break;
		case ImageLayout::DEPTH_STENCIL:
			return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			break;
	}
	DEBUG_ASSERT(false);
	return VK_IMAGE_LAYOUT_UNDEFINED;
}

inline VkSampleCountFlagBits Conv(const u32& m)
{
	switch (m)
	{
		case 1:
			return VK_SAMPLE_COUNT_1_BIT;
			break;
		case 2:
			return VK_SAMPLE_COUNT_2_BIT;
			break;
		case 4:
			return VK_SAMPLE_COUNT_4_BIT;
			break;
		case 8:
			return VK_SAMPLE_COUNT_8_BIT;
			break;
		case 16:
			return VK_SAMPLE_COUNT_16_BIT;
			break;
		case 32:
			return VK_SAMPLE_COUNT_32_BIT;
			break;
	}

	return VK_SAMPLE_COUNT_1_BIT;
}

inline VkBufferUsageFlags Conv(const BufferUsage& m)
{
	switch (m)
	{
		case BufferUsage::Vertex:
			return VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		case BufferUsage::Index:
			return VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		case BufferUsage::Texture:
			return VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		case BufferUsage::Staging:
			return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		case BufferUsage::Uniform:
			return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
		case BufferUsage::SSBODevice:
			return VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		case BufferUsage::SSBOHost:
			return VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
		default:
			DEBUG_ASSERT(false);
	}
	return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
}

inline VkMemoryPropertyFlags Conv2MemFlags(const BufferUsage& m)
{
	switch (m)
	{
		case BufferUsage::Vertex:
		case BufferUsage::Index:
		case BufferUsage::Texture:
			return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		case BufferUsage::Staging:
			return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		case BufferUsage::Uniform:
		case BufferUsage::SSBOHost:
			return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
		case BufferUsage::SSBODevice:
			return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		case BufferUsage::GenericDeviceVisible:
			return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		default:
			DEBUG_ASSERT(false);
	}
	return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
}

inline VkShaderStageFlagBits Conv(const ShaderTypeBits& m)
{
	u32 vkBits = 0;
	if ((u32)m & (u32)ShaderTypeBits::Vertex)
		vkBits = (vkBits | VK_SHADER_STAGE_VERTEX_BIT);
	if ((u32)m & (u32)ShaderTypeBits::Fragment)
		vkBits = (vkBits | VK_SHADER_STAGE_FRAGMENT_BIT);
	if((u32)m & (u32)ShaderTypeBits::Compute)
		vkBits = (vkBits | VK_SHADER_STAGE_COMPUTE_BIT);
	if ((u32)m & (u32)ShaderTypeBits::Geometry)
		vkBits = (vkBits | VK_SHADER_STAGE_GEOMETRY_BIT);
	if ((u32)m & (u32)ShaderTypeBits::All)
		vkBits = VK_SHADER_STAGE_ALL;

	return (VkShaderStageFlagBits)vkBits;
}

inline VkClearValue AttachTypeToClearVal(const AttachmentType& m)
{
	switch (m)
	{
		case AttachmentType::GBufferColor:
			return g_BlackCLearColor;
		case AttachmentType::DepthStencil:
			return g_WhiteCLearColor;
		default:
			DEBUG_ASSERT(false);
			break;
	}
	return g_BlackCLearColor;
}
