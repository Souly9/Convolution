#pragma once
#include "../GlobalDefines.h"
#include "States.h"

// Classes can register functions that recieve writeable application state and update it, usually executed at the end of the update cycle~before next one
using ApplicationStateUpdateFunction = stltype::function<void(ApplicationState& appState)>;

class ApplicationStateManager
{
public:
	const ApplicationState& GetCurrentApplicationState() { return m_appStates[m_currentState]; }

	void RegisterUpdateFunction(ApplicationStateUpdateFunction&& updateFunction);

	// Can't be called from multiple threads! Updates all states with the registered functions
	void ProcessStateUpdates();

private:
	static inline constexpr u32 MAX_STATES = 2;
	threadSTL::Futex m_updateStateFutex;
	// Double buffered application state to make multi threaded access easier
	stltype::fixed_vector<ApplicationState, MAX_STATES, false> m_appStates{ MAX_STATES };
	stltype::fixed_vector<ApplicationStateUpdateFunction, 32> m_updateFunctions;
	stltype::atomic<u8> m_currentState = 0;
};