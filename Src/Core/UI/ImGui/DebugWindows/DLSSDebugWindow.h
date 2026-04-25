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

        ImGui::SetNextWindowSize(ImVec2(520.0f, 520.0f), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("DLSS Debug", &m_isOpen))
        {
            ImGui::End();
            return;
        }

        ImGui::TextWrapped("This window shows engine-side DLSS telemetry. NVIDIA's non-production sl.imgui plugin is not bundled in this repository, so the official Streamline ImGui overlay is not available here.");
        ImGui::Separator();

        ImGui::Text("AA Mode: %s", renderState.aaType == AntialiasingType::DLSS ? "DLSS" : "Not DLSS");
        ImGui::Text("Streamline Initialized: %s", BoolToString(debugState.streamlineInitialized));
        ImGui::Text("DLSS Feature Supported: %s", BoolToString(debugState.featureSupported));
        ImGui::Text("Configured: %s", BoolToString(debugState.configured));
        ImGui::Text("Evaluate Blocked: %s", BoolToString(debugState.evaluateBlocked));
        ImGui::Text("Last Configure Failed: %s", BoolToString(debugState.lastConfigureFailed));
        ImGui::Text("Pending Reset Flag: %s", BoolToString(debugState.needsReset));
        ImGui::Text("Configured Mode: %s", DLSSModeToString(debugState.configuredMode));

        if (ImGui::CollapsingHeader("Resolutions", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Render Resolution: %.0f x %.0f", renderState.renderResolution.x, renderState.renderResolution.y);
            ImGui::Text("Swapchain Resolution: %.0f x %.0f", renderState.swapchainResolution.x, renderState.swapchainResolution.y);
            ImGui::Text("DLSS Input: %u x %u", debugState.inputWidth, debugState.inputHeight);
            ImGui::Text("DLSS Output: %u x %u", debugState.outputWidth, debugState.outputHeight);
            ImGui::Text("Optimal Render: %u x %u", debugState.optimalRenderWidth, debugState.optimalRenderHeight);
            ImGui::Text("Render Range: %u x %u  ->  %u x %u",
                        debugState.renderWidthMin,
                        debugState.renderHeightMin,
                        debugState.renderWidthMax,
                        debugState.renderHeightMax);
            ImGui::Text("Optimal Sharpness: %.3f", debugState.optimalSharpness);
            ImGui::Text("Estimated VRAM Usage: %.2f MB",
                        static_cast<f32>(debugState.estimatedVRAMUsageInBytes) / (1024.0f * 1024.0f));
        }

        if (ImGui::CollapsingHeader("Constants", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("Jitter Offset: %.6f, %.6f", debugState.jitter.x, debugState.jitter.y);
            ImGui::Text("Motion Vector Scale: %.3f, %.3f",
                        debugState.motionVectorScale.x,
                        debugState.motionVectorScale.y);
            ImGui::Text("Near/Far: %.4f / %.4f", debugState.nearPlane, debugState.farPlane);
            ImGui::Text("FOV (rad): %.6f", debugState.fovRadians);
            ImGui::Text("Aspect Ratio: %.6f", debugState.aspectRatio);
            ImGui::Text("Camera Motion Included: %s", BoolToString(debugState.cameraMotionIncluded));
            ImGui::Text("Motion Vectors Jittered: %s", BoolToString(debugState.motionVectorsJittered));
            ImGui::Text("Depth Inverted: %s", BoolToString(debugState.depthInverted));
        }

        if (ImGui::CollapsingHeader("Results", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Text("slSetConstants: %s (0x%X)",
                        ResultToString(debugState.lastSetConstantsResult),
                        static_cast<u32>(debugState.lastSetConstantsResult));
            ImGui::Text("slSetTagForFrame: %s (0x%X)",
                        ResultToString(debugState.lastTagResult),
                        static_cast<u32>(debugState.lastTagResult));
            ImGui::Text("slEvaluateFeature: %s (0x%X)",
                        ResultToString(debugState.lastEvaluateResult),
                        static_cast<u32>(debugState.lastEvaluateResult));
        }

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
