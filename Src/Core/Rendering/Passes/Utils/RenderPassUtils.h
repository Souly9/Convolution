#pragma once
#include "Core/Rendering/Core/RenderingIncludes.h"

static inline bool NeedToRender(const IndirectDrawCmdBuf& buffer)
{
    if (buffer.GetDrawCmdNum() == 0)
        return false;
    return true;
}
