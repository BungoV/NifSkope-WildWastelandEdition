// HLSL source of features/near/cell_vrs_rate.frag (fragment shader), lane HLSLSRC1 (notes/hlsltools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if none (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "NearBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
/* lane VRS1 (src/features/near/cellvrs.h): one fragment per shading-rate tile. The method of AMD FidelityFX-VariableShading
 * (MIT, GPUOpen): the largest step between neighbouring pixels of the finished picture across the tile (and its
 * one-pixel border), horizontal and vertical apart; an axis whose steps all stay under the allowance shades coarse
 * along it. The step is per channel (max), so a colour-only edge stays 1x1. Fast camera motion (the temporal AA's
 * vectors, in pixels a frame) raises the allowance, as FidelityFX's motion factor does; a still camera reads 0. */

Texture2D<float4> vrsSrc : register(t0, space0); SamplerState _vrsSrc_sampler : register(s0, space0);	// this frame's picture, resolved, display values
Texture2D<float4> vrsMv : register(t1, space0); SamplerState _vrsMv_sampler : register(s1, space0);	// previous minus current, texture units (read only when vrsMvOn)
cbuffer LooseUniforms : register(b31, space0)
{
	int vrsMvOn;
	float2 vrsSize;
	float2 vrsTile;
	float vrsCut;		// the opaque pass's allowance
	float vrsCutSorted;	// the sorted pass's (stricter, never past 2x2)
	int vrsMax;		// 2 or 4
	float vrsMotion;
};


static uint rateOpaque;
static uint rateSorted;

// palette indices (cellvrs.cpp): 0 1x1, 1 1x2, 2 2x1, 3 2x2, 4 2x4, 5 4x2, 6 4x4 (width x height)
uint pick( float2 d, float cut, int maxRate )
{
	int rx = d.x < cut ? 2 : 1;
	int ry = d.y < cut ? 2 : 1;
	if ( maxRate >= 4 ) {
		if ( d.x < cut * 0.25 && rx == 2 ) rx = 4;
		if ( d.y < cut * 0.25 && ry == 2 ) ry = 4;
	}
	if ( rx == 4 && ry == 1 ) rx = 2;	// no 4x1 / 1x4 in the hardware's set
	if ( ry == 4 && rx == 1 ) ry = 2;
	if ( rx == 1 ) return ry == 1 ? 0u : 1u;
	if ( ry == 1 ) return 2u;
	if ( rx == 2 ) return ry == 2 ? 3u : 4u;
	return ry == 2 ? 5u : 6u;
}

void frag_main()
{
	int2 size = int2( vrsSize );
	int2 tile = int2( vrsTile );
	int2 o = int2( gl_FragCoord.xy ) * tile;
	int2 hi = size - 1;
	float2 d = (float2)( 0.0 );
	for ( int y = -1; y < tile.y; y++ ) {
		for ( int x = -1; x < tile.x; x++ ) {
			int2 p = clamp( o + int2( x, y ), (int2)( 0 ), hi );
			float3 c = vrsSrc.Load( int3(p, 0) ).rgb;
			float3 r = vrsSrc.Load( int3(clamp( p + int2( 1, 0 ), (int2)( 0 ), hi ), 0) ).rgb;
			float3 u = vrsSrc.Load( int3(clamp( p + int2( 0, 1 ), (int2)( 0 ), hi ), 0) ).rgb;
			float3 dx = abs( c - r ), dy = abs( c - u );
			d = max( d, float2( max( dx.r, max( dx.g, dx.b ) ), max( dy.r, max( dy.g, dy.b ) ) ) );
		}
	}
	float k = 1.0;
	if ( vrsMvOn != 0 ) {
		int2 m = clamp( o + tile / 2, (int2)( 0 ), hi );
		float v = length( vrsMv.Load( int3(m, 0) ).xy * vrsSize );
		k = clamp( 1.0 + vrsMotion * max( v - 1.0, 0.0 ), 1.0, 4.0 );	// under a pixel a frame: none
	}
	rateOpaque = pick( d, vrsCut * k, vrsMax );
	rateSorted = pick( d, vrsCutSorted * k, 2 );
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    uint rateOpaque : SV_Target0;
    uint rateSorted : SV_Target1;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.rateOpaque = rateOpaque;
    stage_output.rateSorted = rateSorted;
    return stage_output;
}
