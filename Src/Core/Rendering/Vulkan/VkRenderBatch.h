#pragma once
#include "Core/Global/GlobalDefines.h"
#include "Core/Rendering/Core/RenderBatch.h"

// Isn't responsible for freeing or managing the globals, just provides access
class VkRenderBatch : public RBatch
{
public:
    void Start();
    void DispatchDraw();
    void End();
};