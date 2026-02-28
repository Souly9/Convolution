#ifndef SHADERS_TYPES_H
#define SHADERS_TYPES_H

#ifdef __cplusplus
// Definitions for C++
// Expects mathstl::Vector2/3/4 and Matrix to be defined by whoever includes this
using vec2 = mathstl::Vector2;
using vec3 = mathstl::Vector3;
using vec4 = mathstl::Vector4;
using mat4 = mathstl::Matrix;
using uint = u32;

#define INOUT(type) type&
#define FUNC_QUALIFIER inline
#else
// Definitions for GLSL
#define INOUT(type) inout type
#define FUNC_QUALIFIER
#endif

#endif // SHADERS_TYPES_H
