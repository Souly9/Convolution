#ifndef SHADERS_TYPES_H
#define SHADERS_TYPES_H
#ifdef __cplusplus
#include <cstddef>
#include <type_traits>

// Definitions for C++
// Expects mathstl::Vector2/3/4 and Matrix to be defined by whoever includes this
using vec2 = mathstl::Vector2;
using vec3 = mathstl::Vector3;
using vec4 = mathstl::Vector4;
using mat4 = mathstl::Matrix;
using ivec2 = DirectX::XMINT2;
using ivec3 = DirectX::XMINT3;
using uint = u32;
using BindlessTextureHandle = u32;

#define INOUT(type)    type&
#define FUNC_QUALIFIER inline

namespace RenderFlags
{
inline bool IsFlagSet(u32 flags, u32 flag)
{
    return (flags & flag) != 0;
}
inline void SetFlag(u32& flags, u32 flag, bool value)
{
    if (value)
        flags |= flag;
    else
        flags &= ~flag;
}
} // namespace RenderFlags
#else
// Definitions for GLSL
#define INOUT(type) inout type
#define FUNC_QUALIFIER
#define BindlessTextureHandle uint
#endif

#define STRUCTDECL(name) struct name {
#define STRUCTFIELD(type, name) type name;
#define STRUCTFIELD_ARRAY(type, name, count) type name[count];
#define STRUCTEND() };
#endif // SHADERS_TYPES_H
 
