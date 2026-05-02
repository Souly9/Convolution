#pragma once
#include "Core/Global/GlobalVariables.h"
#include "Core/Global/Profiling.h"
#include "Core/Rendering/Core/Nvidia/StreamlineManager.h"
#include "InfoWindow.h"

class DLSSDebugWindow : public ImGuiWindow
{
public:
    DLSSDebugWindow()
    {
        m_isOpen = false;
    }

    void DrawWindow(f32 dt)
    {
        ScopedZone("DLSSDebugWindow");
        if (!m_isOpen)
            return;

        const auto& renderState = g_pApplicationState->GetCurrentApplicationState().renderState;
        const auto debugState = Nvidia::StreamlineManager::GetDLSSDebugState();

        ImGui::SetNextWindowSize(ImVec2(460.0f, 280.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("DLSS Debug", &m_isOpen))
        {
            ImGui::End();
            return;
        }

        ImGui::TextWrapped("NVIDIA Streamline ImGui is loaded as the sl.imgui plugin. Use Ctrl+Shift+Home to toggle the Streamline overlay. The engine hides its own ImGui while that overlay is active so it can receive input. Ctrl+Shift+Insert is only useful for plugins that expose a buffer visualizer, such as DLSS-G.");
        ImGui::Separator();

        ImGui::Text("AA Mode: %s", renderState.aaType == AntialiasingType::DLSS ? "DLSS" : "Not DLSS");
        ImGui::Text("Streamline Initialized: %s", BoolToString(debugState.streamlineInitialized));
        ImGui::Text("DLSS Feature Supported: %s", BoolToString(debugState.featureSupported));
        ImGui::Text("Streamline ImGui Plugin: %s", BoolToString(debugState.imguiPluginAvailable));
        ImGui::Text("Configured: %s", BoolToString(debugState.configured));
        ImGui::Text("Evaluate Blocked: %s", BoolToString(debugState.evaluateBlocked));
        ImGui::Text("Last Configure Failed: %s", BoolToString(debugState.lastConfigureFailed));
        ImGui::Text("Pending Reset Flag: %s", BoolToString(debugState.needsReset));
        ImGui::Text("Configured Mode: %s", DLSSModeToString(debugState.configuredMode));

        ImGui::Separator();
        ImGui::Text("DLSS Input: %u x %u", debugState.inputWidth, debugState.inputHeight);
        ImGui::Text("DLSS Output: %u x %u", debugState.outputWidth, debugState.outputHeight);
        ImGui::Text("Estimated VRAM Usage: %.2f MB",
                    static_cast<f32>(debugState.estimatedVRAMUsageInBytes) / (1024.0f * 1024.0f));
        ImGui::Text("Evaluate Calls: %llu", debugState.evaluateCallCount);
        ImGui::Text("slSetConstants: %s (0x%X)",
                    ResultToString(debugState.lastSetConstantsResult),
                    static_cast<u32>(debugState.lastSetConstantsResult));
        ImGui::Text("slSetTagForFrame: %s (0x%X)",
                    ResultToString(debugState.lastTagResult),
                    static_cast<u32>(debugState.lastTagResult));
        ImGui::Text("slEvaluateFeature: %s (0x%X)",
                    ResultToString(debugState.lastEvaluateResult),
                    static_cast<u32>(debugState.lastEvaluateResult));

        ImGui::End();
    }

private:
    static const char* BoolToString(bool value)
    {
        return value ? "Yes" : "No";
    }

    static const char* DLSSModeToString(sl::DLSSMode mode)
    {
        switch (mode)
        {
            case sl::DLSSMode::eOff: return "Off";
            case sl::DLSSMode::eMaxPerformance: return "Max Performance";
            case sl::DLSSMode::eBalanced: return "Balanced";
            case sl::DLSSMode::eMaxQuality: return "Max Quality";
            case sl::DLSSMode::eUltraPerformance: return "Ultra Performance";
            case sl::DLSSMode::eUltraQuality: return "Ultra Quality";
            case sl::DLSSMode::eDLAA: return "DLAA";
            default: return "Unknown";
        }
    }

    static const char* ResultToString(sl::Result result)
    {
        switch (result)
        {
            case sl::Result::eOk: return "Ok";
            case sl::Result::eErrorNGXFailed: return "NGX Failed";
            case sl::Result::eErrorNotInitialized: return "Not Initialized";
            case sl::Result::eErrorInvalidParameter: return "Invalid Parameter";
            case sl::Result::eErrorFeatureNotSupported: return "Feature Not Supported";
            case sl::Result::eErrorMissingConstants: return "Missing Constants";
            case sl::Result::eErrorInvalidState: return "Invalid State";
            case sl::Result::eWarnOutOfVRAM: return "Out Of VRAM";
            default: return "Other";
        }
    }
};
