#pragma once
#include "Core/Global/Typedefs.h"
#include "Core/Rendering/Core/CommandBuffer.h"
#include "Core/Rendering/Core/DescriptorSetLayout.h"
#include "Core/Rendering/Core/FrameResourceManager.h"
#include "Core/Rendering/Core/RenderDefinitions.h"
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

#include "Core/Global/Utils/MathFunctions.h"

enum class RGNodeFlags : u32
{
    None           = 0,
    RequiresRT     = 1u << 0,
    HasSideEffects = 1u << 1,
    IsOpaque       = 1u << 2,
    IsCulled       = 1u << 3
};

struct RGNode
{
    stltype::string name;
    QueueType queueType{QueueType::Graphics};
    PassStage stage{PassStage::MainGeometry};
    ExclusionGroup exclusionGroup{ExclusionGroup::None};
    u32 viewMask{1};

    u32 GetViewCount() const
    {
        u32 v = viewMask;
        u32 count = 0;
        while (v > 0)
        {
            count += (v & 1u);
            v >>= 1u;
        }
        return count > 0 ? count : 1;
    }

    u32 flags{0};

    bool RequiresRT() const { return mathstl::isFlagSet(flags, (u32)RGNodeFlags::RequiresRT); }
    void SetRequiresRT(bool v = true) { mathstl::setFlag(flags, (u32)RGNodeFlags::RequiresRT, v); }

    bool HasSideEffects() const { return mathstl::isFlagSet(flags, (u32)RGNodeFlags::HasSideEffects); }
    void SetHasSideEffects(bool v = true) { mathstl::setFlag(flags, (u32)RGNodeFlags::HasSideEffects, v); }

    bool IsOpaque() const { return mathstl::isFlagSet(flags, (u32)RGNodeFlags::IsOpaque); }
    void SetIsOpaque(bool v = true) { mathstl::setFlag(flags, (u32)RGNodeFlags::IsOpaque, v); }

    bool IsCulled() const { return mathstl::isFlagSet(flags, (u32)RGNodeFlags::IsCulled); }
    void SetIsCulled(bool v = true) { mathstl::setFlag(flags, (u32)RGNodeFlags::IsCulled, v); }

    stltype::fixed_vector<RGResourceAccess, 16> reads;
    stltype::fixed_vector<RGResourceAccess, 16> writes;

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
