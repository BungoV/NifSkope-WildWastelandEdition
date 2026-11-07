// Lane RTEMIS1 part 2 (2026-11-06): ReSTIR GI's per-pixel rules, included by restirgi.comp after restir_common.glsl
// (restirLum, restirRand, restirSeed) and compiled as C++ by tests/features/gi/restirgi_math (through
// tests/restir_math/restir_shim.h and restirgi_shim.h). Every function marked MIRROR is src/features/gi/restirgimath.h's
// function of that name; change both or neither. The subset restir_common.glsl keeps: no swizzles, no out/inout
// parameters, struct returns, literals that read the same in both languages.

#define RESTIRGI_BIT_NOJACOBIAN 1   // restirgimath.h kRedNoJacobian
#define RESTIRGI_BIT_NOVIS 2        // kRedNoVis
#define RESTIRGI_BIT_NOPDF 4        // kRedNoPdf
#define RESTIRGI_BIT_NOTEMPORAL 8   // kRedNoTemporal
#define RESTIRGI_BIT_WHITE 16       // kRedWhite (not a red: the white-noise bounce)
#define RESTIRGI_BIT_R2 32          // kRedR2 (the R2 lattice in place of the STBN mask)
#define RESTIRGI_BIT_NOJCLAMP 64    // kRedNoJClamp
#define RESTIRGI_BIT_LINEAR 256     // kRedLinear (GPU only: row-order invocations, the 8 x 8 tiles' before)
#define RESTIRGI_HISTORY_CAP 20.0   // kHistoryCap
#define RESTIRGI_EPS 0.001          // kEps
#define RESTIRGI_PI 3.14159265358979

// a reservoir with its sample (64 bytes, std430): the hit point (or a miss's direction), its normal, the radiance it sends
struct RgRes
{
	vec3 xs;
	float hit;    // 1 a hit, 0 a miss (xs is then the direction)
	vec3 ns;
	float wsum;
	vec3 Lo;
	float M;
	float W;
	float any;    // 1 when a sample is held
	float pad0;
	float pad1;
};

RgRes rgEmpty()
{
	RgRes r;
	r.xs = vec3( 0.0 );
	r.hit = 0.0;
	r.ns = vec3( 0.0 );
	r.wsum = 0.0;
	r.Lo = vec3( 0.0 );
	r.M = 0.0;
	r.W = 0.0;
	r.any = 0.0;
	r.pad0 = 0.0;
	r.pad1 = 0.0;
	return r;
}

// a bare sample (a fresh bounce) in an empty reservoir
RgRes rgSample( vec3 xs, vec3 ns, vec3 Lo, bool hit )
{
	RgRes r = rgEmpty();
	r.xs = xs;
	r.ns = ns;
	r.Lo = Lo;
	r.hit = hit ? 1.0 : 0.0;
	return r;
}

// MIRROR restirgimath.h lum
float rgLum( vec3 c ) { return restirLum( c.x, c.y, c.z ); }

// MIRROR restirgimath.h cosineDir
vec3 rgCosineDir( vec3 n, float u1, float u2 )
{
	const vec3 a = abs( n.x ) > 0.9 ? vec3( 0.0, 1.0, 0.0 ) : vec3( 1.0, 0.0, 0.0 );
	const vec3 t = normalize( cross( a, n ) );
	const vec3 b = cross( n, t );
	const float r = sqrt( u1 );
	const float ph = 2.0 * RESTIRGI_PI * u2;
	const float z = sqrt( max( 0.0, 1.0 - u1 ) );
	return normalize( t * ( r * cos( ph ) ) + b * ( r * sin( ph ) ) + n * z );
}

// MIRROR restirgimath.h blue: an R2 lattice dither a pixel walked by the 2D R2 sequence a frame (integer math)
float rgBlue( int x, int y, uint f, uint seed, int dim )
{
	const uint a = dim == 0 ? 3242174889u : 2447445414u;
	const uint b = dim == 0 ? 2447445414u : 3242174889u;
	const uint v = uint( x ) * a + uint( y ) * b + f * a + restirSeed( seed, 0u, uint( 7 + dim ) );
	return float( v >> 8u ) * ( 1.0 / 16777216.0 );
}

