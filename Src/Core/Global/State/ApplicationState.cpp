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

void ApplicationStateManager::SetCurrentScene(stltype::unique_ptr<Scene>&& scene)
{
	DEBUG_ASSERT(scene != m_pCurrentScene);
	Scene* pRawScene = scene.release();
	pRawScene->Load();
	DEBUG_LOGF("Preparing to set current scene to: {}", pRawScene->GetName().c_str());
	RegisterUpdateFunction([pRawScene, this](auto& appState)
		{
			DEBUG_LOGF("Setting current scene to: {}", pRawScene->GetName().c_str());
			UnloadCurrentScene();
			m_pCurrentScene = stltype::unique_ptr<Scene>(pRawScene);
			DEBUG_LOGF("Loaded scene: {}", pRawScene->GetName().c_str())
			appState.pCurrentScene = pRawScene;
		});
}

void ApplicationStateManager::ReloadCurrentScene()
{
	DEBUG_ASSERT(GetCurrentScene() != nullptr);
	DEBUG_LOGF("Preparing to reload current scene: {}", GetCurrentScene()->GetName().c_str());
	RegisterUpdateFunction([this](auto& appState)
		{
			DEBUG_LOGF("Reloading current scene: {}", GetCurrentScene()->GetName().c_str());
			m_pCurrentScene->Unload();
			m_pCurrentScene->Load();
		});
}

void ApplicationStateManager::UnloadCurrentScene()
{
	DEBUG_LOGF("Unloading current scene");
	if(m_pCurrentScene)
		m_pCurrentScene.reset();
}
