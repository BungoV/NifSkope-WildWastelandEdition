// profile: cs_6_0
// lane VKWATER4 (2026-11-15, plan row G5): the ultra ocean's spectrum, inverse FFT, layers and mips on Vulkan compute.
// GL twins: res/shaders/features/water/ocean_spectrum / ocean_fft / ocean_final / ocean_mip (.frag, src/features/water/
// cellocean.cpp draws 1 + 14 + 1 + 7). The arrays' six tiles sit side by side in 768 x 128 strips, one image a level
// (a tile's 2 x 2 mean never crosses a tile edge); cellocean.cpp copies each strip into its array layers.
// Float order: NV GL's own (its fp assembly, notes/vkwater4/asmO), every step precise: cmul as fma( a.x, b.x, -a.y b.y ),
// fma( a.x, b.y, a.y b.x ); |k| as 1 / rsqrt of an x-first fma dot.
//   -D OCEAN_STAGE=1  spectrum (out0, out1 RGBA32F): T0 h0 (RGBA32F), T1 q (R32F); c[0].x phase, c[1] dk 0-3, c[2].xy dk 4-5, i.w 0
//   -D OCEAN_STAGE=2  one Stockham stage (RGBA32F x 2): T0, T1 the fields; i.x Ns, i.y axis (0 rows, 1 columns)
//   -D OCEAN_STAGE=3  layers B (out0 RGBA16F) + C (out1 RGBA32F): T0 x0, T1 x1; i.x the tile offset (128; GL's 128 * layer)
//   -D OCEAN_STAGE=4  layers D (out0 RGBA16F): T0 x1; i.x the tile offset
//   -D OCEAN_STAGE=5  one mip of B (out0 RGBA16F) + C (out1 RGBA32F): T0, T1 the level above
//   -D OCEAN_STAGE=6  one mip of D (out0 RGBA16F): T0 the level above
#ifndef OCEAN_STAGE
#define OCEAN_STAGE 1
#endif
#if OCEAN_STAGE == 1 || OCEAN_STAGE == 2
#define LPX_OUT0_FORMAT "rgba32f"
#define LPX_OUT1_FORMAT "rgba32f"
#define LPX_OUT1_TYPE float4
#elif OCEAN_STAGE == 3 || OCEAN_STAGE == 5
#define LPX_OUT0_FORMAT "rgba16f"
#define LPX_OUT1_FORMAT "rgba32f"
#define LPX_OUT1_TYPE float4
#else
#define LPX_OUT0_FORMAT "rgba16f"
#endif
#define LPX_OUT0_TYPE float4
#include "LightBindingsVk.hlsli"

// a b, GL's fused form
float2 oceanCmul( float2 a, float2 b )
{
	precise float y = a.y * b.y;
	precise float x = mad( a.x, b.x, -y );
	precise float z = a.y * b.x;
	precise float w = mad( a.x, b.y, z );
	return float2( x, w );
}

// v unchanged (LpxJob.i.w is 0 in the spectrum), but opaque to the compiler: no mul reassociates across it
float oceanKeep( float v )
{
	return asfloat( asuint( v ) ^ uint( LpxJobPc.i.w ) );
}

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 size;
	if ( !LpxTexel( id, size ) )
		return;
	const int2 p = int2( id.xy );
