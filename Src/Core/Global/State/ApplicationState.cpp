#include "ApplicationState.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/SceneGraph/Mesh.h"
#include "Core/IO/FileReader.h"
#include "Core/SceneGraph/Scene.h"
#include "Core/Rendering/Core/StaticFunctions.h"

void ApplicationStateManager::ProcessStateUpdates()
{
    u32 nextState = 0;
    ApplicationState newState;
    {
        SimpleScopedGuard<CustomMutex> lock(m_updateStateFutex);
        u32 currentState = m_currentState.load(stltype::memory_order_relaxed);
        nextState = (currentState + 1) % MAX_STATES;
        // Copy from active current state so all previous updates are preserved!
        newState = m_appStates[currentState];
        newState.renderState.stats = {};
        for (auto& updateFunction : m_updateFunctions)
        {
            updateFunction(newState);
        }
        m_updateFunctions.clear();
    }
    if (m_pNextScene != nullptr)
    {
        SwitchSceneInternal();
        m_pNextScene = nullptr;
        newState.pCurrentScene = m_pCurrentScene.get();
    }
    m_appStates[nextState] = newState;
    m_currentState.store(nextState, stltype::memory_order_release);
}
void ApplicationStateManager::SwitchSceneInternal()
{
    DEBUG_LOGF("Setting current scene to: {}", m_pNextScene->GetName().c_str());
    UnloadCurrentScene();
    m_pNextScene->Load();
    m_pCurrentScene = std::move(m_pNextScene);
    DEBUG_LOGF("Loaded scene: {}", m_pCurrentScene->GetName().c_str())
}
void ApplicationStateManager::RegisterUpdateFunction(ApplicationStateUpdateFunction&& updateFunction)
{
    SimpleScopedGuard<CustomMutex> lock(m_updateStateFutex);
    m_updateFunctions.push_back(std::move(updateFunction));
}

void ApplicationStateManager::SetCurrentScene(stltype::unique_ptr<Scene>&& scene)
{
    DEBUG_ASSERT(scene != m_pCurrentScene);
    m_pNextScene = std::move(scene);
}

void ApplicationStateManager::ReloadCurrentScene()
{
    DEBUG_ASSERT(GetCurrentScene() != nullptr);
    DEBUG_LOGF("Preparing to reload current scene: {}", GetCurrentScene()->GetName().c_str());
    RegisterUpdateFunction(
        [this](auto& appState)
        {
            DEBUG_LOGF("Reloading current scene: {}", GetCurrentScene()->GetName().c_str());
            m_pCurrentScene->Unload();
            m_pCurrentScene->Load();
        });
}

void ApplicationStateManager::UnloadCurrentScene()
{
    DEBUG_LOGF("Unloading current scene");
    if (m_pCurrentScene)
    {
        g_pFileReader->CancelAllRequests();
        g_pFileReader->FinishAllRequests();

        g_pQueueHandler->DispatchAllRequests();
        // Doesn't need to be fast nor do we want to to run into weird sync errors
        SRF::WaitForDeviceIdle<RenderAPI>();

        m_pCurrentScene.reset();

        g_pTexManager->Flush();
        g_pMeshManager->Flush();
    }
}
