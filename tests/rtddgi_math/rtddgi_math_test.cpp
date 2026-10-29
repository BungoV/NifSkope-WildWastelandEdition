// rtddgi_math_test -- lane RTXGI1 M1 (2026-10-29): the DDGI probe math of src/rtddgimath.h, no Qt, no GPU.
//
//   g++ -O2 -std=c++17 -I src tests/rtddgi_math/rtddgi_math_test.cpp -o rtddgi_math_test -static
//   ./rtddgi_math_test            -> every check PASS, last line "RTDDGI_MATH PASS 14", exit 0
//   -DRTDDGI_RED_<NAME>           -> swaps a wrong rule into the header; check <name> must FAIL (exit 1)
//
// Checks and their reds (tests/rtddgi_math/run.sh builds all fourteen and asserts each):
//   oct     octahedral encode/decode round trip + texel centres            RED_OCT     no lower-hemisphere fold
//   fib     spherical Fibonacci covers the sphere; rotation; fixed rays     RED_FIB     strata from the pole only
//   hyst    [J21] per-texel thresholds 0.25 / 0.8, age ramp                 RED_HYST    thresholds swapped
//   gamma   gamma-5 encode/decode                                           RED_GAMMA   gamma 2 encode
//   cheb    Chebyshev weight: 1 before the mean, cubed, floor 0.05          RED_CHEB    cube dropped
//   bias    [J21 eq 2] self-shadow bias (0.2 n + 0.8 o) 0.75 D B            RED_BIAS    weights swapped
//   border  the octahedral border copy continues the map across the edge    RED_BORDER  clamp copy, no mirror
//   sky     the sky radiance cube gives an open probe the weather's amb     RED_SKY     naive per-axis cube
//   norm    blend of uniform radiance L decodes (x 2 pi) to pi L            RED_NORM    no 0.5
//   class   classification Off / Asleep / Vigilant                          RED_CLASS   threshold 0.75
//   reloc   relocation through a backface, and away from a near face        RED_RELOC   never moves through
//   tie     M1FIX nearest-hit tie: front beats back, then the lower number    RED_TIE     the number alone
//   skyvis  M1FIX sky visibility: cosine-weighted miss share, no gamma, the   RED_SKYVIS  plain share of facing rays
//           irradiance texel's own h
//   sample  M2 the 8-probe read: res/shaders/cell_ddgi.glsl (compiled here    RED_SAMPLE  the header drops the
//           through glsl_shim.h) equals sampleVolume; a wall's far side is     backface weight
//           dropped, the trilinear red takes it in
// Every check is independent of the others' reds (a red build may pass the other thirteen).

#include "rtddgimath.h"

#include <cmath>
#include <cstdio>
#include <vector>

// M2: the cell view's GLSL read, compiled here from the shader's own file (res/shaders/cell_ddgi.glsl)
namespace glsl {
#include "glsl_shim.h"
#include "../../res/shaders/cell_ddgi.glsl"
}
#undef uniform

using namespace rtddgi;

