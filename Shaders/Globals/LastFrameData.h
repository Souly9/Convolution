#ifndef SHADERS_LAST_FRAME_DATA_H
#define SHADERS_LAST_FRAME_DATA_H

#include "Types.h"
struct LastFrameData
{
    mat4 view;
    mat4 proj;
    mat4 viewProj; 
};

#endif // SHADERS_LAST_FRAME_DATA_H