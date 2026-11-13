// HLSL source of features/near/fo4_defaultbl.vert (vertex shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: vs_6_8
#include "NearBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#define Sample( s, uv ) SampleLevel( s, uv, 0 )	// GLSL: texture() outside a fragment shader reads level 0
#define SampleCmp SampleCmpLevelZero	// the same for a shadow compare (SampleCmp is fragment-only)
#define GL_ARB_shader_draw_parameters 1	// the GLSL extension's macro (gl_BaseInstanceARB)

// lane BINDLESS2: fo4_default.vert reading its matrices from the per-draw row (ww_bindless.glsl)
#define WW_BINDLESS 1
#define WW_BL_VERT 1
#define vert_main wwBlMain
#include "../core/fo4_default_vs.hlsl"
#undef vert_main

// lane BINDLESSMDI1: the row once per vertex, so a multi-draw's fragments read their own draw's maps
void vert_main()
{
	wwBlRow = wwDrawIdx;
	wwBlMain();
}
