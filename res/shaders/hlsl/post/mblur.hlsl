// Lane VKFRAME1 P7 (2026-11-12): the game's motion blur on Vulkan, game_mblur.frag's twin (DofCommon.hlsli). T0 the frame
// (linear), T1 the vectors (linear); c[4] = mc0 (scale, max blur, threshold, -), c[5] = mc1 (corner).
// MBLUR_FUSE (game_dofmblur.frag's twin): every tap reads the depth of field's composite as a linear filter would read its
// target; T2 the quarter blur (linear), T3 the depth, c[0..3] = dc0..dc3, i = (far only, the app's red).
// MBLUR_FUSE + DOF_LENS (game_doflensmblur.frag's twin): T2 the far layer, T3 the depth, T4 the near layer,
// c[0] = lz, c[1] = lensP, c[2] = lensQ. PFX_RED drops the last tap.
#include "PostFxBindingsVk.hlsli"

#ifdef MBLUR_FUSE
static PfxJob J;
static float LensS1;

float4 DofTexel( int2 p, float2 size )	// the composite at one full-size texel
{
#ifdef DOF_LENS
	return F4FX_LensTexel( p, PfxT0, PfxT2, PfxT3, PfxT4, PfxLinear, LensS1, J.c[0], J.c[1], J.c[2] );
#else
	const float2 v1 = ( float2( p ) + 0.5f ) / size;
	const float k = F4FX_DofFactor( PfxT3.Load( int3( p, 0 ) ).r, J.c[0], J.c[1], J.c[2], J.i.x != 0, J.i.y != 0 );
	const float4 b = PfxT2.SampleLevel( PfxLinear, v1 * J.c[3].zw, 0.0f );
	const float4 a = PfxT0.Load( int3( p, 0 ) );
	return F4FX_DofBlend( k, a, b );
#endif
}

float3 ColorAt( float2 uv, float2 size )	// the composite read as a linear filter reads its target (clamped)
{
	const float2 x = uv * size - 0.5f;
	const float2 i0 = floor( x );
	const float2 f = x - i0;
	int2 lo = int2( i0 );
	int2 hi = min( lo + 1, int2( size ) - 1 );
	lo = max( lo, int2( 0, 0 ) );
	hi = max( hi, int2( 0, 0 ) );
	float3 r = DofTexel( lo, size ).rgb * ( ( 1.0f - f.x ) * ( 1.0f - f.y ) );
	if ( f.x > 0.0f )
		r += DofTexel( int2( hi.x, lo.y ), size ).rgb * ( f.x * ( 1.0f - f.y ) );
	if ( f.y > 0.0f ) {
		r += DofTexel( int2( lo.x, hi.y ), size ).rgb * ( ( 1.0f - f.x ) * f.y );
		if ( f.x > 0.0f )
			r += DofTexel( hi, size ).rgb * ( f.x * f.y );
	}
	return r;
}
#else
float3 ColorAt( float2 uv, float2 size )
{
	return PfxT0.SampleLevel( PfxLinear, uv, 0.0f ).rgb;
}
#endif

[numthreads( 8, 8, 1 )]
void main( uint3 id : SV_DispatchThreadID )
{
	uint2 osize;
	if ( !PfxTexel( id, osize ) )
		return;
	const PfxJob j = PfxJobPc;
#ifdef MBLUR_FUSE
	J = j;
#ifdef DOF_LENS
	LensS1 = F4FX_LensFocus( PfxT3, j.c[0], j.c[1] );
#endif
#endif
	const float4 mc0 = j.c[4], mc1 = j.c[5];
	uint fw, fh;
	PfxT0.GetDimensions( fw, fh );
	const float2 size = float2( fw, fh );
	const float2 g = ( float2( id.xy ) + 0.5f ) / size;
	const float2 v1 = F4FX_MbV( g );
	const float2 m = F4FX_MbVector( PfxT1, PfxLinear, v1 );
	const float l2 = dot( m, m );
	const float L = sqrt( l2 );
	const float2 dc = v1 - mc1.zw;
	float rr = sqrt( dot( dc, dc ) );
	rr = F4FX_PostSat( rr * 0.1f + -0.018f );
	rr = L * rr;
	if ( rr < 0.001f ) {
#ifdef MBLUR_FUSE
		PFX_STORE0( id.xy, DofTexel( int2( id.xy ), size ) );
#else
		PFX_STORE0( id.xy, PfxT0.SampleLevel( PfxLinear, g, 0.0f ) );
#endif
		return;
	}
	float2 dir = rsqrt( l2 ) * m;
	const float len = min( rr, mc0.y );
	dir = len * dir;
	dir = dir * mc0.x;

	const float2 p0 = min( v1, mc1.xy );
	const float w0 = F4FX_MbTapWeight( F4FX_MbVector( PfxT1, PfxLinear, p0 ), L, mc0.z );
	float3 col = ColorAt( F4FX_MbV( p0 ), size );
	const float4 p12 = min( dir.xyxy * float4( 0.25f, 0.25f, 0.5f, 0.5f ) + v1.xyxy, mc1.xyxy );
	const float w1 = F4FX_MbTapWeight( F4FX_MbVector( PfxT1, PfxLinear, p12.xy ), L, mc0.z );
	float ws = w0 + w1;
	const float3 c1t = w1 * ColorAt( F4FX_MbV( p12.xy ), size );
	col = col * w0 + c1t;
	const float w2 = F4FX_MbTapWeight( F4FX_MbVector( PfxT1, PfxLinear, p12.zw ), L, mc0.z );
	ws = w2 + ws;
	col = ColorAt( F4FX_MbV( p12.zw ), size ) * w2 + col;
	const float2 p3 = min( dir * 0.75f + v1, mc1.xy );
#ifdef PFX_RED
	const float w3 = 0.0f;
#else
	const float w3 = F4FX_MbTapWeight( F4FX_MbVector( PfxT1, PfxLinear, p3 ), L, mc0.z );
#endif
	ws = w3 + ws;
	col = ColorAt( F4FX_MbV( p3 ), size ) * w3 + col;
	ws = ws + 0.001f;
	PFX_STORE0( id.xy, float4( col / ws, 1.0f ) );
}
