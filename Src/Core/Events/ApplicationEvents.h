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