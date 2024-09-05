#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Utils/MemoryUtilities.h"

struct PipelineVertexInputs
{
	
};

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

};

struct PipelineUniformLayout
{

};

struct RasterizerInfo
{
	PolygonMode polyMode = PolygonMode::Fill; // GPU Feature required except for fill
	f32 lineWidth = 1.0;
	Cullmode cullmode = Cullmode::Back;
	FrontFace frontMode = FrontFace::Clockwise;
	f32 depthBiasConstantFactor = 0.0;
	f32 depthBiasClamp = 0.0;
	f32 depthBiasSlopeFactor = 0.0;
	bool enableDepthClamp = false; // GPU Feature required
	bool discardEverythingBeforeRasterizer = false;
	bool depthBias = false;
};

struct PipelineInfo
{
	PipelineUniformLayout uniformLayout;
	ColorBlendInfo colorInfo{};
	ColorBlendAttachmentInfo colorBlendInfo{};
	MultisampleInfo multisampleInfo{};
	RasterizerInfo rasterizerInfo{};
	DirectX::XMFLOAT4 viewPortExtents;
	Topology topology = Topology::TriangleList;
};

IMPLEMENT_GRAPHICS_API
class Pipeline
{

};

#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VkPipeline.h"
#endif

using PSO = Pipeline<RenderAPI>;