// MIRROR restirgimath.h stbnIndex
uint rgStbnIndex( int x, int y, uint f, uint seed, int dim )
{
	const uint ox = ( seed & 127u ) + ( dim == 0 ? 0u : 37u );
	const uint oy = ( ( seed >> 7u ) & 127u ) + ( dim == 0 ? 0u : 71u );
	return ( ( uint( x ) + ox ) & 127u ) + ( ( uint( y ) + oy ) & 127u ) * 128u + ( f & 63u ) * 16384u;
}

// MIRROR restirgimath.h stbnValue
float rgStbnValue( uint v ) { return ( float( v ) + 0.5 ) * ( 1.0 / 256.0 ); }

// MIRROR restirgimath.h bounceDir (the caller reads the mask's two bytes at rgStbnIndex dim 0 and 1; mask: it has one)
vec3 rgBounceDir( vec3 n, int x, int y, int W, uint seed, uint f, int red, bool mask, uint m0, uint m1 )
{
	if ( ( red & RESTIRGI_BIT_WHITE ) != 0 ) {
		const uint s0 = restirSeed( seed ^ uint( y * W + x ), f, 0u );
		return rgCosineDir( n, restirRand( s0, 0u ), restirRand( s0, 1u ) );
	}
	if ( mask && ( red & RESTIRGI_BIT_R2 ) == 0 )
		return rgCosineDir( n, rgStbnValue( m0 ), rgStbnValue( m1 ) );
	return rgCosineDir( n, rgBlue( x, y, f, seed, 0 ), rgBlue( x, y, f, seed, 1 ) );
}

// MIRROR restirgimath.h dirTo
vec3 rgDirTo( vec3 x, RgRes y ) { return y.hit > 0.5 ? normalize( y.xs - x ) : y.xs; }

// MIRROR restirgimath.h target: lum(Lo) x cos theta, visibility not included
float rgTarget( vec3 x, vec3 n, RgRes y )
{
	const float c = dot( n, rgDirTo( x, y ) );
	return c > 0.0 ? rgLum( y.Lo ) * c : 0.0;
}

// MIRROR restirgimath.h jacobian
float rgJacobian( vec3 xv, vec3 xq, RgRes y, int red )
{
	if ( y.hit < 0.5 || ( red & RESTIRGI_BIT_NOJACOBIAN ) != 0 )
		return 1.0;
	const vec3 dv = xv - y.xs;
	const vec3 dq = xq - y.xs;
	const float lv = dot( dv, dv );
	const float lq = dot( dq, dq );
	if ( lv <= 0.0 || lq <= 0.0 )
		return 0.0;
	const float cv = abs( dot( y.ns, dv ) ) / sqrt( lv );
	const float cq = abs( dot( y.ns, dq ) ) / sqrt( lq );
	return cq > 1e-6 ? ( cv / cq ) * ( lq / lv ) : 0.0;
}

// MIRROR restirgimath.h jacobianKept: -1 drops the neighbour (J outside [1/10, 10])
float rgJacobianKept( float j, int red )
{
	if ( ( red & RESTIRGI_BIT_NOJCLAMP ) != 0 )
		return j;
	return j >= 0.1 && j <= 10.0 ? j : -1.0;
}

// MIRROR restirgimath.h update
RgRes rgUpdate( RgRes r, RgRes y, float w, float m, float u )
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
RgRes rgFinalize( RgRes r, float pHat, float Z )
{
	r.W = r.any > 0.5 && pHat > 0.0 && Z > 0.0 ? r.wsum / ( Z * pHat ) : 0.0;
	return r;
}

// MIRROR restirgimath.h candidate
RgRes rgCandidate( vec3 x, vec3 n, RgRes y, int red )
{
	const float pHat = rgTarget( x, n, y );
	const float pdf = dot( n, rgDirTo( x, y ) ) / RESTIRGI_PI;
	const float w = ( red & RESTIRGI_BIT_NOPDF ) != 0 ? pHat : ( pdf > 0.0 ? pHat / pdf : 0.0 );
	const RgRes r = rgUpdate( rgEmpty(), y, w, 1.0, 0.0 );
	return rgFinalize( r, pHat, r.M );
}

// MIRROR restirgimath.h estimate
vec3 rgEstimate( vec3 x, vec3 n, RgRes r )
{
	if ( r.any < 0.5 || !( r.W > 0.0 ) )
		return vec3( 0.0 );
	const float c = max( 0.0, dot( n, rgDirTo( x, r ) ) );
	return r.Lo * ( c * r.W );
}
