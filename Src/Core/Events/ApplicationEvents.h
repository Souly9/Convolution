#pragma once
#include "Core/ECS/Entity.h"
#include "Core/ECS/Systems/System.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/State/ApplicationState.h"
#include "EventUtils.h"

struct NextFrameEventData
{
    u32 frameIdx;
};
using NextFrameEventCallback = stltype::fixed_function<8, void(const NextFrameEventData&)>;
struct PostFrameEventData
{
    u32 frameIdx;
};
using PostFrameEventCallback = stltype::fixed_function<8, void(const PostFrameEventData&)>;

struct ShaderHotReloadEventData
{
};
using ShaderHotReloadEventCallback = stltype::fixed_function<8, void(const ShaderHotReloadEventData&)>;

struct SwapchainRecreationEventData
{
};
using SwapchainRecreationEventCallback = stltype::fixed_function<8, void(const SwapchainRecreationEventData&)>;
struct SwapchainRecreatedEventData
{
    mathstl::Vector2 swapchainResolution{};
    mathstl::Vector2 renderResolution{};
    bool swapchainWasRecreated{false};
};
using SwapchainRecreatedEventCallback = stltype::fixed_function<16, void(const SwapchainRecreatedEventData&)>;
struct SceneLoadedEventData
{
};
using SceneLoadedEventCallback = stltype::fixed_function<16, void(const SceneLoadedEventData&)>;
