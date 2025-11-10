#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Utils/MemoryUtilities.h"
#include "Defines/DescriptorLayoutDefines.h"

enum class Topology
{
	TriangleList,
	TriangleStrip,
	Lines,
	LineStrip,
	Points
};

enum class PolygonMode
{
	Fill,
	Line,
	Point
};

enum class Cullmode
{
	None,
	Front,
	Back,
	Both
};

enum class FrontFace
{
	Clockwise,
	CounterClockwise
};

struct MultisampleInfo
{

};

struct ColorBlendAttachmentInfo
{

};

struct ColorBlendInfo
{
	u32 blendAttachmentCount = 0;
};

struct RasterizerInfo
{
	PolygonMode polyMode = PolygonMode::Fill; // GPU Feature required except for fill
	f32 lineWidth = 1.0;
	Cullmode cullmode = Cullmode::None;
	FrontFace frontMode = FrontFace::Clockwise;
	f32 depthBiasConstantFactor = 0.0;
	f32 depthBiasClamp = 0.0;
	f32 depthBiasSlopeFactor = 0.0;
	bool enableDepthClamp = false; // GPU Feature required
	bool discardEverythingBeforeRasterizer = false;
	bool depthBias = false;
};

struct DescriptorSetLayoutInfo
{
	stltype::vector<PipelineDescriptorLayout> sharedDescriptors;
	stltype::vector<PipelineDescriptorLayout> pipelineSpecificDescriptors;
};

struct PipelineAttachmentInfo
{
	stltype::vector<TexFormat> colorAttachments;
	TexFormat depthAttachmentFormat;
};

struct PushConstant
{
	ShaderTypeBits shaderUsage{ ShaderTypeBits::Vertex };
	u32 offset;
	u32 size;
};
struct PushConstantInfo
{
	stltype::vector<PushConstant> constants;
};
struct PipelineInfo
{
	PipelineAttachmentInfo attachmentInfos;
	PushConstantInfo pushConstantInfo{};
	ColorBlendInfo colorInfo{};
	ColorBlendAttachmentInfo colorBlendInfo{};
	MultisampleInfo multisampleInfo{};
	RasterizerInfo rasterizerInfo{};
	DirectX::XMFLOAT4 viewPortExtents;
	DescriptorSetLayoutInfo descriptorSetLayout;
	Topology topology{ Topology::TriangleList };
	u32 viewMask{ 0 };
	bool dynamicViewScissor{ true };
	bool hasDepth{ true };
};

static PipelineAttachmentInfo CreateAttachmentInfo(const stltype::vector<Texture>& colorAttachments, const Texture& depthAttachment)
{
	PipelineAttachmentInfo info{};
	info.colorAttachments.reserve(colorAttachments.size());
	for (const auto& colorTex : colorAttachments)
	{
		info.colorAttachments.push_back(colorTex.GetInfo().format);
	}
	info.depthAttachmentFormat = depthAttachment.GetInfo().format;
	return info;
}
static PipelineAttachmentInfo CreateAttachmentInfo(const stltype::vector<ColorAttachment>& colorAttachments)
{
	PipelineAttachmentInfo info{};
	info.colorAttachments.reserve(colorAttachments.size());
	for (const auto& colorAttachment : colorAttachments)
	{
		info.colorAttachments.push_back(colorAttachment.GetDesc().format);
	}
	return info;
}
static PipelineAttachmentInfo CreateAttachmentInfo(const stltype::vector<ColorAttachment>& colorAttachments, const DepthAttachment& depthAttachment)
{
	PipelineAttachmentInfo info = CreateAttachmentInfo(colorAttachments);
	info.depthAttachmentFormat = depthAttachment.GetDesc().format;
	return info;
}
