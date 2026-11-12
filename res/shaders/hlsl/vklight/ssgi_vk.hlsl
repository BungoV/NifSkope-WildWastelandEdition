// profile: cs_6_0
// lane VKLIGHT1 (2026-11-12): the screen-space GI cast (res/shaders/features/gi/cell_ssgi.frag stage 1) on Vulkan compute,
// one thread per work texel, 8x8 groups.
// T0 the g-buffer (RGBA32F: view normal, linear depth), T1 the scene colour with its levels 0..4 (RGBA16F), T2 the blue
// noise sheet; out0 the cast (RGBA16F, work size).
// Push: c[0] (fullW, fullH, workW, workH), c[1] (P00, P11, radius, thickness), c[2] (sourceBrightness, saturation);
// i = (slices, steps, frameIndex, flags: 1 blue noise, 2 full-mask early out). SSGI_RED (gate only): the thickness halved.
#define LPX_OUT0_FORMAT "rgba16f"
#include "LightBindingsVk.hlsli"

#define SSGI_GBUF( p ) LpxT0.Load( int3( clamp( ( p ), int2( 0, 0 ), P_.fullSize - 1 ), 0 ) )
#define SSGI_SCENE( uv, lod ) LpxT1.SampleLevel( LpxLinear, ( uv ), ( lod ) ).rgb
#define SSGI_NOISE( p ) LpxT2.Load( int3( ( p ), 0 ) ).rg
#include "SsgiCommon.hlsli"

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !LpxTexel( id, size ) )
		return;
	F4FX_SsgiParams p;
	p.fullSize = int2( LpxJobPc.c[0].xy );
	p.workSize = int2( LpxJobPc.c[0].zw );
	p.proj = LpxJobPc.c[1].xy;
	p.radius = LpxJobPc.c[1].z;
	p.thickness = LpxJobPc.c[1].w;
#ifdef SSGI_RED
	p.thickness *= 0.5f;
#endif
	p.sourceBrightness = LpxJobPc.c[2].x;
	p.saturation = LpxJobPc.c[2].y;
	p.slices = LpxJobPc.i.x;
	p.steps = LpxJobPc.i.y;
	p.frameIndex = LpxJobPc.i.z;
	p.blueNoise = ( LpxJobPc.i.w & 1 ) != 0;
	p.fullBreak = ( LpxJobPc.i.w & 2 ) != 0;
	LpxOut0[id.xy] = F4FX_SsgiCast( int2( id.xy ), p );
}
