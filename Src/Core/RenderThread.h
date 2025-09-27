#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Global/ThreadBase.h"
#include "Core/UI/ImGui/ImGuiManager.h"
#include "Rendering/RenderLayer.h"
#include "Core/Rendering/Passes/PassManager.h"

class RenderThread : public ThreadBase
{
public:
	RenderThread(ImGuiManager* pImGuiManager);
	void Stop() { m_keepRunning = false; }
	void WaitForGameThreadAndPreviousFrame();

	void RenderLoop();

	RenderPasses::PassManager* Start();
	RenderPasses::PassManager m_passManager;
	ImGuiManager* m_pImGuiManager{};
};