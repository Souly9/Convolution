#ifndef LIGHTING_PASS_H
#define LIGHTING_PASS_H

#extension GL_ARB_shading_language_include : enable
#extension GL_EXT_nonuniform_qualifier : enable

// Unified sets for Lighting pass
#define SharedDataUBOSet     1
#define TransformSSBOSet     2
#define TileArraySet         3
#define GBufferUBOSet        4
#define ShadowViewUBOSet     5
#define PassPerObjectDataSet 10

#include "Types.h"
#include "BindingSlots.h"
#include "GlobalBuffers.h"
#include "PerObjectBuffers.h"
#include "Textures.h"
#include "GBufferUBO.h"
#include "LightData.h"
#include "ClusteredShading/LightIndexIO.h"
#include "PBR/DisneyPBR.h"
#include "ShadowBuffers.h"
#include "Utils/Shadows.h"
#include "Tonemapping/GT7.h"

#endif // LIGHTING_PASS_H
