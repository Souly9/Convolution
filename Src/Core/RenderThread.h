#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/ThreadBase.h"
#include "Core/Rendering/Passes/PassManager.h"
#include "Core/UI/ImGui/ImGuiManager.h"
#include "Rendering/RenderLayer.h"

class RenderThread : public ThreadBase
{
public:
    RenderThread(ImGuiManager* pImGuiManager);
    void Stop()
    {
        m_keepRunning = false;
    }
    void WaitForGameThreadAndPreviousFrame();

    void RenderLoop();

    RenderPasses::PassManager* Start();
    void CleanUp();

    stltype::unique_ptr<RenderPasses::PassManager> m_passManager;
    ImGuiManager* m_pImGuiManager{};
};