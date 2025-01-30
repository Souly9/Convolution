#pragma once
#include "Input/SelectedEntityMover.h"
#include "Input/EntitySelector.h"

namespace StaticBehaviorCollection
{
	static void RegisterAllBehaviors()
	{
		SelectedEntityMover::RegisterCallbacks();
		EntitySelector::RegisterCallbacks();
	}
}