#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/UI/LogData.h"
#include <EASTL/fixed_function.h>

using ImGuiRenderFunction = stltype::fixed_function<8, void(f32 dt, ApplicationInfos& appInfos)>;

class ImGuiManager
{
public:
    void CleanUp();

    void BeginFrame();
    void EndFrame();

    void RenderElements(f32 dt, ApplicationInfos& appInfos);

    static void RegisterRenderFunction(ImGuiRenderFunction&& renderFunction);

    // Rebuilds the default dock layout at the start of the next frame
    static void RequestLayoutReset();

private:
    static void BuildDefaultLayout(u32 dockspaceId);

    static stltype::vector<ImGuiRenderFunction> s_registeredFunctions;
    static bool s_streamlineOverlayInputMode;
    static bool s_layoutResetRequested;
};
