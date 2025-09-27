#pragma once
#include "InfoWindow.h"

class StatsWindow : public ImGuiWindow
{
public:
	StatsWindow()
	{
		g_pEventSystem->AddUpdateEventCallback([this](const UpdateEventData& d) { OnUpdate(d); });
	}

	void DrawWindow(f32 dt)
	{
		stltype::string device = m_lastState.physicalRenderDeviceName;
		ImGui::Begin("RenderStats", &m_isOpen);
		ImGui::Text("Performance Stats");
		ImGui::Text("Avg FPS: %u", m_frameCount);
		ImGui::Text("Last deltatime: %.4f", m_lastDt);
		ImGui::Separator();
		ImGui::Text("Renderer State");
		ImGui::Text("Selected Rendering Device: %s", device.c_str());
		ImGui::End();
	}

	void OnUpdate(const UpdateEventData& d)
	{
		f32 dt = d.dt;
		const auto& state = d.state;
		m_lastDt = dt;
		m_lastState = state.renderState;
		if(m_totalTime < 1.f)
		{
			++m_curFrameCount;
			m_totalTime += dt;
		}
		else
		{
			m_frameCount = m_curFrameCount;
			m_curFrameCount = 0;
			m_totalTime = 0.f;
		}
	}

protected:
	RendererState m_lastState;
	f32 m_lastDt;
	u32 m_frameCount{ 0 };
	f32 m_totalTime{ 0.f };
	u32 m_curFrameCount{ 0 };
};