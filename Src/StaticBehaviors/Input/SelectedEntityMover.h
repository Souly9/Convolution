#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Events/InputEvents.h"

namespace SelectedEntityMover
{
	void RegisterCallbacks();
	void OnKeyPress(const KeyPressEventData& data);

	void OnUpdate(const UpdateEventData& data);
}