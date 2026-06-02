#ifndef SHADERS_BINDLESS_H
#define SHADERS_BINDLESS_H

#extension GL_EXT_nonuniform_qualifier : require

#include "Common.h"

layout(set = 0, binding = GlobalBindlessTextureBufferSlot) uniform sampler2D GlobalBindlessTextures[];
layout(set = 0, binding = GlobalBindlessArrayTextureBufferSlot) uniform sampler2DArray GlobalBindlessArrayTextures[];
#ifndef BindlessImageSet
#define BindlessImageSet 1
#endif
layout(set = BindlessImageSet, binding = GlobalBindlessImageBufferSlot) uniform writeonly image2D GlobalBindlessImages[];

#endif // SHADERS_BINDLESS_H
 

