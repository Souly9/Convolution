#pragma once
#include "Input/SelectedEntityMover.h"

namespace StaticBehaviorCollection
{
	static void RegisterAllBehaviors()
	{
		SelectedEntityMover::RegisterCallbacks();
	}
}