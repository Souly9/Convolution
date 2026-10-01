// Vulkan-only vendor SDK integration (Streamline/DLSS, XeSS): pass registration and debug UI.
// Shared code reaches it through g_renderer; non-Windows builds link VendorSdkStubs.cpp.
#include "VkVendorIntegration.h"
#include "Core/Global/State/States.h"
#include "Core/Rendering/Core/Nvidia/StreamlineManager.h"
#include "Core/Rendering/Passes/AA/DLSSPass.h"
#include "Core/Rendering/Passes/AA/XeSSPass.h"
#include "Core/Rendering/Passes/PassManager.h"
#include "Core/Rendering/Vulkan/XeSS/XeSSManager.h"
#include <imgui/imgui.h>

namespace
{
const char* BoolToString(bool value)
{
    return value ? "Yes" : "No";
}

const char* DLSSModeToString(sl::DLSSMode mode)
{
    switch (mode)
    {
        case sl::DLSSMode::eOff:
            return "Off";
        case sl::DLSSMode::eMaxPerformance:
            return "Max Performance";
        case sl::DLSSMode::eBalanced:
            return "Balanced";
        case sl::DLSSMode::eMaxQuality:
            return "Max Quality";
        case sl::DLSSMode::eUltraPerformance:
            return "Ultra Performance";
        case sl::DLSSMode::eUltraQuality:
            return "Ultra Quality";
        case sl::DLSSMode::eDLAA:
            return "DLAA";
        default:
            return "Unknown";
    }
}

const char* ResultToString(sl::Result result)
{
    switch (result)
    {
        case sl::Result::eOk:
            return "Ok";
        case sl::Result::eErrorNGXFailed:
            return "NGX Failed";
        case sl::Result::eErrorNotInitialized:
            return "Not Initialized";
        case sl::Result::eErrorInvalidParameter:
            return "Invalid Parameter";
        case sl::Result::eErrorFeatureNotSupported:
            return "Feature Not Supported";
        case sl::Result::eErrorMissingConstants:
            return "Missing Constants";
        case sl::Result::eErrorInvalidState:
            return "Invalid State";
        case sl::Result::eWarnOutOfVRAM:
            return "Out Of VRAM";
        default:
            return "Other";
    }
}

const char* VariantToString(Nvidia::DLSSVariant variant)
{
    return variant == Nvidia::DLSSVariant::RayReconstruction ? "Ray Reconstruction" : "Super Resolution";
}

stltype::string VersionToString(const sl::Version& version)
{
    return version ? stltype::string(version.toStr().c_str()) : stltype::string("n/a");
}
} // namespace

