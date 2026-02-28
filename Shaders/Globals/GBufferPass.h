#ifndef GBUFFER_CREATION_H
#define GBUFFER_CREATION_H

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable

// Unified sets for GBuffer pass
#define SharedDataUBOSet     1
#define TransformSSBOSet     2
#define PassPerObjectDataSet 3

#include "Types.h"
#include "BindingSlots.h"
#include "GlobalBuffers.h"
#include "PerObjectBuffers.h"
#include "Textures.h"
#include "DrawBuildBuffers.h"

#endif // GBUFFER_CREATION_H
