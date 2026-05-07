#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/ThreadBase.h"
#include "Core/Rendering/Passes/PassManager.h"
#include "Core/UI/ImGui/ImGuiManager.h"
#include "Rendering/RenderLayer.h"
#include <atomic>

class RenderThread : public ThreadBase
{
public:
    RenderThread(ImGuiManager* pImGuiManager, RenderBackendImpl<RenderAPI>* pRenderBackend);
    void Stop()
    {
        m_keepRunning = false;
    }
    void WaitForGameThreadAndPreviousFrame();

    void RenderLoop();
    bool HandleResizeAtFrameStart();

    RenderPasses::PassManager* Start();
    void CleanUp();

    stltype::unique_ptr<RenderPasses::PassManager> m_passManager;
    ImGuiManager* m_pImGuiManager{};
    RenderBackendImpl<RenderAPI>* m_pRenderBackend{};
    std::atomic_bool m_swapchainRecreationRequested{false};
};