#if OCEAN_STAGE == 1
	const int tile = p.x / 128;
	const int mx = p.x - tile * 128;
	const int2 n = int2( mx < 64 ? mx : mx - 128, p.y < 64 ? p.y : p.y - 128 );
	const float dks[6] = { LpxJobPc.c[1].x, LpxJobPc.c[1].y, LpxJobPc.c[1].z, LpxJobPc.c[1].w, LpxJobPc.c[2].x, LpxJobPc.c[2].y };
	precise float2 k = float2( n ) * dks[tile];
	precise float kk = mad( k.y, k.y, k.x * k.x );
	precise float kl = 1.0 / max( rsqrt( kk ), 1.175494351e-38 );	// the max (a no-op) blocks a fold into sqrt
	precise float ki = kl > 0.0 ? 1.0 / kl : 0.0;
	const float4 h0 = LPX_FETCH( LpxT0, p );
	const float q = LPX_FETCH( LpxT1, p ).r;
	precise float qp = q * LpxJobPc.c[0].x;
	precise float ph = frac( qp ) * 6.2831853;
	const float c = cos( ph ), s = sin( ph );
	precise float2 ha = oceanCmul( h0.xy, float2( c, s ) );
	precise float2 hb = oceanCmul( h0.zw, float2( c, -s ) );
	precise float2 h = ha + hb;
	precise float2 ih = float2( -h.y, h.x );
	precise float2 sy = k.y * ih;
	precise float4 o0, o1;
	o0.x = mad( k.x, ih.x, -sy.y );
	o0.y = mad( k.x, ih.y, sy.x );
	// NV's Vulkan compiler reassociates mul chains through precise ((-kx kx) ki) hx -> (-kx hx) (kx ki)); oceanKeep holds GL's order
	precise float a = oceanKeep( oceanKeep( -k.x * k.x ) * ki );
	precise float byy = oceanKeep( ki * oceanKeep( -k.y * k.y ) );
	precise float2 dyy = h * byy;
	o0.z = mad( a, h.x, -dyy.y );
	o0.w = mad( a, h.y, dyy.x );
	precise float cxy = oceanKeep( ki * oceanKeep( -k.x * k.y ) );
	precise float dyk = k.y * ki;
	precise float2 dy = ih * dyk;
	precise float dxk = k.x * ki;
	o1.z = mad( ih.x, dxk, -dy.y );
	o1.w = mad( ih.y, dxk, dy.x );
	o1.x = mad( h.x, cxy, -h.y );
	o1.y = mad( h.y, cxy, h.x );
	LpxOut0[id.xy] = o0;
	LpxOut1[id.xy] = o1;
#elif OCEAN_STAGE == 2
	const int ns = LpxJobPc.i.x, axis = LpxJobPc.i.y;
	const int i = axis == 0 ? ( p.x & 127 ) : p.y;
	const int k = i % ns;
	const int j = ( i / ( 2 * ns ) ) * ns + k;
	const float sg = ( ( i / ns ) & 1 ) != 0 ? -1.0 : 1.0;
	const int2 pa = axis == 0 ? int2( p.x - i + j, p.y ) : int2( p.x, j );
	const int2 pb = axis == 0 ? int2( pa.x + 64, p.y ) : int2( p.x, j + 64 );
	precise float r = float( k ) / float( ns );
	precise float th = r * 3.14159265;
	precise float2 w = sg * float2( cos( th ), sin( th ) );
	const float4 a0 = LPX_FETCH( LpxT0, pa ), a1 = LPX_FETCH( LpxT1, pa );
	const float4 b0 = LPX_FETCH( LpxT0, pb ), b1 = LPX_FETCH( LpxT1, pb );
	precise float4 o0 = a0 + float4( oceanCmul( b0.xy, w ), oceanCmul( b0.zw, w ) );
	precise float4 o1 = a1 + float4( oceanCmul( b1.xy, w ), oceanCmul( b1.zw, w ) );
	LpxOut0[id.xy] = o0;
	LpxOut1[id.xy] = o1;
#elif OCEAN_STAGE == 3 || OCEAN_STAGE == 4
	const int2 pr = int2( ( p.x & 127 ) + LpxJobPc.i.x * ( p.x >> 7 ), p.y );
#if OCEAN_STAGE == 3
	const float4 x0 = LPX_FETCH( LpxT0, pr ), x1 = LPX_FETCH( LpxT1, pr );
	precise float4 oc = float4( x0.x * x0.x, x0.y * x0.y, x0.x * x0.y, x1.x );
	LpxOut0[id.xy] = x0;
	LpxOut1[id.xy] = oc;
#else
	const float4 x1 = LPX_FETCH( LpxT0, pr );
	LpxOut0[id.xy] = float4( x1.yzw, 0.0 );
#endif
#else
	const int2 q = 2 * p;
	precise float4 m0 = ( ( LPX_FETCH( LpxT0, q ) + LPX_FETCH( LpxT0, q + int2( 1, 0 ) ) ) + LPX_FETCH( LpxT0, q + int2( 0, 1 ) ) )
		+ LPX_FETCH( LpxT0, q + int2( 1, 1 ) );
	precise float4 r0 = m0 * 0.25;
	LpxOut0[id.xy] = r0;
#if OCEAN_STAGE == 5
	precise float4 m1 = ( ( LPX_FETCH( LpxT1, q ) + LPX_FETCH( LpxT1, q + int2( 1, 0 ) ) ) + LPX_FETCH( LpxT1, q + int2( 0, 1 ) ) )
		+ LPX_FETCH( LpxT1, q + int2( 1, 1 ) );
	precise float4 r1 = m1 * 0.25;
	LpxOut1[id.xy] = r1;
#endif
#endif
}
