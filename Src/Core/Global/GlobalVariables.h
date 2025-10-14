#pragma once
#include "GlobalDefines.h"
#include <EAThread/eathread_condition.h>
#include "Core/WindowManager.h"
#include "Core/ConsoleLogger.h"
#include "Core/TimeData.h"
#include "Core/IO/FileReader.h"
#include "Core/ECS/EntityManager.h"
#include "Core/Rendering/Core/Utils/DeleteQueue.h"
#include "State/ApplicationState.h"
#include "Core/Events/EventSystem.h"
#include "Core/Rendering/Core/ShaderManager.h"

extern threadSTL::Semaphore     g_mainRenderThreadSyncSemaphore;
extern threadSTL::Semaphore     g_renderThreadReadSemaphore;
extern threadSTL::Semaphore     g_frameTimerSemaphore;
extern threadSTL::Semaphore     g_frameTimerSemaphore2;
extern threadSTL::Semaphore     g_imguiSemaphore;

class MemoryManager;
class TextureMan;
class ConsoleLogger;
class TimeData;
class FileReader;
class DeleteQueue;
class EventSystem;
struct ApplicationState;
class ApplicationStateManager;
class ShaderManager;
class MaterialManager;
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

// Holds the frame number of the current frame (aka whether it's the first, second etc. frame of the swapchain)
extern u32 g_currentFrameNumber;
extern DirectX::XMUINT2 g_swapChainExtent;

namespace FrameGlobals
{
	static inline u32 GetFrameNumber() { return g_currentFrameNumber; }
	static inline void SetFrameNumber(u32 frameNumber) { g_currentFrameNumber = frameNumber; }
	static inline u32 GetPreviousFrameNumber(u32 frameNumber) { return --frameNumber % FRAMES_IN_FLIGHT; }

	static inline  const DirectX::XMUINT2& GetSwapChainExtent() { return g_swapChainExtent; }
	static inline  f32 GetScreenAspectRatio() { return static_cast<f32>(g_swapChainExtent.x) / static_cast<f32>(g_swapChainExtent.y); }
	static inline  void SetSwapChainExtent(const DirectX::XMUINT2& swapChainExtent) { g_swapChainExtent = swapChainExtent; }
}

#include "Core/Rendering/Core/TextureManager.h"
#include "Core/SceneGraph/Mesh.h"
#include "Core/Rendering/Core/Material.h"
#include "Core/Rendering/Core/MaterialManager.h"
#include "Core/Rendering/Core/TransferUtils/TransferQueueHandler.h"
