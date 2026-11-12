// Lane VKFRAME1 round 8 (2026-11-12): the temporal AA's camera motion vectors, game_taa_mv.frag's twin: the pixel's
// unjittered NDC taken to the previous frame by reproj, previous minus current in texture units. T0 the depth (r32f);
// c[0..3] = reproj's GL columns, c[4] = (jitterNdc, invSize). Target rg32f. PFX_RED drops the jitter.
// GL's own rounding of p.xy / p.w (a value near 1) shows in about a third of the vectors at 1 - 2 last bits of the NDC; no
// order of the matrix product or the NDC sum matched it (8 tried), so the gate bounds the vectors at 2^-22 (rule ndc).
#define PFX_OUT0_FORMAT "rg32f"
#define PFX_OUT0_TYPE float2
#include "PostFxBindingsVk.hlsli"

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !PfxTexel( id, size ) )
		return;
	const PfxJob j = PfxJobPc;
	const float2 fc = float2( id.xy ) + 0.5f;
	const float2 uv = fc * j.c[4].zw;
	const float d = PfxT0.Load( int3( id.xy, 0 ) ).r;
#ifdef PFX_RED
	const float3 ndc = float3( uv * 2.0f - 1.0f, d * 2.0f - 1.0f );
#else
	const float3 ndc = float3( uv * 2.0f - 1.0f - j.c[4].xy, d * 2.0f - 1.0f );
#endif
	const float4 p = j.c[0] * ndc.x + j.c[1] * ndc.y + j.c[2] * ndc.z + j.c[3];
	PFX_STORE0( id.xy, ( p.xy / p.w - ndc.xy ) * 0.5f );
}
