// profile: cs_6_0
// The cell view's ambient obscurance on Vulkan compute (lane VKLIGHT1, 2026-11-12): GL twin res/shaders/features/ao/cell_ao.frag,
// one stage per build (src/features/ao/ao.cpp draws 5, 1 x4, 2, 3, 4). Math: AoCommon.hlsli; set: LightBindingsVk.hlsli.
//   -D AO_STAGE=5  gbuf (RGBA32F, full size): T0 zres (window depth), T1 zlin; c[0..3] the inverse projection's GL columns,
//                  c[5].x aoUnit; i.x 1 = read zlin
//   -D AO_STAGE=1  depth mip (R32F): T0 gbuf (level 1) or the level above; i.x the level
//   -D AO_STAGE=2  raw obscurance + depth key (RG32F, half size): T0 gbuf, T1..T4 mips 1..4; c[0] (P00, P11, radius, bias),
//                  c[1].x intensity; i.x angles, i.y 1 = no reset
//   -D AO_STAGE=3  blur (RG32F): T0 the raw / across target; i.x 0 across, 1 down; i.y 1 = no blur. AO_STAGE=4 the same
//                  into the R32F final target
// Every stage: c[4] = (full w, h, half w, h); stages 1 and 2: c[2] = (mip 1 w, h, mip 2 w, h), c[3] = (mip 3, mip 4).
// Reds: AO_GBUF_RED, AO_MIP_RED, AO_RAW_RED, AO_BLUR_RED (AoCommon.hlsli).
#if AO_STAGE == 5
#define LPX_OUT0_FORMAT "rgba32f"
#define LPX_OUT0_TYPE float4
#elif AO_STAGE == 1 || AO_STAGE == 4
#define LPX_OUT0_FORMAT "r32f"
#define LPX_OUT0_TYPE float
#else
#define LPX_OUT0_FORMAT "rg32f"
#define LPX_OUT0_TYPE float2
#endif
#include "LightBindingsVk.hlsli"
#include "AoCommon.hlsli"

F4FX_AoSizes AoSizesPc()
{
	F4FX_AoSizes s;
	s.full = int2( LpxJobPc.c[4].xy );
	s.half_ = int2( LpxJobPc.c[4].zw );
	s.mip[0] = s.full;
	s.mip[1] = int2( LpxJobPc.c[2].xy );
	s.mip[2] = int2( LpxJobPc.c[2].zw );
	s.mip[3] = int2( LpxJobPc.c[3].xy );
	s.mip[4] = int2( LpxJobPc.c[3].zw );
	return s;
}

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !LpxTexel( id, size ) )
		return;
	const int2 P = int2( id.xy );
#if AO_STAGE == 5
	float4 inv[4] = { LpxJobPc.c[0], LpxJobPc.c[1], LpxJobPc.c[2], LpxJobPc.c[3] };
	LpxOut0[id.xy] = F4FX_AoGbuf( LpxT0, LpxT1, int2( LpxJobPc.c[4].xy ), inv, LpxJobPc.c[5].x, LpxJobPc.i.x != 0, P );
#elif AO_STAGE == 1
	LpxOut0[id.xy] = F4FX_AoMip( LpxT0, AoSizesPc(), LpxJobPc.i.x, P );
#elif AO_STAGE == 2
	LpxOut0[id.xy] = F4FX_AoRaw( LpxT0, LpxT1, LpxT2, LpxT3, LpxT4, AoSizesPc(), LpxJobPc.c[0].xy, float3( LpxJobPc.c[0].zw, LpxJobPc.c[1].x ),
		LpxJobPc.i.x, LpxJobPc.i.y != 0, P );
#else
	const float2 v = F4FX_AoBlur( LpxT0, int2( size ), LpxJobPc.i.x != 0 ? int2( 0, 1 ) : int2( 1, 0 ), LpxJobPc.i.y != 0, P );
#if AO_STAGE == 4
	LpxOut0[id.xy] = v.x;
#else
	LpxOut0[id.xy] = v;
#endif
#endif
}
