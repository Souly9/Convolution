#pragma once
#include "Input/EntitySelector.h"
#include "Input/SelectedEntityMover.h"

namespace StaticBehaviorCollection
{
static void RegisterAllBehaviors()
{
    SelectedEntityMover::RegisterCallbacks();
    EntitySelector::RegisterCallbacks();
}
} // namespace StaticBehaviorCollection