// ReSTIR DI's shared rules in HLSL (lane VKCOMPUTE1, 2026-11-12; from src/features/gi/rtddgi/restir_common.glsl). Shared,
// no bindings. Every function marked MIRROR is src/restirmath.h's function of that name; change all three or none.
// Grown with the kernels that use it (air rays: the luminance; restir: the reservoirs, hash streams, targets).
#ifndef RESTIR_COMMON_HLSLI
#define RESTIR_COMMON_HLSLI

// MIRROR restirmath.h luminance (Rec. 709)
float Restir_Lum( float r, float g, float b ) { return 0.2126 * r + 0.7152 * g + 0.0722 * b; }

#endif
