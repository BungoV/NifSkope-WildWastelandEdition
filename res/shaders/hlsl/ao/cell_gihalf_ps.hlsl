// HLSL source of features/ao/cell_gihalf.frag (fragment shader), lane HLSLSRC2 (notes/hlslsrc2/tools/glsl2hlsl.py): one readable file for every
// variant, the GLSL's comments, names and order kept. Loose uniforms are the LooseUniforms cbuffer (b31).
// variants: #if WW_CELL_FOGLEAK, WW_CELL_RESTIR, WW_CELL_RESTIRGI, WW_CELL_RTDDGI, WW_CELL_RTDDGI_CASCADES, WW_FRAME_UBO (the loader's defines; res/shaders/hlsl/variants.txt lists the sets built)
// profile: ps_6_0
#include "AoBindingsVk.hlsli"
#define WW_HLSL_SRC 1	// the shared .hlsli files give their readable half
static float4 gl_FragCoord;
/* lane PERF1: the screen GI, once per giStep x giStep pixels (2 half resolution, 1 full), at the pixel giStep x h of
 * lane AO1's shared opaque g-buffer (view normal, linear depth in game units; 1e6 empty). The volume read
 * (cellGiVolume) in the pixel's volume: the shapes' mask (cell_givol.frag), else the room grid. Out: the sample and its
 * key (world position, packed normal; w < 0 no surface), which the surfaces' cellGiSample upsamples (cell_lights.glsl).
 * Red WW_CELL_GI_VIS=march: the old chain (the pass does not run then; it still compiles). */

#define WW_CELL_FX 1
#define CELL_DDGI_FIVE 1	// GIFIX1101 SO: the lobe's five-point read (cell_ddgi.glsl, cell_gihalfat.glsl)
#define WW_LOOSE_TYPES 1
#include "../air/lookdev_fog.hlsli"
#include "../shadows/ww_sunshadow.hlsli"
#ifndef WW_CELL_FX
#define WW_CELL_FX 1	// as below, before this file
#endif
#include "../lights/cell_lights.hlsli"
#include "../lights/cell_cluster.hlsli"
#if defined(WW_DEBUG_VIEWS)
#include "../overlays/debug_view.hlsli"
#endif
#if defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH))
#include "cell_gihalfat.hlsli"
#endif
#if defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH)) && defined(WW_CELL_RTDDGI)
#ifndef CELL_DDGI_FIVE
#define CELL_DDGI_FIVE 1	// as below, before this file
#endif
#include "../gi/cell_ddgi.hlsli"
#endif
#if !(defined(WW_CELL_PRT)) && defined(WW_CELL_RTDDGI)
#ifndef CELL_DDGI_FIVE
#define CELL_DDGI_FIVE 1	// as below, before this file
#endif
#include "../gi/cell_ddgi.hlsli"
#include "cell_gihalfat.hlsli"
#endif
#undef WW_LOOSE_TYPES
cbuffer LooseUniforms : register(b31, space0)
{
#define WW_LOOSE_UNIFORMS 1
#include "../air/lookdev_fog.hlsli"
#include "../shadows/ww_sunshadow.hlsli"
#ifndef WW_CELL_FX
#define WW_CELL_FX 1	// as below, before this file
#endif
#include "../lights/cell_lights.hlsli"
#include "../lights/cell_cluster.hlsli"
#if defined(WW_DEBUG_VIEWS)
#include "../overlays/debug_view.hlsli"
#endif
#if defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH))
#include "cell_gihalfat.hlsli"
#endif
#if defined(WW_CELL_PRT) && !(defined(WW_CELL_GI_MARCH)) && defined(WW_CELL_RTDDGI)
#ifndef CELL_DDGI_FIVE
#define CELL_DDGI_FIVE 1	// as below, before this file
#endif
#include "../gi/cell_ddgi.hlsli"
#endif
#if !(defined(WW_CELL_PRT)) && defined(WW_CELL_RTDDGI)
#ifndef CELL_DDGI_FIVE
#define CELL_DDGI_FIVE 1	// as below, before this file
#endif
#include "../gi/cell_ddgi.hlsli"
#include "cell_gihalfat.hlsli"
#endif
#undef WW_LOOSE_UNIFORMS
	int2 giFull;		// the g-buffer's size (the viewport's)
	float2 giProj;		// the projection's P00, P11
	int giStep;		// 2 half resolution, 1 full
	bool giVolOn;		// the mask was drawn this frame
};
#include "../air/lookdev_fog.hlsli"
#include "../shadows/ww_sunshadow.hlsli"
#include "../lights/cell_lights.hlsli"