namespace VkVendor
{
bool IsDLSSSupported()
{
    return Nvidia::StreamlineManager::IsDLSSSupported();
}

bool IsDLSSRRSupported()
{
    return Nvidia::StreamlineManager::IsDLSSRRSupported();
}

bool IsXeSSSupported()
{
    return VulkanXeSS::XeSSManager::IsSupported();
}

bool IsDLSSDebugUIAvailable()
{
    return Nvidia::StreamlineManager::IsDLSSDebugUIAvailable();
}

void AddUpscalerPasses(RenderPasses::PassManager& passManager)
{
    if (Nvidia::StreamlineManager::IsDLSSSupported())
    {
        passManager.AddPass(stltype::make_unique<RenderPasses::DLSSExposurePass>());
        passManager.AddPass(stltype::make_unique<RenderPasses::DLSSPass>());
    }
    if (VulkanXeSS::XeSSManager::IsSupported())
    {
        passManager.AddPass(stltype::make_unique<RenderPasses::XeSSPass>());
    }
}

void BeginFrame(u32 frameIdx)
{
    Nvidia::StreamlineManager::AcquireNewFrameToken(frameIdx);
}

void DrawDiagnosticsUI(const RendererState& state)
{
    // DLSS & Streamline (Condensed)
    if (state.dlssSupported && ImGui::CollapsingHeader("DLSS & Streamline"))
    {
        const auto debugState = Nvidia::StreamlineManager::GetDLSSDebugState();

        ImGui::Text("Mode: %s | Streamline: %s | Overlay: %s",
                    (state.aaType == AntialiasingType::DLSS) ? DLSSModeToString(debugState.configuredMode) : "Off",
                    debugState.streamlineInitialized ? "Ready" : "No",
                    Nvidia::StreamlineManager::IsDLSSDebugUIAvailable() ? "Ctrl+Shift+Home" : "Off");

        ImGui::Text("Resolution: %u x %u -> %u x %u | VRAM: %.1f MB",
                    debugState.inputWidth, debugState.inputHeight,
                    debugState.outputWidth, debugState.outputHeight,
                    static_cast<f32>(debugState.estimatedVRAMUsageInBytes) / (1024.0f * 1024.0f));

        ImGui::Text("Calls: %llu eval | Tag: %s | Const: %s | Eval: %s",
                    debugState.evaluateCallCount,
                    ResultToString(debugState.lastTagResult),
                    ResultToString(debugState.lastSetConstantsResult),
                    ResultToString(debugState.lastEvaluateResult));
    }
}

void DrawSettingsUI()
{
    using SL = Nvidia::StreamlineManager;
    if (!ImGui::CollapsingHeader("DLSS / Streamline Debug"))
        return;
    if (!SL::IsAvailable())
    {
        ImGui::TextDisabled("Streamline is not initialized");
        return;
    }

    const auto state = SL::GetDLSSDebugState();
    const auto versions = SL::GetVersionInfo();
    auto settings = SL::GetDebugSettings();
    bool changed = false;

    ImGui::SeparatorText("Status");
    ImGui::Text("SDK %s | DLSS %s (NGX %s) | RR %s (NGX %s)",
                VersionToString(versions.sdk).c_str(),
                VersionToString(versions.dlssPlugin).c_str(),
                VersionToString(versions.dlssNGX).c_str(),
                VersionToString(versions.rrPlugin).c_str(),
                VersionToString(versions.rrNGX).c_str());
    const auto requirementSet = [](sl::FeatureRequirementFlags flags, sl::FeatureRequirementFlags bit)
    { return (static_cast<u32>(flags) & static_cast<u32>(bit)) != 0; };
    ImGui::Text("DLLs: %s | VSync off required: %s | HW scheduling required: %s",
                versions.developmentPlugins ? "development" : "production",
                BoolToString(requirementSet(versions.dlssFlags, sl::FeatureRequirementFlags::eVSyncOffRequired)),
                BoolToString(requirementSet(versions.dlssFlags, sl::FeatureRequirementFlags::eHardwareSchedulingRequired)));
    ImGui::Text("Running: %s | Mode: %s | %u x %u -> %u x %u | VRAM: %.1f MB",
                VariantToString(state.variant),
                DLSSModeToString(state.configuredMode),
                state.inputWidth,
                state.inputHeight,
                state.outputWidth,
                state.outputHeight,
                static_cast<f32>(state.estimatedVRAMUsageInBytes) / (1024.0f * 1024.0f));
    ImGui::Text("Optimal render: %u x %u (min %u x %u, max %u x %u)",
                state.optimalRenderWidth,
                state.optimalRenderHeight,
                state.renderWidthMin,
                state.renderHeightMin,
                state.renderWidthMax,
                state.renderHeightMax);
    ImGui::Text("Configured: %s | Configure failed: %s | Evaluate blocked: %s",
                BoolToString(state.configured),
                BoolToString(state.lastConfigureFailed),
                BoolToString(state.evaluateBlocked));
    ImGui::Text("Evaluating: %s | Reset this frame: %s | Exposure texture: %s | Calls: %llu",
                BoolToString(!state.bypassed),
                BoolToString(state.reset),
                BoolToString(state.exposureTextureTagged),
                static_cast<unsigned long long>(state.evaluateCallCount));
    ImGui::Text("Constants: %s | Tag: %s | Evaluate: %s",
                ResultToString(state.lastSetConstantsResult),
                ResultToString(state.lastTagResult),
                ResultToString(state.lastEvaluateResult));
    ImGui::Text("Jitter sent: (%.3f, %.3f) px | MV scale sent: (%.3f, %.3f)",
                state.jitter.x,
                state.jitter.y,
                state.motionVectorScale.x,
                state.motionVectorScale.y);

    ImGui::SeparatorText("Controls");
    changed |= ImGui::Checkbox("Bypass DLSS (copy input)", &settings.bypass);
    changed |= ImGui::Checkbox("Use exposure texture (off = DLSS auto exposure)", &settings.useExposureTexture);
    changed |= ImGui::Checkbox("Flip jitter X", &settings.flipJitterX);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Flip jitter Y", &settings.flipJitterY);
    changed |= ImGui::Checkbox("Override motion vector scale", &settings.overrideMotionVectorScale);
    if (settings.overrideMotionVectorScale)
        changed |= ImGui::DragFloat2("Motion vector scale", &settings.motionVectorScale.x, 0.01f, -4.0f, 4.0f);

    static const char* presetNames[] = {"Default", "J", "K", "L", "M"};
    static const sl::DLSSPreset presetValues[] = {sl::DLSSPreset::eDefault,
                                                  sl::DLSSPreset::ePresetJ,
                                                  sl::DLSSPreset::ePresetK,
                                                  sl::DLSSPreset::ePresetL,
                                                  sl::DLSSPreset::ePresetM};
    int presetIdx = 0;
    for (int i = 0; i < IM_ARRAYSIZE(presetValues); ++i)
    {
        if (presetValues[i] == settings.preset)
            presetIdx = i;
    }
    if (ImGui::Combo("SR Preset", &presetIdx, presetNames, IM_ARRAYSIZE(presetNames)))
    {
        settings.preset = presetValues[presetIdx];
        changed = true;
    }

    changed |= ImGui::Checkbox("Invert DLSS indicator X", &settings.invertIndicatorAxisX);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Invert DLSS indicator Y", &settings.invertIndicatorAxisY);

    static const char* verbosityNames[] = {"Off", "Errors", "Warnings", "Info"};
    int verbosity = static_cast<int>(settings.logVerbosity);
    if (ImGui::Combo("Streamline Log", &verbosity, verbosityNames, IM_ARRAYSIZE(verbosityNames)))
    {
        settings.logVerbosity = static_cast<SL::LogVerbosity>(verbosity);
        changed = true;
    }

    if (ImGui::Button("Reset DLSS History"))
    {
        ++settings.resetGeneration;
        changed = true;
    }

    if (changed)
        SL::SetDebugSettings(settings);

    ImGui::SeparatorText("Streamline Overlays");
    if (SL::IsDLSSDebugUIAvailable())
    {
        ImGui::TextWrapped("Present is routed through Streamline. Ctrl+Shift+Home: stats overlay, "
                           "Ctrl+Shift+Insert: tagged buffer view (keys in sl.common.json).");
    }
    else
    {
        ImGui::TextWrapped("Unavailable: needs the development DLLs (Debug/RelWithDebInfo) and CONV_SL_OVERLAY=1 "
                           "at startup, which routes swapchain and present through sl.interposer.");
    }
}
} // namespace VkVendor
