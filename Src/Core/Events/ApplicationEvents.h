#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/ECS/Entity.h"
#include "Core/ECS/Systems/System.h"
#include "EventUtils.h"

struct WindowResizeEventData
{
	f32 x, y;
};
using WindowResizeEventCallback = stltype::fixed_function<16, void(const WindowResizeEventData&)>;
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