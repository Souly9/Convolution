#include "ApplicationState.h"

void ApplicationStateManager::ProcessStateUpdates()
{
	m_updateStateFutex.Lock();
	u32 currentState = m_currentState;
	currentState = ++currentState % MAX_STATES;
	// Don't change actual index before processing update functions
	auto newState = m_appStates[currentState];
	for (auto& updateFunction : m_updateFunctions)
	{
		updateFunction(newState);
	}
	m_updateFunctions.clear();
	for (auto& state : m_appStates)
	{
		state = newState;
	}
	m_updateStateFutex.Unlock();
	m_currentState = currentState;
}
void ApplicationStateManager::RegisterUpdateFunction(ApplicationStateUpdateFunction&& updateFunction)
{  
	m_updateStateFutex.Lock();
	m_updateFunctions.push_back(std::move(updateFunction));
	m_updateStateFutex.Unlock();
}