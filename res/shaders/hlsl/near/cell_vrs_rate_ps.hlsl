// HLSL source of cell_vrs_rate.frag, translated by notes/hlslsrc1/tools/glsl2hlsl.py (text level: comments, names and
// #if variants kept from the GLSL). Matrices hold GL's columns as rows (row_major blocks), so a GL product A * B
// reads mul(B, A).
// profile: ps_6_0
// variants: #if (the loader's WW_* defines select the variant; built with -D per set)
#include "NearBindingsVk.hlsli"
#include "../core/GlslCompat.hlsli"
#define WW_STAGE_PS 1
static float4 gl_FragCoord;

/* lane VRS1 (src/features/near/cellvrs.h): one fragment per shading-rate tile. The method of AMD FidelityFX-VariableShading
 * (MIT, GPUOpen): the largest step between neighbouring pixels of the finished picture across the tile (and its
 * one-pixel border), horizontal and vertical apart; an axis whose steps all stay under the allowance shades coarse
 * along it. The step is per channel (max), so a colour-only edge stays 1x1. Fast camera motion (the temporal AA's
 * vectors, in pixels a frame) raises the allowance, as FidelityFX's motion factor does; a still camera reads 0. */

Texture2D<float4> vrsSrc; SamplerState _vrsSrc_sampler;	// this frame's picture, resolved, display values
Texture2D<float4> vrsMv; SamplerState _vrsMv_sampler;	// previous minus current, texture units (read only when vrsMvOn)
uniform int vrsMvOn;
uniform float2 vrsSize;
uniform float2 vrsTile;
uniform float vrsCut;		// the opaque pass's allowance
uniform float vrsCutSorted;	// the sorted pass's (stricter, never past 2x2)
uniform int vrsMax;		// 2 or 4
uniform float vrsMotion;

static uint rateOpaque;
static uint rateSorted;

// stage I/O: the members (locations, semantics) of the generated port, each under its GLSL declaration's #if
struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};
struct SPIRV_Cross_Output
{
    uint rateOpaque : SV_Target0;
    uint rateSorted : SV_Target1;
};


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
	int2 size = (int2)(vrsSize);
	int2 tile = (int2)(vrsTile);
	int2 o = (int2)(gl_FragCoord.xy) * tile;
	int2 hi = size - 1;
	float2 d = (float2)(0.0);
	for ( int y = -1; y < tile.y; y++ ) {
		for ( int x = -1; x < tile.x; x++ ) {
			int2 p = clamp( o + int2( x, y ), (int2)(0), hi );
			float3 c = vrsSrc.Load(int3(p, 0)).rgb;
			float3 r = vrsSrc.Load(int3(clamp( p + int2( 1, 0 ), (int2)(0), hi ), 0)).rgb;
			float3 u = vrsSrc.Load(int3(clamp( p + int2( 0, 1 ), (int2)(0), hi ), 0)).rgb;
			float3 dx = abs( c - r ), dy = abs( c - u );
			d = max( d, float2( max( dx.r, max( dx.g, dx.b ) ), max( dy.r, max( dy.g, dy.b ) ) ) );
		}
	}
	float k = 1.0;
	if ( vrsMvOn != 0 ) {
		int2 m = clamp( o + tile / 2, (int2)(0), hi );
		float v = length( vrsMv.Load(int3(m, 0)).xy * vrsSize );
		k = clamp( 1.0 + vrsMotion * max( v - 1.0, 0.0 ), 1.0, 4.0 );	// under a pixel a frame: none
	}
	rateOpaque = pick( d, vrsCut * k, vrsMax );
	rateSorted = pick( d, vrsCutSorted * k, 2 );
}

// entry: copies the stage inputs into the GLSL-named globals, runs the GLSL main, returns the outputs
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
