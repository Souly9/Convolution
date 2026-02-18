#include "GlobalVariables.h"
#include "Core/ConsoleLogger.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Events/EventSystem.h"
#include "Core/Global/State/ApplicationState.h"
#include "Core/IO/FileReader.h"
#include "Core/Rendering/Core/MaterialManager.h"
#include "Core/Rendering/Core/ShaderManager.h"
#include "Core/Rendering/Core/TextureManager.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
#include "Core/Rendering/Core/Utils/DeleteQueue.h"
#include "Core/SceneGraph/Mesh.h"
#include "Core/TimeData.h"
#include "Core/WindowManager.h"
#include "PCH.h"

threadstl::Semaphore g_mainRenderThreadSyncSemaphore{0};
threadstl::Semaphore g_renderThreadReadSemaphore{0};
threadstl::Semaphore g_frameTimerSemaphore{0};
threadstl::Semaphore g_frameTimerSemaphore2{0};
threadstl::Semaphore g_imguiSemaphore{0};

stltype::unique_ptr<EventSystem> g_pEventSystem = stltype::make_unique<EventSystem>();
stltype::unique_ptr<WindowManager> g_pWindowManager = nullptr;
stltype::unique_ptr<ConsoleLogger> g_pConsoleLogger = stltype::make_unique<ConsoleLogger>();
stltype::unique_ptr<TimeData> g_pGlobalTimeData = stltype::make_unique<TimeData>();
stltype::unique_ptr<FileReader> g_pFileReader = stltype::make_unique<FileReader>();
stltype::unique_ptr<MaterialManager> g_pMaterialManager = stltype::make_unique<MaterialManager>();
stltype::unique_ptr<AsyncQueueHandler> g_pQueueHandler = stltype::make_unique<AsyncQueueHandler>();
stltype::unique_ptr<DeleteQueue> g_pDeleteQueue = stltype::make_unique<DeleteQueue>();
stltype::unique_ptr<ECS::EntityManager> g_pEntityManager = stltype::make_unique<ECS::EntityManager>();
stltype::unique_ptr<TextureManager> g_pTexManager = stltype::make_unique<TextureManager>();
stltype::unique_ptr<MeshManager> g_pMeshManager = stltype::make_unique<MeshManager>();
ApplicationStateManager* g_pApplicationState = nullptr;
stltype::unique_ptr<ShaderManager> g_pShaderManager = stltype::make_unique<ShaderManager>();
u32 g_currentFrameNumber = 0;
TexFormat g_swapChainFormat = TexFormat::R16G16B16A16_FLOAT;
mathstl::Vector2 g_swapChainExtent = {0, 0};