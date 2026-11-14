// HLSL source of features/near/ww_bindless.glsl (include-only GLSL), lane FO4MATCH1 part B: the GLSL's comments, names
// and order kept. The entry includes it three times: under WW_LOOSE_TYPES (none), inside its LooseUniforms cbuffer with
// WW_LOOSE_UNIFORMS defined (the vertex stage's row uniforms), and once plain (the row buffer, the row and the maps).
#ifdef WW_HLSL_SRC
#if defined(WW_LOOSE_TYPES)	// the types its loose uniforms use

#elif defined(WW_LOOSE_UNIFORMS)
#ifdef WW_BL_VERT
	uint wwDrawBase;
	// lane FARLOOK1101 (FARINSTBL1): an instanced far ring's base instance is a placement row; it reads wwDrawBase alone
	bool wwBlUniformRow;
#endif
#else
// lane BINDLESS2: the per-draw row (src/features/near/cellbindless.h WwBl::Row, std430, 176 bytes): the draw's matrices and the
// material's seven 2D maps as resident texture handles. wwDrawIdx is the draw's row in the ring at binding 12.
// lane BINDLESSMDI1: the row is wwDrawBase + the draw's base instance (0 for a plain draw; a multi-draw's command
// carries its own), handed to the fragment stage flat.
// GL: tex[i] is the 64-bit GL_ARB_bindless_texture handle; Vulkan / Fusion Shaders: .x the image index, .y the sampler index.
struct WwDraw
{
	row_major float4x4 mv;
	float4 nm[3];
	uint2 tex[8];
};
// GL binding 12 (tests/hlsllive/build_spv.py BLOCKS); Vulkan binding 12 as tests/vkframe/blview_ps.hlsl (free in set 0);
// DXIL t63: the core's t0-t62 are taken
#if defined(__spirv__) && defined(WW_VK_BINDLESS)
[[vk::binding(12, 0)]]
#endif
StructuredBuffer<WwDraw> wwDraws : register(t63, space0);
// the vertex stage sets it in main (wwDrawBase + the base instance, or wwDrawBase alone when wwBlUniformRow); the
// fragment stage takes it flat from the vertex stage
static uint wwBlRow;
#define wwDrawIdx wwBlRow
#ifndef WW_BL_VERT
// the material's maps: BaseMap ... EnvironmentMap and their _<map>_sampler names
#if defined(__spirv__) && defined(WW_GL_BINDLESS)
#include "ww_bindless_gl.hlsli"
#else
#include "ww_bindless_vk.hlsli"
#endif
#endif
#endif	// WW_LOOSE_UNIFORMS
#else	// WW_HLSL_SRC: the per-variant files' half (HLSLBULK1)
// Shared HLSL for res/shaders/features/near/ww_bindless.glsl (include-only GLSL), HLSLBULK1 round 2 (notes/hlslbulk1/tools/factor.py). No bindings: the
// functions below are the converted ones, they read the declarations of the shader that includes them (its cbuffers,
// resources, statics and LooseUniforms: the per-module shim and the entry own those). Define WW_FN_<name> for each
// wanted function before the #include (and #undef after), or WW_FN_ALL for all of them; a _V<n> suffix is a variant
// of the same GLSL function (a different define set at the entry). Fusion Shaders (F4FS) reuses the math from here.
// This GLSL holds declarations and macros only; the declarations live in the per-module shim.
#endif
