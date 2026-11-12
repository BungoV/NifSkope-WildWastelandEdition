// ReSTIR GI's per-pixel rules in HLSL (lane HLSLRESTIR1, 2026-11-12; the twin of src/features/gi/rtddgi/restirgi_common.glsl,
// statement for statement). Shared, no bindings; it calls Restir_Rand / Restir_Seed / Restir_Lum (RestirCommon.hlsli).
// Every function marked MIRROR is src/features/gi/restirgimath.h's function of that name; change all three or none.
// Gate: tests/hlslmatch restirgi / restirgi_filt (this file inside restirgi.hlsl against restirgi.comp, byte for byte).
#ifndef RESTIRGI_COMMON_HLSLI
#define RESTIRGI_COMMON_HLSLI

#include "RestirCommon.hlsli"

#define RESTIRGI_BIT_NOJACOBIAN 1   // restirgimath.h kRedNoJacobian
#define RESTIRGI_BIT_NOVIS 2        // kRedNoVis
#define RESTIRGI_BIT_NOPDF 4        // kRedNoPdf
#define RESTIRGI_BIT_NOTEMPORAL 8   // kRedNoTemporal
#define RESTIRGI_BIT_WHITE 16       // kRedWhite (not a red: the white-noise bounce)
#define RESTIRGI_BIT_R2 32          // kRedR2 (the R2 lattice in place of the STBN mask)
#define RESTIRGI_BIT_NOJCLAMP 64    // kRedNoJClamp
#define RESTIRGI_BIT_LINEAR 256     // kRedLinear (GPU only: row-order invocations, the 8 x 8 tiles' before)
#define RESTIRGI_BIT_HALF 512       // kTraceHalf (the checkerboard trace)
#define RESTIRGI_BIT_QUARTER 1024   // kTraceQuarter (one of each 2 x 2)
#define RESTIRGI_BIT_MERGEVIS 2048  // kRedMergeVis (the spatial merge weight shadowed: the cost cut's before)
#define RESTIRGI_BIT_ALLLIGHTS 4096 // kRedAllLights (the bounce hit lit by every light, a ray each: the light pick's before)
#define RESTIRGI_HISTORY_CAP 20.0   // kHistoryCap
#define RESTIRGI_EPS 0.001          // kEps
#define RESTIRGI_PI 3.14159265358979

// a reservoir with its sample (64 bytes, std430): the hit point (or a miss's direction), its normal, the radiance it sends
struct RgRes
{
	float3 xs;
	float hit;    // 1 a hit, 0 a miss (xs is then the direction)
	float3 ns;
	float wsum;
	float3 Lo;
	float M;
	float W;
	float any;    // 1 when a sample is held
	float pad0;
	float pad1;
};

RgRes RestirGi_Empty()
{
	RgRes r;
	r.xs = float3( 0.0, 0.0, 0.0 );
	r.hit = 0.0;
	r.ns = float3( 0.0, 0.0, 0.0 );
	r.wsum = 0.0;
	r.Lo = float3( 0.0, 0.0, 0.0 );
	r.M = 0.0;
	r.W = 0.0;
	r.any = 0.0;
	r.pad0 = 0.0;
	r.pad1 = 0.0;
	return r;
}

// a bare sample (a fresh bounce) in an empty reservoir
RgRes RestirGi_Sample( float3 xs, float3 ns, float3 Lo, bool hit )
{
	RgRes r = RestirGi_Empty();
	r.xs = xs;
	r.ns = ns;
	r.Lo = Lo;
	r.hit = hit ? 1.0 : 0.0;
	return r;
}

// MIRROR restirgimath.h lum
float RestirGi_Lum( float3 c ) { return Restir_Lum( c.x, c.y, c.z ); }

// MIRROR restirgimath.h cosineDir
float3 RestirGi_CosineDir( float3 n, float u1, float u2 )
{
	const float3 a = abs( n.x ) > 0.9 ? float3( 0.0, 1.0, 0.0 ) : float3( 1.0, 0.0, 0.0 );
	const float3 t = normalize( cross( a, n ) );
	const float3 b = cross( n, t );
	const float r = sqrt( u1 );
	const float ph = 2.0 * RESTIRGI_PI * u2;
	const float z = sqrt( max( 0.0, 1.0 - u1 ) );
	return normalize( t * ( r * cos( ph ) ) + b * ( r * sin( ph ) ) + n * z );
}

// MIRROR restirgimath.h blue: an R2 lattice dither a pixel walked by the 2D R2 sequence a frame (integer math)
float RestirGi_Blue( int x, int y, uint f, uint seed, int dim )
{
	const uint a = dim == 0 ? 3242174889u : 2447445414u;
	const uint b = dim == 0 ? 2447445414u : 3242174889u;
	const uint v = uint( x ) * a + uint( y ) * b + f * a + Restir_Seed( seed, 0u, uint( 7 + dim ) );
	return float( v >> 8u ) * ( 1.0 / 16777216.0 );
}

// MIRROR restirgimath.h stbnIndex
uint RestirGi_StbnIndex( int x, int y, uint f, uint seed, int dim )
{
	const uint ox = ( seed & 127u ) + ( dim == 0 ? 0u : 37u );
	const uint oy = ( ( seed >> 7u ) & 127u ) + ( dim == 0 ? 0u : 71u );
	return ( ( uint( x ) + ox ) & 127u ) + ( ( uint( y ) + oy ) & 127u ) * 128u + ( f & 63u ) * 16384u;
}

// MIRROR restirgimath.h stbnValue
float RestirGi_StbnValue( uint v ) { return ( float( v ) + 0.5 ) * ( 1.0 / 256.0 ); }

