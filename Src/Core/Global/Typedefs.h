#pragma once
#ifdef NO_PCH
#include <EASTL/type_traits.h>
#include <EASTL/vector.h>
#include <SimpleMath/SimpleMath.h>
#include <eathread/eathread.h>

#endif
#include <cstdint>

namespace stltype = eastl;
namespace threadstl = EA::Thread;
namespace mathstl = DirectX::SimpleMath;

using u8 = uint8_t;
using u32 = uint32_t;
using s32 = int32_t;
using u64 = uint64_t;
using f32 = float;
using f64 = double;

using ShaderID = u64;
using PipelineID = u32;
using C_ID = u64;
using TextureHandle = u32;