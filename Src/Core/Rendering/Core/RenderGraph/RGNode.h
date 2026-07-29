#pragma once
#include "Core/Global/Typedefs.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/DescriptorSetLayout.h"
#include "Core/Rendering/Core/FrameResourceManager.h"
#include "Core/Rendering/Core/RenderDefinitions.h"
#include "Core/Rendering/Core/TransferUtils/TransferDefines.h"
#include "RGExecutionContext.h"
namespace RenderPasses
{
struct MainPassData;
struct FrameRendererContext;
}

struct RGResourceAccess
{
    RGResourceHandle handle{kInvalidRGHandle};
    SyncStages stage{SyncStages::NONE};
    AccessFlags access{AccessFlags::NONE};
    ImageLayout layout{ImageLayout::UNDEFINED};
};

enum class ExclusionGroup : u8
{
    None = 0,
    TemporalAA,
    RayTracing
};

using RGExecuteCallback = stltype::fixed_function<128, void(const RenderPasses::MainPassData&, const RenderPasses::FrameRendererContext&, const RGExecutionContext&)>;
using RGContextResolver = stltype::fixed_function<64, stltype::vector<DescriptorSet::Ptr>(const RenderPasses::MainPassData&, const RenderPasses::FrameRendererContext&)>;

struct RGNode
{
    stltype::string name;
    QueueType queueType{QueueType::Graphics};
    ExclusionGroup exclusionGroup{ExclusionGroup::None};

    bool requiresRT{false};
    bool hasSideEffects{false};
    bool isOpaque{false};
    bool isCulled{false};

    stltype::fixed_vector<RGResourceAccess, 8> reads;
    stltype::fixed_vector<RGResourceAccess, 8> writes;

    stltype::vector<PipelineDescriptorLayout> declaredLayouts;

    RGContextResolver contextResolver;
    RGExecuteCallback executeCallback;

    struct LayoutOverride
    {
        RGResourceHandle handle{kInvalidRGHandle};
        ImageLayout layout{ImageLayout::UNDEFINED};
    };
    stltype::fixed_vector<LayoutOverride, 4> layoutOverrides;
};
