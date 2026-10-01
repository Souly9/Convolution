#include "ApplicationState.h"
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/MaterialManager.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Passes/PassManager.h"
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

    if (m_sceneSwitchPending.load(stltype::memory_order_acquire) && !m_isRenderThreadRunning.load(stltype::memory_order_acquire))
    {
        ExecuteSceneSwitchOnRenderThread();
    }

    newState.pCurrentScene = m_pCurrentScene.get();
    m_appStates[nextState] = newState;
    m_currentState.store(nextState, stltype::memory_order_release);
}

void ApplicationStateManager::SwitchSceneInternal()
{
    ExecuteSceneSwitchOnRenderThread();
}

void ApplicationStateManager::RegisterUpdateFunction(ApplicationStateUpdateFunction&& updateFunction)
{
    SimpleScopedGuard<CustomMutex> lock(m_updateStateFutex);
    m_updateFunctions.push_back(std::move(updateFunction));
}

void ApplicationStateManager::SetCurrentScene(stltype::unique_ptr<Scene>&& scene)
{
    DEBUG_ASSERT(scene != m_pCurrentScene);
    SimpleScopedGuard<CustomMutex> lock(m_updateStateFutex);
    m_pNextScene = std::move(scene);
    m_sceneSwitchPending.store(true, stltype::memory_order_release);
}

void ApplicationStateManager::ReloadCurrentScene()
{
    DEBUG_ASSERT(GetCurrentScene() != nullptr);
    DEBUG_LOGF("Preparing to reload current scene: {}", GetCurrentScene()->GetName().c_str());
    m_reloadRequested.store(true, stltype::memory_order_release);
    m_sceneSwitchPending.store(true, stltype::memory_order_release);
}

void ApplicationStateManager::ExecuteSceneSwitchOnRenderThread()
{
    SimpleScopedGuard<CustomMutex> lock(m_updateStateFutex);

    if (!m_pNextScene && !m_reloadRequested.load(stltype::memory_order_relaxed))
    {
        m_sceneSwitchPending.store(false, stltype::memory_order_release);
        return;
    }

    g_engine.GetFileReader().CancelAllRequests();
    g_engine.GetFileReader().FinishAllRequests();

    g_renderer.GetQueueHandler().FlushAllTransferCommands();
    SRF::WaitForDeviceIdle<RenderAPI>();

    if (m_pPassManager != nullptr)
    {
        m_pPassManager->ResetSceneState();
    }

    if (m_pCurrentScene)
    {
        m_pCurrentScene->Unload();
        if (!m_pNextScene && m_reloadRequested.load(stltype::memory_order_relaxed))
        {
            DEBUG_LOGF("Reloading current scene: {}", m_pCurrentScene->GetName().c_str());
            m_pCurrentScene->Load();
            m_reloadRequested.store(false, stltype::memory_order_release);
            m_sceneSwitchPending.store(false, stltype::memory_order_release);
            return;
        }
        m_pCurrentScene.reset();
    }

    g_renderer.GetTextureManager().Flush();
    g_engine.GetMeshManager().Flush();
    g_renderer.GetMaterialManager().Flush();

    if (m_pNextScene)
    {
        DEBUG_LOGF("Setting current scene to: {}", m_pNextScene->GetName().c_str());
        m_pNextScene->Load();
        m_pCurrentScene = std::move(m_pNextScene);
        DEBUG_LOGF("Loaded scene: {}", m_pCurrentScene->GetName().c_str());
    }

    m_reloadRequested.store(false, stltype::memory_order_release);
    m_sceneSwitchPending.store(false, stltype::memory_order_release);

    m_updateFunctions.push_back([](ApplicationState& state)
    {
        state.selectedEntities.clear();
        state.mainCameraEntity = {};
    });
}

void ApplicationStateManager::UnloadCurrentScene()
{
    ExecuteSceneSwitchOnRenderThread();
}
