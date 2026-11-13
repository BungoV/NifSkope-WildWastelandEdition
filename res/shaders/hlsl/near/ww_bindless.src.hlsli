// HLSL source of ww_bindless.glsl, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
#pragma once
#include "../core/GlslCompat.hlsli"
// lane BINDLESS2: the per-draw row (src/features/near/cellbindless.h WwBl::Row, std430, 176 bytes): the draw's matrices and the
// material's seven 2D maps as resident texture handles. wwDrawIdx is the draw's row in the ring at binding 12.
// lane BINDLESSMDI1: the row is wwDrawBase + the draw's base instance (0 for a plain draw; a multi-draw's command
// carries its own), handed to the fragment stage flat.
struct WwDraw
{
	row_major float4x4 mv;
	float4 nm[3];
	uint2 tex[8];
};
layout( std430, binding = 12 ) readonly buffer WwDrawBuf
{
	WwDraw wwDraws[];
};
uniform uint wwDrawBase;
#ifdef WW_BL_VERT
#ifdef GL_ARB_shader_draw_parameters
// lane FARLOOK1101 (FARINSTBL1): an instanced far ring's base instance is a placement row; it reads wwDrawBase alone
uniform bool wwBlUniformRow;
#define wwDrawIdx ( wwDrawBase + ( wwBlUniformRow ? 0u : uint( gl_BaseInstanceARB ) ) )
#else
#define wwDrawIdx wwDrawBase
#endif
static uint wwBlRow;
#else
static uint wwBlRow;
#define wwDrawIdx wwBlRow
#endif
