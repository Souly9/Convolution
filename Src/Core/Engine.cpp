#include "Engine.h"
#include "Core/ConsoleLogger.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Events/EventSystem.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/IO/FileReader.h"
#include "Core/SceneGraph/Mesh.h"
#include "Core/SceneGraph/SceneStreamer.h"
#include "Core/WindowManager.h"

Engine::Engine() = default;
Engine::~Engine() = default;

void Engine::Init()
{
    // EventSystem before the EntityManager, whose constructor registers callbacks on it
    m_pConsoleLogger = stltype::make_unique<ConsoleLogger>();
    m_pFileReader = stltype::make_unique<FileReader>();
    m_pEventSystem = stltype::make_unique<EventSystem>();
    m_pApplicationState = stltype::make_unique<ApplicationStateManager>();
    m_pEntityManager = stltype::make_unique<ECS::EntityManager>();
    m_pMeshManager = stltype::make_unique<MeshManager>();
    m_pSceneStreamer = stltype::make_unique<SceneStreamer>();
}

void Engine::CreateMainWindow(u32 width, u32 height, stltype::string_view title)
{
    m_pWindowManager = stltype::make_unique<WindowManager>(width, height, title);
}

void Engine::StopIO()
{
    // Undelivered results are freed here, their callbacks would create entities, meshes and textures
    if (m_pFileReader)
    {
        m_pFileReader->Stop();
        m_pFileReader.reset();
    }
}

void Engine::DestroyApplicationState()
{
    m_pApplicationState.reset();
}

void Engine::ShutdownWorld()
{
    m_pSceneStreamer.reset();
    m_pEntityManager.reset();
    m_pMeshManager.reset();
}

void Engine::Shutdown()
{
    m_pEventSystem.reset();
    m_pWindowManager.reset();
    glfwTerminate();
    m_pConsoleLogger.reset();
}
