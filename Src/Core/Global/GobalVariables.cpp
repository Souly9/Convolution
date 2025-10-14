#include "GlobalDefines.h"
#include "GlobalVariables.h"

extern threadSTL::Semaphore g_mainRenderThreadSyncSemaphore{0};
extern threadSTL::Semaphore g_renderThreadReadSemaphore{ 0 };
extern threadSTL::Semaphore g_frameTimerSemaphore{ 0 }; 
extern threadSTL::Semaphore g_frameTimerSemaphore2{ 0 };
extern threadSTL::Semaphore g_imguiSemaphore{ 0 };

extern stltype::unique_ptr<EventSystem> g_pEventSystem = stltype::make_unique<EventSystem>();
extern stltype::unique_ptr<WindowManager> g_pWindowManager = nullptr;
extern stltype::unique_ptr<ConsoleLogger> g_pConsoleLogger = stltype::make_unique<ConsoleLogger>();
extern stltype::unique_ptr<TimeData> g_pGlobalTimeData = stltype::make_unique<TimeData>();
extern stltype::unique_ptr<FileReader> g_pFileReader = stltype::make_unique<FileReader>();
extern stltype::unique_ptr<MaterialManager> g_pMaterialManager = stltype::make_unique<MaterialManager>();
extern stltype::unique_ptr<AsyncQueueHandler> g_pQueueHandler = stltype::make_unique<AsyncQueueHandler>();
extern stltype::unique_ptr<DeleteQueue> g_pDeleteQueue = stltype::make_unique<DeleteQueue>();
extern stltype::unique_ptr<ECS::EntityManager> g_pEntityManager = stltype::make_unique<ECS::EntityManager>();
extern stltype::unique_ptr<TextureManager> g_pTexManager = stltype::make_unique<TextureManager>();
extern stltype::unique_ptr<MeshManager> g_pMeshManager = stltype::make_unique<MeshManager>();
extern ApplicationStateManager* g_pApplicationState = nullptr;
extern stltype::unique_ptr<ShaderManager> g_pShaderManager = stltype::make_unique<ShaderManager>();
extern u32 g_currentFrameNumber = 0;
extern DirectX::XMUINT2 g_swapChainExtent = { 0,0 };