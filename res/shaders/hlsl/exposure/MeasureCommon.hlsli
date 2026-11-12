// Exposure measure: the shared math (lane VKFRAME1 P4, 2026-11-12). No bindings and no constant block: each app's shim
// declares its resources and calls this (NifSkope: post/BloomBindingsVk.hlsli + exposure/measure_q.hlsl on Vulkan;
// Fusion Shaders: its own adaptation pass on its D3D slots). GL twin: res/shaders/features/exposure/cell_is_measure_q.frag;
// every expression keeps its order (tests/vkframe/measure.sh compares the two on the GPU).
#ifndef F4FX_MEASURE_COMMON_HLSLI
#define F4FX_MEASURE_COMMON_HLSLI

// a non-finite channel reads 0 (tested by its bits: a compiler may fold isnan / isinf away)
float F4FX_MeasureFinite( float v )
{
	return ( asuint( v ) & 0x7F800000u ) == 0x7F800000u ? 0.0 : v;
}

/* One frame pixel p: counted where sample 0's stencil reads 1 in its low two bits (a cell program or a cell effect wrote it
 * last: linear light). rgb = the mean of its samples (non-finite read as 0), a = 1; a pixel outside the frame or not counted
 * returns a = -1. */
float4 F4FX_MeasurePixel( Texture2DMS<float4> frame, Texture2DMS<uint> stencil, int2 p, int2 size, int samples )
{
	if ( p.x >= size.x || p.y >= size.y )
		return float4( 0.0, 0.0, 0.0, -1.0 );
	if ( ( stencil.Load( p, 0 ) & 3u ) != 1u )
		return float4( 0.0, 0.0, 0.0, -1.0 );
	float3 px = float3( 0.0, 0.0, 0.0 );
	for ( int i = 0; i < samples; i++ ) {
		const float3 v = frame.Load( p, i ).rgb;
		px += float3( F4FX_MeasureFinite( v.r ), F4FX_MeasureFinite( v.g ), F4FX_MeasureFinite( v.b ) );
	}
	return float4( px / float( max( samples, 1 ) ), 1.0 );
}

/* Fold one pixel into a quarter texel's running sum, in the GL twin's order (rows, then columns of the 4 x 4 block).
 * Finish with float4( sum / 16.0, cover / 16.0 ) (the full-size route's mip level 2). */
void F4FX_MeasureAdd( inout float3 sum, inout float cover, float4 pixel )
{
	if ( pixel.a >= 0.0 ) {
		sum += pixel.rgb;
		cover += 1.0;
	}
}

// One quarter texel q from one thread: its 4 x 4 frame pixels.
float4 F4FX_MeasureQuarter( Texture2DMS<float4> frame, Texture2DMS<uint> stencil, int2 q, int2 size, int samples )
{
	float3 sum = float3( 0.0, 0.0, 0.0 );
	float cover = 0.0;
	for ( int y = 0; y < 4; y++ )
		for ( int x = 0; x < 4; x++ )
			F4FX_MeasureAdd( sum, cover, F4FX_MeasurePixel( frame, stencil, q * 4 + int2( x, y ), size, samples ) );
	return float4( sum / 16.0, cover / 16.0 );
}

#endif
