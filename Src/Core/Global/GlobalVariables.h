#pragma once
#include "GlobalDefines.h"
#include "FrameGlobals.h"
#include <EAThread/eathread_condition.h>

extern threadstl::Semaphore g_mainRenderThreadSyncSemaphore;
extern threadstl::Semaphore g_renderThreadReadSemaphore;
extern threadstl::Semaphore g_frameTimerSemaphore;
extern threadstl::Semaphore g_frameTimerSemaphore2;
extern threadstl::Semaphore g_imguiSemaphore;

class MemoryManager;
class TextureMan;
class ConsoleLogger;
class TimeData;
class WindowManager;
class FileReader;
class DeleteQueue;
class EventSystem;
struct ApplicationState;
class ApplicationStateManager;
class ShaderManager;
class MaterialManager;

#ifdef USE_VULKAN
class VkTextureManager;
using TextureManager = VkTextureManager;
#else
class TextureManager;
#endif

template <class BackendAPI>
    requires stltype::is_base_of_v<AvailableRenderBackends, BackendAPI>
class GPUMemManager;

class AsyncQueueHandler;
class MeshManager;
namespace ECS
{
class EntityManager;
}

extern stltype::unique_ptr<WindowManager> g_pWindowManager;
extern stltype::unique_ptr<ConsoleLogger> g_pConsoleLogger;
extern stltype::unique_ptr<TimeData> g_pGlobalTimeData;
extern stltype::unique_ptr<FileReader> g_pFileReader;
extern stltype::unique_ptr<EventSystem> g_pEventSystem;
extern stltype::unique_ptr<DeleteQueue> g_pDeleteQueue;
extern ApplicationStateManager* g_pApplicationState;
extern stltype::unique_ptr<ECS::EntityManager> g_pEntityManager;
extern stltype::unique_ptr<ShaderManager> g_pShaderManager;
extern stltype::unique_ptr<MaterialManager> g_pMaterialManager;
extern stltype::unique_ptr<TextureManager> g_pTexManager;
extern stltype::unique_ptr<AsyncQueueHandler> g_pQueueHandler;
extern stltype::unique_ptr<MeshManager> g_pMeshManager;

#ifdef USE_VULKAN
extern stltype::unique_ptr<GPUMemManager<Vulkan>> g_pGPUMemoryManager;
#else
// Define for other APIs if needed
#endif


