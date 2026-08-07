#pragma once
#include "GlobalDefines.h"
#include "Core/Rendering/Core/RenderDefinitions.h"
#include "Core/Rendering/Core/TracyManager.h"
#include <eathread/eathread_condition.h>

class WindowManager;
class ConsoleLogger;
class TimeData;
class FileReader;
class DeleteQueue;
class EventSystem;
class ApplicationStateManager;
class ShaderManager;
class MaterialManager;
class AsyncQueueHandler;
class MeshManager;

namespace ECS
{
class EntityManager;
}

#ifdef USE_VULKAN
class VkTextureManager;
using TextureManager = VkTextureManager;
#else
class TextureManager;
#endif

class ConvolutionState
{
public:
    // Subsystem Singletons
    static stltype::unique_ptr<WindowManager> pWindowManager;
    static stltype::unique_ptr<ConsoleLogger> pConsoleLogger;
    static stltype::unique_ptr<TimeData> pGlobalTimeData;
    static stltype::unique_ptr<FileReader> pFileReader;
    static stltype::unique_ptr<EventSystem> pEventSystem;
    static stltype::unique_ptr<DeleteQueue> pDeleteQueue;
    static ApplicationStateManager* pApplicationState;
    static stltype::unique_ptr<ECS::EntityManager> pEntityManager;
    static stltype::unique_ptr<ShaderManager> pShaderManager;
    static stltype::unique_ptr<MaterialManager> pMaterialManager;
    static stltype::unique_ptr<TextureManager> pTexManager;
    static stltype::unique_ptr<AsyncQueueHandler> pQueueHandler;
    static stltype::unique_ptr<MeshManager> pMeshManager;
    static stltype::unique_ptr<TracyGPUManager> pTracyGPUManager;

    // Synchronization Semaphores
    static threadstl::Semaphore mainRenderThreadSyncSemaphore;
    static threadstl::Semaphore renderThreadReadSemaphore;
    static threadstl::Semaphore frameTimerSemaphore;
    static threadstl::Semaphore frameTimerSemaphore2;
    static threadstl::Semaphore imguiSemaphore;

    // Frame Runtime State
    static u32 currentFrameNumber;
    static mathstl::Vector2 swapChainExtent;
    static TexFormat swapChainFormat;
};

class FrameGlobals
{
public:
    static inline u32 GetFrameNumber() { return ConvolutionState::currentFrameNumber; }
    static inline u32 GetPreviousFrameNumber(u32 frameIdx) { return (frameIdx + 2 - 1) % 2; }
    static inline void SetFrameNumber(u32 frame) { ConvolutionState::currentFrameNumber = frame; }

    static inline mathstl::Vector2 GetSwapChainExtent() { return ConvolutionState::swapChainExtent; }
    static inline void SetSwapChainExtent(const mathstl::Vector2& extent) { ConvolutionState::swapChainExtent = extent; }

    static inline TexFormat GetSwapChainFormat() { return ConvolutionState::swapChainFormat; }
    static inline void SetSwapChainFormat(TexFormat format) { ConvolutionState::swapChainFormat = format; }
};

// Aliases mapping legacy global symbols to ConvolutionState members
#define g_pWindowManager ConvolutionState::pWindowManager
#define g_pConsoleLogger ConvolutionState::pConsoleLogger
#define g_pGlobalTimeData ConvolutionState::pGlobalTimeData
#define g_pFileReader ConvolutionState::pFileReader
#define g_pEventSystem ConvolutionState::pEventSystem
#define g_pDeleteQueue ConvolutionState::pDeleteQueue
#define g_pApplicationState ConvolutionState::pApplicationState
#define g_pEntityManager ConvolutionState::pEntityManager
#define g_pShaderManager ConvolutionState::pShaderManager
#define g_pMaterialManager ConvolutionState::pMaterialManager
#define g_pTexManager ConvolutionState::pTexManager
#define g_pQueueHandler ConvolutionState::pQueueHandler
#define g_pMeshManager ConvolutionState::pMeshManager
#define g_pTracyGPUManager ConvolutionState::pTracyGPUManager

#define g_mainRenderThreadSyncSemaphore ConvolutionState::mainRenderThreadSyncSemaphore
#define g_renderThreadReadSemaphore ConvolutionState::renderThreadReadSemaphore
#define g_frameTimerSemaphore ConvolutionState::frameTimerSemaphore
#define g_frameTimerSemaphore2 ConvolutionState::frameTimerSemaphore2
#define g_imguiSemaphore ConvolutionState::imguiSemaphore

#define g_currentFrameNumber ConvolutionState::currentFrameNumber
#define g_swapChainExtent ConvolutionState::swapChainExtent
#define g_swapChainFormat ConvolutionState::swapChainFormat
