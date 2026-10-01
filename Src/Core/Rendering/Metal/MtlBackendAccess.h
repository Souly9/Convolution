#pragma once
#include "MtlForwardDecls.h"

// Device state of the Metal backend owned by g_renderer; Metal-only code reads it through these
namespace MtlBackend
{
MTL::Device* Device();
MTL::CommandQueue* GraphicsQueue();
CA::MetalLayer* Layer();
} // namespace MtlBackend
