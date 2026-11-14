// profile: ps_6_0
// Lane VKFRAME1 (2026-11-12): the cell view's tone map as a fragment pass, the way GL draws cell_hdr.frag: a full-screen
// triangle (vs, -E vs -T vs_6_0), per-sample shading (SV_SampleIndex), the value written to the multisample colour
// attachment through the ROP (RGBA8 for the view, RGBA32F for the gate). The math is ToneSample (ToneBindingsVk.hlsli),
// the same as the compute kernel tonemap.hlsl; a sample the stencil test skips is discarded (the attachment keeps it).
// TONEMAP_RED skips the grading LUT; TONEMAP_NO_WORK only loads each sample and writes it (the timing floor).
#include "ToneBindingsVk.hlsli"

float4 vs( uint id : SV_VertexID ) : SV_Position
{
	const float2 q = float2( float( ( id << 1 ) & 2 ), float( id & 2 ) );
	return float4( q * 2.0f - 1.0f, 0.0f, 1.0f );
}

float4 main( float4 pos : SV_Position, uint s : SV_SampleIndex ) : SV_Target
{
	const int2 px = int2( pos.xy );
#ifdef TONEMAP_NO_WORK
	return ToneFrame.Load( px, s );
#else
	const ToneJob j = ToneJobPc;
	F4FX_ImageSpace p = ToneImageSpace( j );
#ifdef TONEMAP_RED
	p.lutOn = false;
#endif
	float3 foBloom = float3( 0.0f, 0.0f, 0.0f );
	if ( p.foOn && ( j.ints.x & TONE_F_FOBLOOMLOD ) != 0 )
		foBloom = ToneFoBloomLod( j, px, s );
	else if ( p.foOn )
		foBloom = ToneComb.Load( int3( px, 0 ) ).rgb * j.fo.x;
	float4 v;
	if ( !ToneSample( j, p, px, s, foBloom, v ) )
		discard;
	return v;
#endif
}
