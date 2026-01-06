#pragma once
#include "Core/Events/EventSystem.h"
#include "Core/Events/InputEvents.h"
#include "Core/Global/GlobalDefines.h"


namespace SelectedEntityMover
{
void RegisterCallbacks();
void OnKeyPress(const KeyPressEventData& data);
void OnScroll(const ScrollEventData& data);
void OnRightMouse(const RightMouseClickEventData& data);

void OnUpdate(const UpdateEventData& data);
} // namespace SelectedEntityMover