Texture2D<float4> giGbuf : register(t46, space0); SamplerState _giGbuf_sampler : register(s46, space0);
Texture2D<float4> giVolMask : register(t47, space0); SamplerState _giVolMask_sampler : register(s47, space0);	// full size: the pixel's interior volume's room + 1, 0 none

static float4 giOut;
#if defined( WW_CELL_RTDDGI ) && !defined( WW_CELL_PRT )
#define CELL_GIHALF_ON cellDdgiOn	// PERFFIX2: the ray-traced volume's screen read (cellGiOn is false without the PRT)
#else
#define CELL_GIHALF_ON cellGiOn
#endif
static float4 giKey;

void frag_main()
{
	int2 f = min( giStep * int2( gl_FragCoord.xy ), giFull - 1 );
	float4 g = giGbuf.Load( int3(f, 0) );
	if ( or( or( or( !cellOn, !CELL_GIHALF_ON ), g.w >= 1.0e5 ), dot( g.xyz, g.xyz ) < 0.5 ) ) {
		giOut = (float4)( 0.0 );
		giKey = float4( 0.0, 0.0, 0.0, -1.0 );
		return;
	}
#if defined( WW_CELL_PRT ) || defined( WW_CELL_RTDDGI )	// lane PRTOFF1: the PRT's screen pass; PERFFIX2: the volume's
	float z = -g.w / length( cellRow[0].xyz );	// back to view units
	float2 ndc = ( float2( f ) + 0.5 ) / float2( giFull ) * 2.0 - 1.0;
	float3 posView = float3( -z * ndc / giProj, z );
	float3 P = cellWorldPos( posView );
	float3 N = cellWorldDir( g.xyz );
#if !defined( WW_CELL_PRT )
	/* PERFFIX2: the volume's own read (cellGiSample's: o to the eye, d the normal), with the mirror direction's sky
	 * visibility riding it (FIX1101A's cellDdgiTwo: the same 8 probes and weights), packed for the specular occlusion */
	float3 V = normalize( float3( cellRow[0].w, cellRow[1].w, cellRow[2].w ) - P );
	float3 R = reflect( -V, N );
	cellDdgiTwo = true;
	cellDdgiD2 = normalize( N + R );	// half way: cellSpecSky's bend at k 0.5
	cellDdgiThree = true;
	cellDdgiD3 = R;
	cellDdgiFive = true;	// GIFIX1101 SO: a quarter and three quarters of the way
	cellDdgiD4 = normalize( N + cellDdgiD2 );
	cellDdgiD5 = normalize( cellDdgiD2 + R );
	CellDdgiOut d = cellDdgiRead( P, N, V, N );
	cellDdgiTwo = false;
	cellDdgiThree = false;
	cellDdgiFive = false;
	float3 e = d.fade * d.E;
	giOut = d.w > 0.0 && d.fade > 0.0 ? cellGiHalfPack5( e, d.fade, d.vis, d.vis4, d.vis2, d.vis5, d.vis3 ) : (float4)( 0.0 );
#elif defined( WW_CELL_GI_MARCH )
	giOut = cellGiSampleFull( P, N );
#else
	float m = giVolOn ? giVolMask.Load( int3(f, 0) ).r : 0.0;
	giOut = cellGiVolume( P, N, m > 0.5 ? int2( int( m + 0.5 ) - 1, -1 ) : cellGiVolAt( P, N ) );
#endif
	giKey = float4( P, cellGiKeyPack( N ) );
#endif
}

struct SPIRV_Cross_Input
{
    float4 gl_FragCoord : SV_Position;
};

struct SPIRV_Cross_Output
{
    float4 giOut : SV_Target0;
    float4 giKey : SV_Target1;
};

SPIRV_Cross_Output main(SPIRV_Cross_Input stage_input)
{
    gl_FragCoord = stage_input.gl_FragCoord;
    gl_FragCoord.w = 1.0 / gl_FragCoord.w;
    frag_main();
    SPIRV_Cross_Output stage_output;
    stage_output.giOut = giOut;
    stage_output.giKey = giKey;
    return stage_output;
}
