// Lane PCMB1 part B: unit test of src/features/near/cellshadowocc.h (the sun casters' receiver cull).
// Known answers, then 200000 random spheres against a brute-force sweep: the helper may keep a caster that cannot
// reach (it never does here, but that is allowed), it must NEVER drop one whose swept sphere enters the slab.
// Reds (must FAIL): -DSHADOWOCC_RED_SIGN (the shadow direction flipped), -DSHADOWOCC_RED_NORADIUS (radius ignored).
#include "features/near/cellshadowocc.h"

#include <cmath>
#include <cstdio>
#include <random>

using namespace WwShadowOcc;

static bool helper( const float c[3], float r, const float d[3], float n, float f )
{
	float dd[3] = { d[0], d[1], d[2] };
#ifdef SHADOWOCC_RED_SIGN
	for ( float & v : dd )
		v = -v;
#endif
#ifdef SHADOWOCC_RED_NORADIUS
	r = 0.0f;
#endif
	return reachesSlab( c, r, dd, n, f );
}

// brute force: the sphere's depth extremes along the sweep, t in [0, 4000] (every slab and centre here lies in 2000)
static bool brute( const float c[3], float r, const float d[3], float n, float f )
{
	for ( int k = 0; k <= 40000; k++ ) {
		const float t = 0.1f * float( k );
		const float z = -( c[2] + t * d[2] );
		if ( z + r >= n && z - r <= f )
			return true;
	}
	return false;
}

int main()
{
	int bad = 0;
	auto expect = [&]( const char * what, bool got, bool want ) {
		if ( got != want ) {
			bad++;
			std::printf( "FAIL %s: got %d want %d\n", what, int( got ), int( want ) );
		}
	};
	const float away[3] = { 0.0f, -0.6f, -0.8f };	// the shadow runs deeper (sun behind the camera)
	const float toward[3] = { 0.0f, -0.6f, 0.8f };	// the shadow runs toward the camera (sun in front)
	const float level[3] = { 1.0f, 0.0f, 0.0f };
	const float far1[3] = { 0.0f, 0.0f, -900.0f }, near1[3] = { 0.0f, 0.0f, -50.0f }, in1[3] = { 0.0f, 0.0f, -300.0f };
	// slab [100, 500]
	expect( "beyond, shadow away: dropped", helper( far1, 10.0f, away, 100.0f, 500.0f ), false );
	expect( "beyond, shadow toward: kept", helper( far1, 10.0f, toward, 100.0f, 500.0f ), true );
	expect( "beyond, level: dropped", helper( far1, 10.0f, level, 100.0f, 500.0f ), false );
	expect( "beyond but its radius reaches: kept", helper( far1, 450.0f, away, 100.0f, 500.0f ), true );
	expect( "in front, shadow toward: dropped", helper( near1, 10.0f, toward, 100.0f, 500.0f ), false );
	expect( "in front, shadow away: kept", helper( near1, 10.0f, away, 100.0f, 500.0f ), true );
	expect( "in front but its radius reaches: kept", helper( near1, 60.0f, toward, 100.0f, 500.0f ), true );
	expect( "inside: kept", helper( in1, 1.0f, toward, 100.0f, 500.0f ), true );
	expect( "cascade 0 (no near side), behind the camera: kept",
		helper( far1, 1.0f, toward, -3.0e38f, 500.0f ), true );
	{	// shadowDir: a point moved along the direction gets a greater light depth
		float m[16] = { 0 };
		const float L[3] = { 0.3f, -0.5f, 0.8f };
		m[2] = L[0] * 0.01f;
		m[6] = L[1] * 0.01f;
		m[10] = L[2] * 0.01f;
		m[14] = -1.0f;
		float d[3];
		shadowDir( m, d );
		const float p[3] = { 5.0f, 7.0f, -40.0f };
		const float z0 = m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14];
		const float z1 = m[2] * ( p[0] + d[0] ) + m[6] * ( p[1] + d[1] ) + m[10] * ( p[2] + d[2] ) + m[14];
		expect( "shadowDir deepens the light depth", z1 > z0, true );
	}
	std::mt19937 rng( 1106 );
	std::uniform_real_distribution<float> u( -1.0f, 1.0f ), u01( 0.0f, 1.0f );
	long unsound = 0, dropped = 0, reach = 0;
	const long N = 200000;
	for ( long i = 0; i < N; i++ ) {
		const float c[3] = { 500.0f * u( rng ), 500.0f * u( rng ), -1000.0f * u01( rng ) + 200.0f * u( rng ) };
		const float r = 80.0f * u01( rng ) * u01( rng );
		float d[3] = { u( rng ), u( rng ), u( rng ) };
		const float l = std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] ) + 1e-6f;
		for ( float & v : d )
			v /= l;
		const float n = ( i % 3 == 0 ) ? -3.0e38f : 900.0f * u01( rng );
		const float f = ( n < 0.0f ? 0.0f : n ) + 600.0f * u01( rng ) + 1.0f;
		const bool h = helper( c, r, d, n, f ), b = brute( c, r, d, n, f );
		reach += b ? 1 : 0;
		if ( !h ) {
			dropped++;
			if ( b && ++unsound <= 5 )
				std::printf( "UNSOUND c (%.1f %.1f %.1f) r %.1f d (%.3f %.3f %.3f) slab [%g, %g]\n", c[0], c[1], c[2], r, d[0], d[1],
					d[2], n, f );
		}
	}
	std::printf( "random %ld: reach %ld, dropped %ld, unsound drops %ld\n", N, reach, dropped, unsound );
	if ( dropped == 0 ) {
		bad++;
		std::printf( "FAIL nothing dropped: the cull is inert\n" );
	}
	bad += int( unsound );
	std::printf( "%s (%d failures)\n", bad ? "SHADOWOCC FAIL" : "SHADOWOCC PASS", bad );
	return bad ? 1 : 0;
}
