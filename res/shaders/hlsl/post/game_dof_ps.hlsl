// HLSL source of features/post/game_dof.frag (fragment shader), lane HLSLSRC3 (notes/hlslsrc3/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "PostBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
#ifndef WW_GLSL_SIZE_Texture2D_float4_
#define WW_GLSL_SIZE_Texture2D_float4_
int2 textureSize(Texture2D<float4> t, int lod) { uint w, h, l; t.GetDimensions(uint(lod), w, h, l); return int2(w, h); }
#endif
static float4 gl_FragCoord;
/* lane GRASSMB1: the game's depth of field composite, instruction for instruction (src/features/post/cellpost.h).
 * t0 the frame, t1 its blur, t2 the depth; c0 = (range, range, distance, -), c1 = (strength, near on, far on,
 * sky sharp), c2 = (-1e8, near, far - near, far * near), c3.zw the blur's uv scale (1 here).
 * The depth is this view's GL window depth: with the default depth range it equals the D3D depth the game's
 * formula expects, far * near / ((1 - d)(far - near) + near) = the view distance.
 * farOnly = the technique the effect picks when (flags & 3) == 2 (no near test, no near/far switches).
 * dofRed = the gate's red: the factor ignores the depth (strength everywhere). */

Texture2D<float4> t0 : register(t1, space0); SamplerState _t0_sampler : register(s1, space0);
Texture2D<float4> t1 : register(t2, space0); SamplerState _t1_sampler : register(s2, space0);
Texture2D<float4> t2 : register(t3, space0); SamplerState _t2_sampler : register(s3, space0);
#define WW_LOOSE_TYPES 1
#if defined(WW_DOF_LENS)
#if defined(WW_DOF_LENS)
#ifndef LENS_FRAME
#define LENS_FRAME t0	// as below, before this file
#endif
#endif
#include "game_doflens.hlsli"
#endif
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#if defined(WW_DOF_LENS)
#if defined(WW_DOF_LENS)
#ifndef LENS_FRAME
#define LENS_FRAME t0	// as below, before this file
#endif
#endif
#include "game_doflens.hlsli"
#endif
#undef WW_LOOSE_UNIFORMS
	float4 c0;
	float4 c1;
	float4 c2;
	float4 c3;
	int farOnly;
	int dofRed;
};


static float4 fragColor;

float sat( float v )	// D3D _sat: NaN -> 0
{
	return v >= 0.0 ? min( v, 1.0 ) : 0.0;
}

// lane DOFLENS1: the thin-lens composite (twin game_doflens.frag; t1 = the far layer)
#ifdef WW_DOF_LENS
#define LENS_FRAME t0
#define _LENS_FRAME_sampler _t0_sampler
#define LENS_FAR t1
#define _LENS_FAR_sampler _t1_sampler
#define LENS_DEPTH t2
#define _LENS_DEPTH_sampler _t2_sampler
#include "game_doflens.hlsli"
#endif

void frag_main()
{
#ifdef WW_DOF_LENS
	lensS1 = lensFocus( LENS_DEPTH, _LENS_DEPTH_sampler );
	fragColor = lensTexel( int2( gl_FragCoord.xy ) );
	return;
#endif
	float2 v1 = gl_FragCoord.xy / float2( textureSize( t0, 0 ) );
	float x = 1.0 - t2.Sample( _t2_sampler, v1 ).r;		// add r0.x, -r0.x, 1
	float y = x * c2.z + c2.y;					// mad
	bool notSky = 0.00001 < x;					// lt
	y = c2.w / y;								// div: the view distance
	float k;
	if ( farOnly != 0 ) {
		k = sat( ( y - c0.z ) / c0.y );			// add, div_sat
		k = k * c1.x;
		notSky = or( notSky, !( 0.0 != c1.w ) );		// ne, not, or
	} else {
		float nearT = ( c0.z - y ) / c0.x;
		bool nearOn = ( y < c0.z ) && ( 0.0 != c1.y );
		nearT = nearOn ? nearT : 0.0;			// and with the mask
		float farT = y - c0.z;
		bool farOn = ( c0.z < y ) && ( 0.0 != c1.z );
		notSky = or( notSky, !( 0.0 != c1.w ) );
		farT = farT / c0.y;
		k = sat( farOn ? farT : nearT );			// movc_sat
		k = k * c1.x;
	}
	if ( dofRed != 0 )
		k = c1.x;
	k = notSky ? k : 0.0;						// and r0.x, r0.y, r0.x
	float4 b = t1.Sample( _t1_sampler, v1 * c3.zw );
	float4 a = t0.Sample( _t0_sampler, v1 );
	fragColor = k * ( b - a ) + a;				// add, mad
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 fragColor : SV_Target0;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.fragColor = fragColor;
    return stage_output;
}
