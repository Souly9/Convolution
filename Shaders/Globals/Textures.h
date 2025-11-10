
#extension GL_EXT_nonuniform_qualifier : require
layout(set = 0, binding = GlobalBindlessTextureBufferSlot) uniform sampler2D GlobalBindlessTextures[];
layout(set = 0, binding = GlobalBindlessArrayTextureBufferSlot) uniform sampler2DArray GlobalBindlessArrayTextures[];