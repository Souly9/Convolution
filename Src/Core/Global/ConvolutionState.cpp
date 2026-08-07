#include "ConvolutionState.h"
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

stltype::unique_ptr<WindowManager> ConvolutionState::pWindowManager = nullptr;
stltype::unique_ptr<ConsoleLogger> ConvolutionState::pConsoleLogger = stltype::make_unique<ConsoleLogger>();
stltype::unique_ptr<TimeData> ConvolutionState::pGlobalTimeData = stltype::make_unique<TimeData>();
stltype::unique_ptr<FileReader> ConvolutionState::pFileReader = stltype::make_unique<FileReader>();
stltype::unique_ptr<EventSystem> ConvolutionState::pEventSystem = stltype::make_unique<EventSystem>();
stltype::unique_ptr<DeleteQueue> ConvolutionState::pDeleteQueue = stltype::make_unique<DeleteQueue>();
ApplicationStateManager* ConvolutionState::pApplicationState = nullptr;
stltype::unique_ptr<ECS::EntityManager> ConvolutionState::pEntityManager = stltype::make_unique<ECS::EntityManager>();
stltype::unique_ptr<ShaderManager> ConvolutionState::pShaderManager = stltype::make_unique<ShaderManager>();
stltype::unique_ptr<MaterialManager> ConvolutionState::pMaterialManager = stltype::make_unique<MaterialManager>();
stltype::unique_ptr<TextureManager> ConvolutionState::pTexManager = stltype::make_unique<TextureManager>();
stltype::unique_ptr<AsyncQueueHandler> ConvolutionState::pQueueHandler = stltype::make_unique<AsyncQueueHandler>();
stltype::unique_ptr<MeshManager> ConvolutionState::pMeshManager = stltype::make_unique<MeshManager>();
stltype::unique_ptr<TracyGPUManager> ConvolutionState::pTracyGPUManager = stltype::make_unique<TracyGPUManager>();

threadstl::Semaphore ConvolutionState::mainRenderThreadSyncSemaphore{0};
threadstl::Semaphore ConvolutionState::renderThreadReadSemaphore{0};
threadstl::Semaphore ConvolutionState::frameTimerSemaphore{0};
threadstl::Semaphore ConvolutionState::frameTimerSemaphore2{0};
threadstl::Semaphore ConvolutionState::imguiSemaphore{0};

u32 ConvolutionState::currentFrameNumber = 0;
mathstl::Vector2 ConvolutionState::swapChainExtent = {0, 0};
TexFormat ConvolutionState::swapChainFormat = TexFormat::R8G8B8A8_UNORM;