namespace {

int g_fail = 0, g_pass = 0;

void report( const char * name, bool ok, const char * fmt, double a = 0, double b = 0, double c = 0 )
{
	char msg[256];
	std::snprintf( msg, sizeof msg, fmt, a, b, c );
	std::printf( "%s %s: %s\n", ok ? "PASS" : "FAIL", name, msg );
	( ok ? g_pass : g_fail )++;
}

// atan2 in double: acos of a float dot cannot resolve below ~3.5e-4 rad (one float step under 1.0)
double angle( V3 a, V3 b )
{
	const double ax = a.x, ay = a.y, az = a.z, bx = b.x, by = b.y, bz = b.z;
	const double cx = ay * bz - az * by, cy = az * bx - ax * bz, cz = ax * by - ay * bx;
	return std::atan2( std::sqrt( cx * cx + cy * cy + cz * cz ), ax * bx + ay * by + az * bz );
}

void checkOct()
{
	double worst = 0;
	const int M = 20000;
	for ( int i = 0; i < M; i++ ) {
		const V3 d = sphericalFibonacci( i, M );
		float u, v;
		octEncode( d, u, v );
		worst = std::max( worst, angle( d, octDecode( u, v ) ) );
	}
	double worstUv = 0;
	for ( int n : { kIrrInterior, kDistInterior } )
		for ( int ty = 0; ty < n; ty++ )
			for ( int tx = 0; tx < n; tx++ ) {
				float u, v;
				octEncode( texelDir( tx, ty, n ), u, v );
				const double eu = std::fabs( u - ( ( tx + 0.5 ) / n * 2.0 - 1.0 ) ), ev = std::fabs( v - ( ( ty + 0.5 ) / n * 2.0 - 1.0 ) );
				worstUv = std::max( worstUv, std::max( eu, ev ) );
			}
	report( "oct", worst < 1e-4 && worstUv < 1e-5, "round trip worst %.3g rad (bar 1e-4), texel centre worst %.3g (bar 1e-5)", worst, worstUv );
}

void checkFib()
{
	const int n = 96;   // the blended rays of 128
	V3 mean( 0, 0, 0 );
	std::vector<V3> s;
	for ( int i = 0; i < n; i++ ) {
		s.push_back( sphericalFibonacci( i, n ) );
		mean = mean + s.back() * ( 1.0f / n );
	}
	double cover = 0;   // the largest angle from any direction to the set
	const int M = 8192;
	for ( int k = 0; k < M; k++ ) {
		const V3 q = octDecode( std::fmod( k * 0.7548776662f, 2.0f ) - 1.0f, std::fmod( k * 0.5698402910f, 2.0f ) - 1.0f );
		double best = 10;
		for ( const V3 & d : s )
			best = std::min( best, angle( q, d ) );
		cover = std::max( cover, best );
	}
	double orth = 0;
	for ( uint32_t f = 0; f < 100; f++ ) {
		const M3 r = updateRotation( 7, f );
		const V3 c0( r.m[0], r.m[3], r.m[6] ), c1( r.m[1], r.m[4], r.m[7] ), c2( r.m[2], r.m[5], r.m[8] );
		orth = std::max( orth, double( std::fabs( dot( c0, c1 ) ) + std::fabs( dot( c1, c2 ) ) + std::fabs( dot( c0, c2 ) ) ) );
		orth = std::max( orth, double( std::fabs( length( c0 ) - 1 ) + std::fabs( length( c1 ) - 1 ) + std::fabs( length( c2 ) - 1 ) ) );
		orth = std::max( orth, double( std::fabs( dot( cross( c0, c1 ), c2 ) - 1 ) ) );   // det +1: a rotation, not a reflection
	}
	const M3 ra = updateRotation( 7, 1 ), rb = updateRotation( 7, 2 );
	double fixedMoved = 0, rotMoved = 0;
	for ( int i = 0; i < 128; i++ ) {
		const double a = angle( rayDir( i, 128, ra ), rayDir( i, 128, rb ) );
		if ( i < kFixedRays )
			fixedMoved = std::max( fixedMoved, a );
		else
			rotMoved = std::max( rotMoved, a );
	}
	const double bar = 0.35;   // ~1.6 x the hexagonal covering radius of 96 points (0.224 rad)
	report( "fib", length( mean ) < 0.02 && cover < bar && orth < 1e-4 && fixedMoved < 1e-6 && rotMoved > 0.05,
		"|mean| %.4f (bar 0.02), covering %.3f rad (bar 0.35), rotation error %.2g", length( mean ), cover, orth );
	if ( fixedMoved >= 1e-6 || rotMoved <= 0.05 )
		std::printf( "      fixed rays moved %.3g, rotated rays moved %.3g\n", fixedMoved, rotMoved );
}

void checkHyst()
{
	const float old[3] = { 0.5f, 0.5f, 0.5f };
	const float s[3] = { 0.6f, 0.5f, 0.5f }, m[3] = { 0.5f, 0.8f, 0.5f }, l[3] = { 0.5f, 0.5f, 1.4f };
	const float h0 = texelHysteresis( 0.97f, old, s ), h1 = texelHysteresis( 0.97f, old, m ), h2 = texelHysteresis( 0.97f, old, l );
	const bool ages = ageHysteresis( 0, 0.97f ) == 0.0f && std::fabs( ageHysteresis( 1, 0.97f ) - 0.5f ) < 1e-6f
		&& ageHysteresis( 1000, 0.97f ) == 0.97f;
	// the distance blend never takes the thresholds: a huge change still blends at the base h
	const V3 d[1] = { V3( 0, 0, 1 ) };
	const float dist[1] = { 900.0f }, oldD[2] = { 10.0f, 100.0f };
	float outD[2];
	blendDistanceTexel( V3( 0, 0, 1 ), d, dist, 1, 50.0f, 1e9f, 0.9f, oldD, outD );
	const bool distKeeps = std::fabs( outD[0] - ( 900.0f * 0.1f + 10.0f * 0.9f ) ) < 1e-3f;
	report( "hyst", std::fabs( h0 - 0.97f ) < 1e-6f && std::fabs( h1 - 0.82f ) < 1e-6f && h2 == 0.0f && ages && distKeeps,
		"change 0.1 -> h %.3f (0.97), 0.3 -> %.3f (0.82), 0.9 -> %.3f (0)", h0, h1, h2 );
}

void checkGamma()
{
	double worst = 0;
	for ( float x : { 1e-4f, 0.01f, 0.5f, 1.0f, 7.3f } )
		worst = std::max( worst, std::fabs( double( gammaDecode( gammaEncode( x ) ) ) - x ) / x );
	const float e = gammaEncode( 1.0f / 32.0f );
	report( "gamma", worst < 1e-4 && std::fabs( e - 0.5f ) < 1e-5f, "round trip worst rel %.3g, encode(1/32) %.5f (0.5)", worst, e );
}

void checkCheb()
{
	const float before = chebyshevWeight( 90.0f, 100.0f, 100.0f * 100.0f + 1.0f );
	const float at1 = chebyshevWeight( 101.0f, 100.0f, 100.0f * 100.0f + 1.0f );   // var 1, d 1 -> 0.5, cubed 0.125
	const float farW = chebyshevWeight( 200.0f, 100.0f, 100.0f * 100.0f + 1.0f );
	report( "cheb", before == 1.0f && std::fabs( at1 - 0.125f ) < 1e-5f && std::fabs( farW - 0.05f ) < 1e-6f,
		"before the mean %.3f (1), var 1 at +1 %.4f (0.125), far %.4f (0.05)", before, at1, farW );
}

void checkBias()
{
	const V3 b = biasVector( V3( 1, 0, 0 ), V3( 0, 1, 0 ), 140.0f, 0.3f );   // 0.75 x 140 x 0.3 = 31.5
	report( "bias", std::fabs( b.x - 6.3f ) < 1e-4f && std::fabs( b.y - 25.2f ) < 1e-4f && std::fabs( b.z ) < 1e-6f,
		"n=+X o=+Y D=140 B=0.3 -> (%.3f, %.3f, %.3f), want (6.3, 25.2, 0)", b.x, b.y, b.z );
}

// the octahedral identity, written apart from the header: a point past an edge is the mirror point across it
void foldUv( float & u, float & v )
{
	if ( u > 1.0f ) { u = 2.0f - u; v = -v; }
	else if ( u < -1.0f ) { u = -2.0f - u; v = -v; }
	if ( v > 1.0f ) { v = 2.0f - v; u = -u; }
	else if ( v < -1.0f ) { v = -2.0f - v; u = -u; }
}

void checkBorder()
{
	double worst = 0;
	for ( int S : { kIrrTile, kDistTile } ) {
		const int n = S - 2;
		for ( int k = 0; k < 4 * S - 4; k++ ) {
			int x, y, sx, sy;
			borderXY( k, S, x, y );
			borderSource( x, y, S, sx, sy );
			if ( sx < 1 || sx > n || sy < 1 || sy > n ) {
				worst = 10;
				continue;
			}
			float u = ( float( x - 1 ) + 0.5f ) / float( n ) * 2.0f - 1.0f, v = ( float( y - 1 ) + 0.5f ) / float( n ) * 2.0f - 1.0f;
			foldUv( u, v );
			worst = std::max( worst, angle( octDecode( u, v ), texelDir( sx - 1, sy - 1, n ) ) );
		}
	}
	report( "border", worst < 1e-5, "border texel vs the folded map, worst %.3g rad (bar 1e-5)", worst );
}

void checkSky()
{
	// a sky-bright weather the cube holds without clamping (a side darker than ~(top + bottom) / 4 cannot be held:
	// skyRadianceCube clamps it and the volume's census counts it)
	const float amb[6][3] = { { 0.5f, 0.4f, 0.45f }, { 0.3f, 0.35f, 0.4f }, { 0.45f, 0.4f, 0.4f }, { 0.35f, 0.35f, 0.45f },
		{ 1.0f, 1.1f, 1.2f }, { 0.15f, 0.12f, 0.2f } };
	float R[6][3];
	const int clamped = skyRadianceCube( amb, R );
	static const float ax[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
	double E[6][3] = {};
	const int M = 200000;
	for ( int i = 0; i < M; i++ ) {
		const V3 d = sphericalFibonacci( i, M );
		float L[3];
		skyRadiance( R, d, L );
		for ( int f = 0; f < 6; f++ ) {
			const double c = std::max( 0.0f, dot( d, V3( ax[f][0], ax[f][1], ax[f][2] ) ) );
			for ( int k = 0; k < 3; k++ )
				E[f][k] += L[k] * c * 4.0 * kPi / M;
		}
	}
	double worst = 0;
	for ( int f = 0; f < 6; f++ )
		for ( int k = 0; k < 3; k++ )
			worst = std::max( worst, std::fabs( E[f][k] - amb[f][k] ) / amb[f][k] );
	report( "sky", worst < 0.005 && clamped == 0, "open probe irradiance vs amb, worst rel %.4f (bar 0.005), clamped %g", worst, clamped );
}

void checkNorm()
{
	const int N = 128, nb = N - kFixedRays;
	const M3 r = updateRotation( 3, 5 );
	std::vector<V3> dirs;
	std::vector<float> rad;
	const float L = 0.3f;
	for ( int i = kFixedRays; i < N; i++ ) {
		dirs.push_back( rayDir( i, N, r ) );
		rad.insert( rad.end(), { L, L, L } );
	}
	double worst = 0;
	for ( int ty = 0; ty < kIrrInterior; ty++ )
		for ( int tx = 0; tx < kIrrInterior; tx++ ) {
			const float old[3] = { 0, 0, 0 };
			float out[3];
			blendIrradianceTexel( texelDir( tx, ty, kIrrInterior ), dirs.data(), rad.data(), nb, 0.0f, old, out );
			const double E = gammaDecode( out[0] ) * 2.0 * kPi;
			worst = std::max( worst, std::fabs( E - kPi * L ) / ( kPi * L ) );
		}
	report( "norm", worst < 1e-4, "uniform L 0.3 decodes x 2 pi to pi L, worst rel %.3g (bar 1e-4)", worst );
}

void checkClass()
{
	V3 dirs[kFixedRays];
	float d[kFixedRays];
	for ( int i = 0; i < kFixedRays; i++ )
		dirs[i] = sphericalFibonacci( i, kFixedRays );
	const V3 st( 140, 140, 140 );
	for ( int i = 0; i < kFixedRays; i++ ) d[i] = -0.2f * 50.0f;
	const int allBack = classify( dirs, d, kFixedRays, st );
	for ( int i = 0; i < kFixedRays; i++ ) d[i] = i % 2 ? -0.2f * 50.0f : 500.0f;
	const int halfBack = classify( dirs, d, kFixedRays, st );
	for ( int i = 0; i < kFixedRays; i++ ) d[i] = kMiss;
	const int open = classify( dirs, d, kFixedRays, st );
	d[5] = 10.0f;
	const int nearHit = classify( dirs, d, kFixedRays, st );
	report( "class", allBack == kOff && halfBack == kOff && open == kAsleep && nearHit == kVigilant,
		"all back %g (0 off), half back %g (0 off), open %g (1 asleep)", allBack, halfBack, open );
	if ( nearHit != kVigilant )
		std::printf( "      one hit at 10: %d (2 vigilant)\n", nearHit );
}

void checkReloc()
{
	V3 dirs[kFixedRays];
	float d[kFixedRays];
	for ( int i = 0; i < kFixedRays; i++ ) {
		dirs[i] = sphericalFibonacci( i, kFixedRays );
		d[i] = dirs[i].z < 0.0f ? -0.2f * 10.0f : 100.0f;   // a slab under the probe seen from inside: its backs 10 below
	}
	const V3 st( 140, 140, 140 );
	const V3 o1 = relocateStep( dirs, d, kFixedRays, V3( 0, 0, 0 ), st );
	bool inLimit = true;
	for ( int k = 0; k < 3; k++ )
		inLimit = inLimit && std::fabs( o1[k] ) <= 0.45f * 140.0f + 1e-3f;
	const bool through = length( o1 ) > 10.0f && o1.z < 0.0f && inLimit;
	for ( int i = 0; i < kFixedRays; i++ )   // a floor 5 below, open above: step up 0.2 x 140 = 28
		d[i] = dirs[i].z < -0.9f ? 5.0f : kMiss;
	const V3 o2 = relocateStep( dirs, d, kFixedRays, V3( 0, 0, 0 ), st );
	const bool away = o2.z > 10.0f && length( o2 ) <= 28.0f + 1e-3f;
	report( "reloc", through && away, "through the backface |offset| %.2f (z %.2f), off the floor z %.2f", length( o1 ), o1.z, o2.z );
}

void checkTie()
{
	// M1FIX: inside the tie window a front face beats a back face whatever their numbers; alike faces, the lower number
	const bool frontOverBack = tieWins( false, 9, true, 3 ) && !tieWins( true, 3, false, 9 );
	const bool lowerAmongLike = tieWins( false, 3, false, 9 ) && !tieWins( false, 9, false, 3 ) && tieWins( true, 3, true, 9 );
	const bool notItself = !tieWins( false, 5, false, 5 ) && !tieWins( true, 5, true, 5 );
	const float w = tieWindow( V3( 10000, -5000, 7000 ), 1000 );   // 1e-4 + 2e-6 x (22000 + 1000)
	report( "tie", frontOverBack && lowerAmongLike && notItself && std::fabs( w - 0.0461f ) < 1e-5f,
		"front beats back %g, lower number among alike %g, window at |o|_1 22000 t 1000: %.5f (0.0461)", frontOverBack, lowerAmongLike, w );
}

void checkSkyVis()
{
	// M1FIX: an open cone of sky above (cos > 0.5, a courtyard), solid elsewhere. For the +Z texel the cosine-weighted
	// miss share is sin^2(60 deg) = 0.75; the plain share of the facing rays (the red) is 0.5. No gamma: h 0.5 over an
	// old 0.2 gives 0.475 (a gamma-5 encode would give 0.944 for the first).
	const int M = 20000;
	std::vector<V3> dirs;
	std::vector<float> dist;
	for ( int i = 0; i < M; i++ ) {
		dirs.push_back( sphericalFibonacci( i, M ) );
		dist.push_back( dirs.back().z > 0.5f ? kMiss : 300.0f );
	}
	float v0 = -1.0f, v1 = -1.0f;
	blendSkyVisTexel( V3( 0, 0, 1 ), dirs.data(), dist.data(), M, 0.0f, 0.0f, &v0 );
	blendSkyVisTexel( V3( 0, 0, 1 ), dirs.data(), dist.data(), M, 0.5f, 0.2f, &v1 );
	// the h it is given is the irradiance texel's own (blendIrradianceTexel's hOut): uniform L 0.3 over an old 0, the
	// texel's h recomputed here from the same encode and thresholds (so the other checks' reds leave this one alone)
	std::vector<float> rad( size_t( M ) * 3, 0.3f );
	const float old3[3] = { 0, 0, 0 };
	float out[3], h = -1.0f, encNew[3];
	blendIrradianceTexel( V3( 0, 0, 1 ), dirs.data(), rad.data(), M, 0.97f, old3, out, &h );
	double s = 0, wsum = 0;
	for ( int r = 0; r < M; r++ ) {
		const float c = std::max( 0.0f, dot( V3( 0, 0, 1 ), dirs[size_t( r )] ) );
		if ( c <= 0.0f )
			continue;
		s += double( c ) * 0.3;
		wsum += c;
	}
	for ( int k = 0; k < 3; k++ )
		encNew[k] = gammaEncode( float( 0.5 * s / wsum ) );
	const float hWant = texelHysteresis( 0.97f, old3, encNew );
	report( "skyvis", std::fabs( v0 - 0.75f ) < 0.005f && std::fabs( v1 - 0.475f ) < 0.005f && std::fabs( h - hWant ) < 1e-6f && hWant < 0.9f,
		"open cone cos > 0.5: %.4f (0.75), h 0.5 over 0.2: %.4f (0.475), hOut - the texel's h %.2g (0)", v0, v1, h - hWant );
}

// M2: a volume in the GPU's buffers and the same volume as the cell view's texture buffer (cell_ddgi.glsl's layout)
struct TestVolume
{
	SampleVolume v;
	std::vector<float> irr, dist, probes, atlas;
	std::vector<uint32_t> age;
	void pack( int nP )
	{
		atlas.assign( size_t( nP ) * 193 * 4, 0.0f );
		std::copy( irr.begin(), irr.end(), atlas.begin() );                                   // 64 texels a probe
		std::copy( dist.begin(), dist.end(), atlas.begin() + std::ptrdiff_t( nP ) * 64 * 4 );  // 128 texels a probe
		for ( int p = 0; p < nP; p++ ) {   // src/rtddgicell.cpp packs the probes' texels the same way
			float * t = &atlas[( size_t( nP ) * 192 + size_t( p ) ) * 4];
			for ( int k = 0; k < 3; k++ )
				t[k] = probes[size_t( p ) * 4 + size_t( k )];
			t[3] = probeUsable( int( probes[size_t( p ) * 4 + 3] + 0.5f ), age[size_t( p )] ) ? float( kVigilant ) : float( kOff );
		}
		v.irr = irr.data();
		v.dist = dist.data();
		v.probes = probes.data();
		v.age = age.data();
	}
	void toGlsl( int nP, unsigned red ) const
	{
		glsl::cellDdgiAtlas.data = atlas.data();
		glsl::cellDdgiAtlas.texels = nP * 193;
		glsl::cellDdgiAtlas.outside = 0;
		glsl::cellDdgiOrigin = glsl::vec4( v.origin.x, v.origin.y, v.origin.z, v.D );
		glsl::cellDdgiStep = glsl::vec4( v.step.x, v.step.y, v.step.z, v.B );
		glsl::cellDdgiDims.x = v.dims[0];
		glsl::cellDdgiDims.y = v.dims[1];
		glsl::cellDdgiDims.z = v.dims[2];
		glsl::cellDdgiDims.w = nP;
		glsl::cellDdgiRed = int( red );
	}
};

V3 randUnit( uint64_t & s )
{
	for ( ;; ) {
		const V3 q( unit01( s ) * 2.0f - 1.0f, unit01( s ) * 2.0f - 1.0f, unit01( s ) * 2.0f - 1.0f );
		const float l = length( q );
		if ( l > 0.05f && l <= 1.0f )
			return q * ( 1.0f / l );
	}
}

void checkSample()
{
	// (1) the GLSL read (res/shaders/cell_ddgi.glsl, as written) equals rtddgimath.h sampleVolume on random volumes,
	// points, normals, view and read directions, under each red bit the cell passes (0, trilinear, nocheb + nobias)
	uint64_t s = 0x5eed2910ULL;
	TestVolume T;
	const int dims[3] = { 5, 4, 3 }, nP = 5 * 4 * 3;
	T.v.origin = V3( -1000.0f, 2000.0f, 50.0f );
	T.v.step = V3( 140.0f, 140.0f, 105.0f );
	for ( int k = 0; k < 3; k++ )
		T.v.dims[k] = dims[k];
	T.v.D = 105.0f;
	T.v.B = kSelfShadowBias;
	T.irr.resize( size_t( nP ) * 256 );
	T.dist.resize( size_t( nP ) * 512 );
	T.probes.resize( size_t( nP ) * 4 );
	T.age.resize( size_t( nP ) );
	for ( float & x : T.irr )
		x = unit01( s );
	for ( size_t i = 0; i < T.dist.size(); i += 2 ) {
		const float mean = 20.0f + 380.0f * unit01( s );
		T.dist[i] = mean;
		T.dist[i + 1] = mean * mean + 4000.0f * unit01( s );
	}
	for ( int p = 0; p < nP; p++ ) {
		for ( int k = 0; k < 3; k++ )
			T.probes[size_t( p ) * 4 + size_t( k )] = ( unit01( s ) - 0.5f ) * 40.0f;
		const float u = unit01( s );
		T.probes[size_t( p ) * 4 + 3] = float( u < 0.12f ? kOff : u < 0.24f ? kAsleep : kVigilant );
		T.age[size_t( p )] = unit01( s ) < 0.1f ? 0u : 1u + uint32_t( unit01( s ) * 50.0f );
	}
	T.pack( nP );
	double worstE = 0, worstV = 0, worstW = 0, worstF = 0;
	int points = 0, noProbe = 0, outside = 0;
	const unsigned reds[3] = { 0u, kSampleTrilinear, kSampleNoCheb | kSampleNoBias };
	for ( unsigned red : reds ) {
		T.toGlsl( nP, red );
		for ( int i = 0; i < 4000; i++ ) {
			// the box and a cell past it (the fade and the clamped cage are read there too)
			const V3 P( T.v.origin.x + ( unit01( s ) * 6.0f - 1.0f ) * T.v.step.x, T.v.origin.y + ( unit01( s ) * 5.0f - 1.0f ) * T.v.step.y,
				T.v.origin.z + ( unit01( s ) * 4.0f - 1.0f ) * T.v.step.z );
			const V3 n = randUnit( s ), o = randUnit( s ), d = i % 3 == 0 ? randUnit( s ) : n;
			const SampleResult a = sampleVolume( T.v, P, n, o, d, red );
			const glsl::CellDdgiOut b = glsl::cellDdgiRead( glsl::vec3( P.x, P.y, P.z ), glsl::vec3( n.x, n.y, n.z ),
				glsl::vec3( o.x, o.y, o.z ), glsl::vec3( d.x, d.y, d.z ) );
			points++;
			noProbe += a.w <= 0.0f ? 1 : 0;
			const double eMax = std::max( { 1e-3, double( a.E.x ), double( a.E.y ), double( a.E.z ) } );
			worstE = std::max( { worstE, std::fabs( a.E.x - b.E.x ) / eMax, std::fabs( a.E.y - b.E.y ) / eMax, std::fabs( a.E.z - b.E.z ) / eMax } );
			worstV = std::max( worstV, double( std::fabs( a.vis - b.vis ) ) );
			worstW = std::max( worstW, std::fabs( a.w - b.w ) / std::max( 1e-6, double( a.w ) ) );
			worstF = std::max( worstF, double( std::fabs( a.fade - b.fade ) ) );
		}
		outside += glsl::cellDdgiAtlas.outside;
	}
	const bool same = worstE < 1e-3 && worstV < 1e-4 && worstW < 1e-3 && worstF < 1e-5 && outside == 0 && noProbe < points / 4;

	// (2) the leak the read exists to stop: a lit layer of probes behind a wall the distance tiles see 10 units away
	// (var 0). The default read drops them (Chebyshev floor, then the crush); the trilinear red takes them in.
	TestVolume L;
	const int nL = 8;
	L.v.origin = V3( 0, 0, 0 );
	L.v.step = V3( 100, 100, 100 );
	L.v.dims[0] = L.v.dims[1] = L.v.dims[2] = 2;
	L.v.D = 100.0f;
	L.irr.assign( size_t( nL ) * 256, 0.0f );
	L.dist.assign( size_t( nL ) * 512, 0.0f );
	L.probes.assign( size_t( nL ) * 4, 0.0f );
	L.age.assign( size_t( nL ), 1u );
	for ( int p = 0; p < nL; p++ ) {
		const bool up = p >= 4;   // z = 100: the far side of a slab at z ~ 90
		for ( int t = 0; t < 64; t++ )
			for ( int c = 0; c < 3; c++ )
				L.irr[( size_t( p ) * 64 + size_t( t ) ) * 4 + size_t( c )] = up ? 0.9f : 0.3f;
		for ( int t = 0; t < 256; t++ ) {
			const float mean = up ? 10.0f : 1000.0f;
			L.dist[( size_t( p ) * 256 + size_t( t ) ) * 2] = mean;
			L.dist[( size_t( p ) * 256 + size_t( t ) ) * 2 + 1] = mean * mean;
		}
		L.probes[size_t( p ) * 4 + 3] = float( kVigilant );
	}
	L.pack( nL );
	const V3 P( 50, 50, 40 ), n( 0, 0, 1 ), o = normalize( V3( 0.3f, 0.2f, 1.0f ) );
	const SampleResult g = sampleVolume( L.v, P, n, o, n, 0u ), r = sampleVolume( L.v, P, n, o, n, kSampleTrilinear );
	const double lg = g.E.x, lr = r.E.x, dark = 2.0 * kPi * std::pow( 0.3, 5.0 );
	// worked by hand: the wall side ~0.021 (the dark layer's 0.0153 plus the crushed lit one), the red ~1.5
	const bool leak = lg < 2.0 * dark && lr > 10.0 * lg;
	report( "sample", same && leak, "GLSL read vs header on %g points: E worst rel %.2g (bar 1e-3), vis worst %.2g (bar 1e-4)",
		points, worstE, worstV );
	std::printf( "      w worst rel %.3g, fade %.3g, reads outside the atlas %d, points with no probe %d; leak: wall side E %.5f "
		"(bar 2 x the dark side's %.5f), trilinear red %.5f (bar 10 x)\n", worstW, worstF, outside, noProbe, lg, dark, lr );
}

} // namespace

int main()
{
	checkOct();
	checkFib();
	checkHyst();
	checkGamma();
	checkCheb();
	checkBias();
	checkBorder();
	checkSky();
	checkNorm();
	checkClass();
	checkReloc();
	checkTie();
	checkSkyVis();
	checkSample();
	std::printf( "RTDDGI_MATH %s %d%s\n", g_fail ? "FAIL" : "PASS", g_fail ? g_fail : g_pass, g_fail ? " failed" : "" );
	return g_fail ? 1 : 0;
}