// MIRROR restirgimath.h traced: the bounce + spatial pass on this pixel at frame f (the half / quarter trace)
bool RestirGi_Traced( int x, int y, uint f, int red )
{
	if ( ( red & RESTIRGI_BIT_QUARTER ) != 0 ) {
		const int k = int( f & 3u );
		const int o = k == 0 ? 0 : k == 1 ? 3 : k == 2 ? 1 : 2;
		return ( ( x & 1 ) | ( ( y & 1 ) << 1 ) ) == o;
	}
	if ( ( red & RESTIRGI_BIT_HALF ) != 0 )
		return ( ( x + y + int( f & 1u ) ) & 1 ) == 0;
	return true;
}

// MIRROR restirgimath.h snapTraced: a spatial neighbour the half trace skipped moves to its traced row neighbour (with no
// history a skipped pixel's reservoir is empty: half of K would merge nothing). The quarter trace keeps its draw
int2 RestirGi_SnapTraced( int x, int y, uint f, int red )
{
	if ( ( red & RESTIRGI_BIT_QUARTER ) == 0 && ( red & RESTIRGI_BIT_HALF ) != 0 && ( ( x + y + int( f & 1u ) ) & 1 ) != 0 )
		return int2( x ^ 1, y );
	return int2( x, y );
}

// MIRROR restirgimath.h bounceDir (the caller reads the mask's two bytes at RestirGi_StbnIndex dim 0 and 1; mask: it has one)
float3 RestirGi_BounceDir( float3 n, int x, int y, int W, uint seed, uint f, int red, bool mask, uint m0, uint m1 )
{
	if ( ( red & RESTIRGI_BIT_WHITE ) != 0 ) {
		const uint s0 = Restir_Seed( seed ^ uint( y * W + x ), f, 0u );
		return RestirGi_CosineDir( n, Restir_Rand( s0, 0u ), Restir_Rand( s0, 1u ) );
	}
	if ( mask && ( red & RESTIRGI_BIT_R2 ) == 0 )
		return RestirGi_CosineDir( n, RestirGi_StbnValue( m0 ), RestirGi_StbnValue( m1 ) );
	return RestirGi_CosineDir( n, RestirGi_Blue( x, y, f, seed, 0 ), RestirGi_Blue( x, y, f, seed, 1 ) );
}

// MIRROR restirgimath.h dirTo
float3 RestirGi_DirTo( float3 x, RgRes y ) { return y.hit > 0.5 ? normalize( y.xs - x ) : y.xs; }

// MIRROR restirgimath.h target: lum(Lo) x cos theta, visibility not included
float RestirGi_Target( float3 x, float3 n, RgRes y )
{
	const float c = dot( n, RestirGi_DirTo( x, y ) );
	return c > 0.0 ? RestirGi_Lum( y.Lo ) * c : 0.0;
}

// MIRROR restirgimath.h jacobian
float RestirGi_Jacobian( float3 xv, float3 xq, RgRes y, int red )
{
	if ( y.hit < 0.5 || ( red & RESTIRGI_BIT_NOJACOBIAN ) != 0 )
		return 1.0;
	const float3 dv = xv - y.xs;
	const float3 dq = xq - y.xs;
	const float lv = dot( dv, dv );
	const float lq = dot( dq, dq );
	if ( lv <= 0.0 || lq <= 0.0 )
		return 0.0;
	const float cv = abs( dot( y.ns, dv ) ) / sqrt( lv );
	const float cq = abs( dot( y.ns, dq ) ) / sqrt( lq );
	return cq > 1e-6 ? ( cv / cq ) * ( lq / lv ) : 0.0;
}

// MIRROR restirgimath.h jacobianKept: -1 drops the neighbour (J outside [1/10, 10])
float RestirGi_JacobianKept( float j, int red )
{
	if ( ( red & RESTIRGI_BIT_NOJCLAMP ) != 0 )
		return j;
	return j >= 0.1 && j <= 10.0 ? j : -1.0;
}

// MIRROR restirgimath.h update
RgRes RestirGi_Update( RgRes r, RgRes y, float w, float m, float u )
{
	r.M = r.M + m;
	if ( !( w > 0.0 ) )
		return r;
	r.wsum = r.wsum + w;
	if ( u * r.wsum < w ) {
		r.xs = y.xs;
		r.hit = y.hit;
		r.ns = y.ns;
		r.Lo = y.Lo;
		r.any = 1.0;
	}
	return r;
}

// MIRROR restirgimath.h finalize
RgRes RestirGi_Finalize( RgRes r, float pHat, float Z )
{
	r.W = r.any > 0.5 && pHat > 0.0 && Z > 0.0 ? r.wsum / ( Z * pHat ) : 0.0;
	return r;
}

// MIRROR restirgimath.h candidate
RgRes RestirGi_Candidate( float3 x, float3 n, RgRes y, int red )
{
	const float pHat = RestirGi_Target( x, n, y );
	const float pdf = dot( n, RestirGi_DirTo( x, y ) ) / RESTIRGI_PI;
	const float w = ( red & RESTIRGI_BIT_NOPDF ) != 0 ? pHat : ( pdf > 0.0 ? pHat / pdf : 0.0 );
	const RgRes r = RestirGi_Update( RestirGi_Empty(), y, w, 1.0, 0.0 );
	return RestirGi_Finalize( r, pHat, r.M );
}

// MIRROR restirgimath.h estimate
float3 RestirGi_Estimate( float3 x, float3 n, RgRes r )
{
	if ( r.any < 0.5 || !( r.W > 0.0 ) )
		return float3( 0.0, 0.0, 0.0 );
	const float c = max( 0.0, dot( n, RestirGi_DirTo( x, r ) ) );
	return r.Lo * ( c * r.W );
}

#endif
