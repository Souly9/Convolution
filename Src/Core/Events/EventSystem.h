#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/ECS/Entity.h"
#include "Core/ECS/Systems/System.h"
#include "EventUtils.h"
#include "ApplicationEvents.h"

struct ApplicationState;
struct UpdateEventData
{
	ApplicationState state;
	f32 dt;
};
using UpdateEventCallback = stltype::fixed_function<16, void(const UpdateEventData&)>;

struct MouseEventData
{
	ECS::Entity selectedEntity;
};
using MouseEventCallback = stltype::fixed_function<16, void(const MouseEventData&)>;

struct BaseInitEventData
{
};
using BaseInitEventCallback = stltype::fixed_function<16, void(const BaseInitEventData&)>;
struct AppInitEventData
{
	ECS::System::SystemInitData systemInitData;
};
using AppInitEventCallback = stltype::fixed_function<16, void(const AppInitEventData&)>;


// Simple class to define a base eventsystem
class EventSystem
{
public:
	EVENT_FUNCTIONS(Mouse);
	EVENT_FUNCTIONS(Update);
	EVENT_FUNCTIONS(BaseInit);
	EVENT_FUNCTIONS(AppInit);
	EVENT_FUNCTIONS(WindowResize);
protected:
	EVENT(Mouse);
	EVENT(Update);
	EVENT(BaseInit);
	EVENT(AppInit);
	EVENT(WindowResize);
};