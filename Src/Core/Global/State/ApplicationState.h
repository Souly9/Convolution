#pragma once
#include "../GlobalDefines.h"
#include "Core/Global/ThreadBase.h"
#include "Core/SceneGraph/Scene.h"
#include "States.h"

struct ApplicationState;

// Classes can register functions that recieve writeable application state and
// update it, usually executed at the end of the update cycle~before next one
using ApplicationStateUpdateFunction = stltype::function<void(ApplicationState& appState)>;

class ApplicationStateManager
{
public:
    const ApplicationState& GetCurrentApplicationState()
    {
        return m_appStates[m_currentState.load(stltype::memory_order_acquire)];
    }

    void RegisterUpdateFunction(ApplicationStateUpdateFunction&& updateFunction);

    void SetCurrentScene(stltype::unique_ptr<Scene>&& scene);
    const Scene* GetCurrentScene() const
    {
        return m_pCurrentScene.get();
    }
    void ReloadCurrentScene();
    
    bool HasPendingSceneSwitch() const
    {
        return m_sceneSwitchPending.load(stltype::memory_order_acquire);
    }

    void SetRenderThreadRunning(bool running)
    {
        m_isRenderThreadRunning.store(running, stltype::memory_order_release);
    }

    // Returns true when a scene was switched or reloaded
    bool ExecuteSceneSwitchOnRenderThread();

    // Can't be called from multiple threads! Updates all states with the
    // registered functions
    void ProcessStateUpdates();

private:
    static inline constexpr u32 MAX_STATES = 2;
    CustomMutex m_updateStateFutex;
    // Double buffered application state to make multi threaded access easier
    stltype::fixed_vector<ApplicationState, MAX_STATES, false> m_appStates{MAX_STATES};
    stltype::fixed_vector<ApplicationStateUpdateFunction, 32> m_updateFunctions;
    stltype::atomic<u8> m_currentState = 0;
    stltype::atomic<bool> m_sceneSwitchPending{false};
    stltype::atomic<bool> m_reloadRequested{false};
    stltype::atomic<bool> m_isRenderThreadRunning{false};
    stltype::unique_ptr<Scene> m_pCurrentScene{nullptr};
    // Scene switching has to be synchronized a bit differently as my design is
    // just too wonky
    stltype::unique_ptr<Scene> m_pNextScene{nullptr};
    // The UI may still hold the previous state's Scene*, so a replaced scene lives until the next switch
    stltype::unique_ptr<Scene> m_pRetiredScene{nullptr};
};