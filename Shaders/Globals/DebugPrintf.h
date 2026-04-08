#ifndef SHADERS_DEBUG_PRINTF_H
#define SHADERS_DEBUG_PRINTF_H

#ifndef CONV_SHADER_DEBUG_PRINTF
#define CONV_SHADER_DEBUG_PRINTF 1
#endif

#if CONV_SHADER_DEBUG_PRINTF
#extension GL_EXT_debug_printf : enable
#define CONV_SHADER_PRINTF_U32(formatLiteral, value) debugPrintfEXT(formatLiteral, uint(value))
#else
#define CONV_SHADER_PRINTF_U32(formatLiteral, value)
#endif

#endif // SHADERS_DEBUG_PRINTF_H
