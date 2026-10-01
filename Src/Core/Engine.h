#pragma once
#include "Core/Global/BuildInfo.h"
#include "Core/Global/GlobalDefines.h"
#include "Core/TimeData.h"
#include <atomic>
#include <eathread/eathread_semaphore.h>

class WindowManager;
class ConsoleLogger;
class FileReader;
class EventSystem;
class ApplicationStateManager;
class MeshManager;
class SceneStreamer;
namespace ECS
{
class EntityManager;
}

// Main/render thread handshake
struct FrameSyncSemaphores
{
    threadstl::Semaphore mainRenderThreadSync{0};
    threadstl::Semaphore renderThreadRead{0};
    threadstl::Semaphore frameTimer{0};
    threadstl::Semaphore frameTimer2{0};
    threadstl::Semaphore imgui{0};
};

// Engine-wide CPU services. g_engine is a plain global: the constructor does nothing, main() drives the lifetime.
class Engine
{
public:
    Engine();
    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    void Init();
    void CreateMainWindow(u32 width, u32 height, stltype::string_view title);
    void StopIO();
    void DestroyApplicationState();
    void ShutdownWorld();
    void Shutdown();

    // Build facts, valid any time (also during static init)
    static constexpr Platform GetPlatform()
    {
        return kPlatform;
    }
    static constexpr bool IsWindows()
    {
        return kPlatform == Platform::Windows;
    }
    static constexpr bool IsMacOS()
    {
        return kPlatform == Platform::MacOS;
    }
    static constexpr bool IsLinux()
    {
        return kPlatform == Platform::Linux;
    }
    static constexpr Architecture GetArchitecture()
    {
        return kArchitecture;
    }

    WindowManager& GetWindowManager()
    {
        DEBUG_ASSERT(m_pWindowManager);
        return *m_pWindowManager;
    }
    ConsoleLogger* TryGetConsoleLogger()
    {
        return m_pConsoleLogger.get();
    }
    FileReader& GetFileReader()
    {
        DEBUG_ASSERT(m_pFileReader);
        return *m_pFileReader;
    }
    EventSystem& GetEventSystem()
    {
        DEBUG_ASSERT(m_pEventSystem);
        return *m_pEventSystem;
    }
    EventSystem* TryGetEventSystem()
    {
        return m_pEventSystem.get();
    }
    ApplicationStateManager& GetApplicationState()
    {
        DEBUG_ASSERT(m_pApplicationState);
        return *m_pApplicationState;
    }
    ApplicationStateManager* TryGetApplicationState()
    {
        return m_pApplicationState.get();
    }
    ECS::EntityManager& GetEntityManager()
    {
        DEBUG_ASSERT(m_pEntityManager);
        return *m_pEntityManager;
    }
    ECS::EntityManager* TryGetEntityManager()
    {
        return m_pEntityManager.get();
    }
    MeshManager& GetMeshManager()
    {
        DEBUG_ASSERT(m_pMeshManager);
        return *m_pMeshManager;
    }
    SceneStreamer& GetSceneStreamer()
    {
        DEBUG_ASSERT(m_pSceneStreamer);
        return *m_pSceneStreamer;
    }

    FrameSyncSemaphores& GetFrameSync()
    {
        return m_frameSync;
    }
    TimeData& GetTime()
    {
        return m_time;
    }
    f32 GetDeltaTime() const
    {
        return m_time.GetDeltaTime();
    }

    // Written on the main thread, read on the render thread
    u32 GetFrameNumber() const
    {
        return m_frameNumber.load(std::memory_order_relaxed);
    }
    void SetFrameNumber(u32 frame)
    {
        m_frameNumber.store(frame, std::memory_order_relaxed);
    }
    static constexpr u32 GetPreviousFrameNumber(u32 frameIdx)
    {
        return (frameIdx + FRAMES_IN_FLIGHT - 1) % FRAMES_IN_FLIGHT;
    }

private:
    stltype::unique_ptr<ConsoleLogger> m_pConsoleLogger;
    stltype::unique_ptr<FileReader> m_pFileReader;
    stltype::unique_ptr<EventSystem> m_pEventSystem;
    stltype::unique_ptr<ApplicationStateManager> m_pApplicationState;
    stltype::unique_ptr<ECS::EntityManager> m_pEntityManager;
    stltype::unique_ptr<MeshManager> m_pMeshManager;
    stltype::unique_ptr<SceneStreamer> m_pSceneStreamer;
    stltype::unique_ptr<WindowManager> m_pWindowManager;
    FrameSyncSemaphores m_frameSync;
    TimeData m_time;
    std::atomic<u32> m_frameNumber{0};
};

extern Engine g_engine;
