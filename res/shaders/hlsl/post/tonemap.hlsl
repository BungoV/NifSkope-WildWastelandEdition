// Lane VKFRAME1 P6 (2026-11-12): the cell view's tone map on Vulkan, cell_hdr.frag's twin per sample. One thread a pixel
// walks its samples: the stencil test (hdrStencilTest / Ref) skips a sample the GL program discards (its output texel keeps
// what was there), else the mode's value goes to ToneOut at column x * samples + s, y (RGBA32F: the fragment's float output
// before the view's blend). Modes as cell_hdr.frag: 0 the cell's imagespace (dithered), 1 the value as written, 2 an effect
// over the sky by its coverage, 3 the bloom's glow at the backdrop's place on the curve. The Fusion Shaders bloom comes from
// its per-pixel combine (bloom_comb.hlsl) only: with the combine off the GL tone map runs.
// TONEMAP_RED skips the grading LUT (the gate's red must FAIL).
#include "ToneBindingsVk.hlsli"

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint w, h, S;
	ToneFrame.GetDimensions( w, h, S );
	if ( any( id.xy >= uint2( w, h ) ) )
		return;
	const ToneJob j = ToneJobPc;
	F4FX_ImageSpace p = ToneImageSpace( j );
#ifdef TONEMAP_RED
	p.lutOn = false;
#endif
	const int flags = j.ints.x;
	const int mode = ( flags >> 16 ) & 3, test = ( flags >> 20 ) & 3, ref = ( flags >> 24 ) & 3;
	const bool cellOn = ( flags & TONE_F_CELLON ) != 0;
	const int2 px = int2( id.xy );
	// the Fusion Shaders bloom at this pixel times the bloom weight: the same at every sample
	float3 foBloom = float3( 0.0f, 0.0f, 0.0f );
	if ( p.foOn )
		foBloom = ToneComb.Load( int3( px, 0 ) ).rgb * j.fo.x;
	for ( uint s = 0; s < S; s++ ) {
		if ( test != 0 ) {
			uint st = ToneStencil.Load( px, s ) & 3u;
			if ( ( st == uint( ref ) ) != ( test == 1 ) )
				continue;
		}
		const uint2 o = uint2( id.x * S + s, id.y );
		const float4 x = ToneFrame.Load( px, s );
		const float2 fc = ToneFragCoord( id.xy );
		float gain = 1.0f;
		if ( ( flags & TONE_F_FOGBLOOM ) != 0 )
			gain = 1.0f + 4.0f * ToneFogBloom.Load( px, s ).r;
		const bool measureOn = ( flags & TONE_F_MEASURE ) != 0;
		float3 mb = float3( 0.0f, 0.0f, 0.0f );
		if ( measureOn )
			mb = ToneMeasureBloom.SampleLevel( ToneLinearClamp, ( fc - j.bloomRect.xy ) * j.bloomRect.zw, 0.0f ).rgb;
		const float3 zero = float3( 0.0f, 0.0f, 0.0f );
		if ( mode == 3 ) {
			float3 lit = F4FX_CellImageSpace( zero, measureOn, mb, gain, foBloom, p, ToneLut, ToneLinearClamp );
			float3 glow = max( lit - F4FX_CellImageSpace( zero, measureOn, mb, gain, zero, p, ToneLut, ToneLinearClamp ), zero );
			if ( ( flags & TONE_F_BACKDROP ) != 0 )
				glow *= F4FX_CellGlowShoulder( ToneBackdrop.Load( int3( px, 0 ) ).rgb, 2.0f * gain * foBloom, p );
			ToneOut[o] = float4( glow, 0.0f );
			continue;
		}
		const float3 foHere = mode == 0 ? foBloom : zero;
		if ( mode == 2 ) {
			float A = clamp( x.a, 0.0f, 1.0f );
			float3 v = max( x.rgb, zero );
			if ( !cellOn )
				ToneOut[o] = float4( v, A );
			else if ( A > 1.0f / 1024.0f )
				ToneOut[o] = float4( F4FX_Dither8( F4FX_CellImageSpace( sqrt( v / A ), measureOn, mb, gain, foHere, p, ToneLut, ToneLinearClamp ) * A,
					uint2( fc ), j.ints.y, j.ints.z ), A );
			else
				ToneOut[o] = float4( max( F4FX_CellImageSpace( sqrt( v ), measureOn, mb, gain, foHere, p, ToneLut, ToneLinearClamp )
					- F4FX_CellImageSpace( zero, measureOn, mb, gain, foHere, p, ToneLut, ToneLinearClamp ), zero ), 0.0f );
			continue;
		}
		if ( mode != 0 || !cellOn ) {
			ToneOut[o] = x;
			continue;
		}
		ToneOut[o] = float4( F4FX_Dither8( F4FX_CellImageSpace( sqrt( max( x.rgb, zero ) ), measureOn, mb, gain, foHere, p, ToneLut,
			ToneLinearClamp ), uint2( fc ), j.ints.y, j.ints.z ), 1.0f );
	}
}
