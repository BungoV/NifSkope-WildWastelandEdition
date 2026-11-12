// Lane VKFRAME1 round 8 (2026-11-12): robust contrast-adaptive sharpening for the Modern temporal AA, taa2_rcas.frag's twin.
// Ported from AMD FidelityFX FSR 1 (ffx_fsr1.h, FsrRcasF), github.com/GPUOpen-Effects/FidelityFX-FSR, MIT licence,
// Copyright (c) 2021 Advanced Micro Devices, Inc.; the licence text is in taa2_rcas.frag.
// T0 the picture; c[0] = (rcasSharp, rcasExpose, -, -), i.x = rcasLinear (1: sharpen in c / (1 + max c), write light).
// PFX_OUT0_FORMAT names the frame's format. PFX_RED drops the noise test.
#include "PostFxBindingsVk.hlsli"

static PfxJob J;

float3 RcasTap( int2 q )
{
	float3 c = PfxT0.Load( int3( q, 0 ) ).rgb;
	if ( J.i.x == 0 )
		return c;
	c = max( c, 0.0f ) * J.c[0].y;
	return c / ( 1.0f + max( c.r, max( c.g, c.b ) ) );
}

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 osize;
	if ( !PfxTexel( id, osize ) )
		return;
	J = PfxJobPc;
	uint tw, th;
	PfxT0.GetDimensions( tw, th );
	const int2 hi = int2( tw, th ) - 1;
	const int2 p = int2( id.xy );
	const float3 b = RcasTap( clamp( p + int2( 0, 1 ), 0, hi ) );
	const float3 d = RcasTap( clamp( p + int2( -1, 0 ), 0, hi ) );
	const float3 e = RcasTap( p );
	const float3 f = RcasTap( clamp( p + int2( 1, 0 ), 0, hi ) );
	const float3 h = RcasTap( clamp( p + int2( 0, -1 ), 0, hi ) );
	const float bL = b.b * 0.5f + ( b.r * 0.5f + b.g ), dL = d.b * 0.5f + ( d.r * 0.5f + d.g ), eL = e.b * 0.5f + ( e.r * 0.5f + e.g );
	const float fL = f.b * 0.5f + ( f.r * 0.5f + f.g ), hL = h.b * 0.5f + ( h.r * 0.5f + h.g );
	float nz = 0.25f * ( bL + dL + fL + hL ) - eL;
	const float rangeL = max( max( max( bL, dL ), max( eL, fL ) ), hL ) - min( min( min( bL, dL ), min( eL, fL ) ), hL );
	nz = clamp( abs( nz ) / max( rangeL, 1e-6f ), 0.0f, 1.0f );
	nz = -0.5f * nz + 1.0f;
#ifdef PFX_RED
	nz = 1.0f;
#endif
	const float3 mn4 = min( min( b, d ), min( f, h ) );
	const float3 mx4 = max( max( b, d ), max( f, h ) );
	const float3 hitMin = min( mn4, e ) / max( 4.0f * mx4, 1e-6f );
	const float3 hitMax = ( 1.0f - max( mx4, e ) ) / min( 4.0f * mn4 + -4.0f, -1e-6f );
	const float3 lobeRGB = max( -hitMin, hitMax );
	float lobe = max( -( 0.25f - 1.0f / 16.0f ), min( max( lobeRGB.r, max( lobeRGB.g, lobeRGB.b ) ), 0.0f ) ) * J.c[0].x;
	lobe *= nz;
	const float rcpL = 1.0f / ( 4.0f * lobe + 1.0f );
	float3 o = clamp( ( lobe * ( b + d + f + h ) + e ) * rcpL, 0.0f, 1.0f );
	if ( J.i.x != 0 ) {
		o = min( o, 0.999f );
		o = o / ( ( 1.0f - max( o.r, max( o.g, o.b ) ) ) * max( J.c[0].y, 1e-6f ) );
	}
	PFX_STORE0( id.xy, float4( o, 1.0f ) );
}
