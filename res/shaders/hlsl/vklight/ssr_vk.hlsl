// profile: cs_6_0
// lane VKLIGHT1 (2026-11-12): the game's screen-space reflections (res/shaders/features/reflections/cell_ssr.frag stages 1-4)
// on Vulkan compute, one thread per half-size texel, 8x8 groups. SSR_STAGE = 1 ray, 2 march, 3 blur across, 4 blur down.
// T0 the g-buffer (RGBA32F: view normal, linear depth), T1 the depth mips 1..4 as levels 0..3 of one R32F image, T2 the scene
// (RGBA32F, alpha the reflection flag, bilinear), T3 the previous stage's target, T4 the water's ray; out0 RGBA32F half size.
// Push: c[0] (fullW, fullH, halfW, halfH), c[1] (P00, P11, near, far), c[2] the game's four, c[3..5].xyz viewToWorld's GL
// columns; i = (ssrRed, haveWaterRay, opaqueRays). SSR_RED (gate only): the stage drops one term (see below).
// SSR_LIVE (lane VKLIGHT2, the app's pass, vkpresent.cpp ssrRun): T1 = mips 1..4 side by side in one R32F image, their x
// offsets in c[6]; LpxBuf rows 0..2 = ssrPrevRot's GL columns (w = ssrPrevT), row 3 = (ssrPrevProj, prevOn); binding 10 the
// previous frame (bilinear). SSR_STAGE 5 = the bounce's donor into the scene's rgb (out0 = the scene, alpha kept).
#ifndef SSR_STAGE
#define SSR_STAGE 1	// hlsl_check compiles without -D
#endif
#ifdef SSR_LIVE
#define LPX_BUF_TYPE float4
#endif
#include "LightBindingsVk.hlsli"

#define SSR_GBUF( t ) LpxT0.Load( int3( ( t ), 0 ) )
#ifdef SSR_LIVE
LPX_BIND( 10 ) Texture2D<float4> SsrPrevT : register( t6 );
#define SSR_ZMIP( m, t ) LpxT1.Load( int3( ( t ) + int2( int( LpxJobPc.c[6][( m ) - 1] ), 0 ), 0 ) ).r
#define SSR_PREV_AT( uv ) SsrPrevT.SampleLevel( LpxLinear, ( uv ), 0.0f )
#else
#define SSR_ZMIP( m, t ) LpxT1.Load( int3( ( t ), ( m ) - 1 ) ).r
#endif
#define SSR_SCENE( t ) LpxT2.Load( int3( ( t ), 0 ) )
#define SSR_SCENE_AT( uv ) LpxT2.SampleLevel( LpxLinear, ( uv ), 0.0f )
#define SSR_SRC( q ) LpxT3.Load( int3( ( q ), 0 ) )
#define SSR_SRC_AT( uv ) LpxT3.SampleLevel( LpxLinear, ( uv ), 0.0f )
#define SSR_WATER( t ) LpxT4.Load( int3( ( t ), 0 ) )
#include "SsrCommon.hlsli"

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !LpxTexel( id, size ) )
		return;
	F4FX_SsrParams p;
	p.fullSize = int2( LpxJobPc.c[0].xy );
	p.halfSize = int2( LpxJobPc.c[0].zw );
	p.proj = LpxJobPc.c[1].xy;
	p.clip = LpxJobPc.c[1].zw;
	p.k = LpxJobPc.c[2];
	p.viewToWorld[0] = LpxJobPc.c[3].xyz;
	p.viewToWorld[1] = LpxJobPc.c[4].xyz;
	p.viewToWorld[2] = LpxJobPc.c[5].xyz;
	p.red = LpxJobPc.i.x;
	p.haveWaterRay = LpxJobPc.i.y != 0;
	p.opaqueRays = LpxJobPc.i.z != 0;
#ifdef SSR_LIVE
	p.prevCol[0] = LpxBuf[0].xyz;
	p.prevCol[1] = LpxBuf[1].xyz;
	p.prevCol[2] = LpxBuf[2].xyz;
	p.prevT = float3( LpxBuf[0].w, LpxBuf[1].w, LpxBuf[2].w );
	p.prevProj = LpxBuf[3].xy;
	p.prevOn = LpxBuf[3].z != 0.0f;
#endif
	int2 q = int2( id.xy );
#if SSR_STAGE == 5
	float4 v = float4( F4FX_SsrPrevDonor( q, p ), LpxOut0[id.xy].a );
#elif SSR_STAGE == 1
#ifdef SSR_RED
	p.k.y = -1.0f;	// red: no angle gate
#endif
	float4 v = F4FX_SsrRay( q, p );
#elif SSR_STAGE == 2
#ifdef SSR_RED
	p.red |= 2;		// red: no 50-unit refusal
#endif
	float4 v = F4FX_SsrMarch( q, p );
#else
	float4 v = F4FX_SsrBlur( q, SSR_STAGE, p );
#ifdef SSR_RED
	v.a = LpxT3.Load( int3( q, 0 ) ).a;	// red: the confidence unblurred
#endif
#endif
	LpxOut0[id.xy] = v;
}
