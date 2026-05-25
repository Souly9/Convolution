#pragma once
#include <imgui.h>

namespace ImGui
{
    inline void CaptureMouseFromApp(bool want_capture_mouse = true)
    {
        SetNextFrameWantCaptureMouse(want_capture_mouse);
    }
}
