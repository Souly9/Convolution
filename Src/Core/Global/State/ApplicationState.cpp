#include "ApplicationState.h"
#include <chrono>
#include "Core/Global/GlobalVariables.h"
#include "Core/Rendering/Core/MaterialManager.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/SceneGraph/Mesh.h"
#include "Core/IO/FileReader.h"
#include "Core/SceneGraph/Scene.h"
#include "Core/SceneGraph/SceneStreamer.h"
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

bool ApplicationStateManager::ExecuteSceneSwitchOnRenderThread()
{
    ScopedZone("ApplicationStateManager::ExecuteSceneSwitch");
    SimpleScopedGuard<CustomMutex> lock(m_updateStateFutex);

    if (!m_pNextScene && !m_reloadRequested.load(stltype::memory_order_relaxed))
    {
        m_sceneSwitchPending.store(false, stltype::memory_order_release);
        return false;
    }

    // Whatever is still streaming or decoding for the old scene is dropped when it is delivered
    g_engine.GetSceneStreamer().Cancel();
    g_engine.GetFileReader().BumpGeneration();

    // The one wait of a switch, nothing produces GPU work until the new scene loads
    const auto idleStart = std::chrono::steady_clock::now();
    SRF::WaitForDeviceIdle<RenderAPI>();
    g_renderer.GetQueueHandler().WaitForFences(~0u);
    const auto destroyStart = std::chrono::steady_clock::now();

    if (m_pCurrentScene)
    {
        m_pCurrentScene->Unload();
    }
    if (m_pNextScene)
    {
        m_pRetiredScene = std::move(m_pCurrentScene);
        m_pCurrentScene = std::move(m_pNextScene);
    }

    g_renderer.GetTextureManager().Flush();
    g_engine.GetMeshManager().Flush();
    g_renderer.GetMaterialManager().Flush();

    const auto destroyEnd = std::chrono::steady_clock::now();
    DEBUG_LOGF("Scene switch: {} ms waiting for the device, {} ms destroying the old scene",
               std::chrono::duration<f32, std::milli>(destroyStart - idleStart).count(),
               std::chrono::duration<f32, std::milli>(destroyEnd - destroyStart).count());

    // Before Load, scenes that set the camera themselves register their update function after this one
    m_updateFunctions.push_back([](ApplicationState& state)
    {
        state.selectedEntities.clear();
        state.mainCameraEntity = {};
    });

    DEBUG_LOGF("Loading scene: {}", m_pCurrentScene->GetName().c_str());
    m_pCurrentScene->Load();

    m_reloadRequested.store(false, stltype::memory_order_release);
    m_sceneSwitchPending.store(false, stltype::memory_order_release);
    return true;
}

void ApplicationStateManager::UnloadCurrentScene()
{
    ExecuteSceneSwitchOnRenderThread();
}
