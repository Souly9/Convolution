#pragma once
#include "Core/Rendering/Core/RenderingForwardDecls.h"
#include "Core/Rendering/Core/RenderDefinitions.h"
#include "Core/Rendering/Core/Resource.h"
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
    CullMode cullmode = CullMode::NONE;
    FrontFace frontMode = FrontFace::CLOCKWISE;
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
    ShaderTypeBits shaderUsage{ShaderTypeBits::Vertex};
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
    Topology topology{Topology::TriangleList};
    u32 viewMask{0};
    bool dynamicViewScissor{true};
    bool hasDepth{true};
};

// Function declarations - implementations in Pipeline.cpp
PipelineAttachmentInfo CreateAttachmentInfo(const stltype::vector<Texture>& colorAttachments,
                                            const Texture& depthAttachment);
PipelineAttachmentInfo CreateAttachmentInfo(const stltype::vector<ColorAttachment>& colorAttachments);
PipelineAttachmentInfo CreateAttachmentInfo(const stltype::vector<ColorAttachment>& colorAttachments,
                                            const DepthAttachment& depthAttachment);

class PipelineBase : public TrackedResource
{
public:
    PipelineBase() = default;
    virtual ~PipelineBase() = default;
};

#include "Core/Rendering/Core/APITraits.h"
#ifdef USE_VULKAN
#include "Core/Rendering/Vulkan/VulkanTraits.h"
#include "Core/Rendering/Vulkan/VkPipeline.h"
#endif

template <typename API>
class ComputePipelineT : public APITraits<API>::ComputePipelineType
{
public:
    using APITraits<API>::ComputePipelineType::ComputePipelineType;
};

template <typename API>
class GraphicsPipelineT : public APITraits<API>::GraphicsPipelineType
{
public:
    using APITraits<API>::GraphicsPipelineType::GraphicsPipelineType;
};
