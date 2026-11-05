// lane INTERIORPERF1: the volumetric particles' light loop walks a cluster list (res/shaders/features/air/cell_volpart.glsl
// vpLightsBegin) instead of every light. A particle lights at its sprite's CENTRE, so the list must be the cluster
// that holds that point (its own projected pixel), not the cluster of the fragment being shaded. Pure C++, no Qt, no GL:
// the lists from the real build (src/features/lights/cellcluster.cpp), the shader's pick mirrored in float as WwClusterGrid::clusterOf.
//   1. every light that can light the point (inside its radius, inside its cone / half space) is in the picked list, or
//      the pick says "every light": the walk sums the same lights, in the same ascending order, as the flat loop
//   2. the lights walked per point (the loop's cost) on an interior-like set of 308 lights (the Museum's count)
//   3. control: the fragment's own pixel (gl_FragCoord, the surfaces' pick) misses lights for points off its tile
// Build: g++ -std=c++17 -O2 -I<tree>/src tests/features/air/volpartcluster/volpartcluster.cpp src/features/lights/cellcluster.cpp
// Red:   -DVOLPART_CLUSTER_RED picks by the fragment's pixel (check 1 must FAIL).
#define _USE_MATH_DEFINES
#include "features/lights/cellcluster.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace
{
int fails = 0, checks = 0;
void check( bool ok, const std::string & what )
{
	checks++;
	fails += ok ? 0 : 1;
	std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what.c_str() );
}

void perspective( float m[16], int W, int H, double fovY )
{
	const double kNear = 10.0, kFar = 60000.0;
	const double h2 = std::tan( fovY / 360.0 * M_PI ) * kNear, w2 = h2 * W / H;
	std::memset( m, 0, 16 * sizeof( float ) );
	m[0] = float( kNear / w2 );
	m[5] = float( kNear / h2 );
	m[10] = float( ( kNear + kFar ) / ( kNear - kFar ) );
	m[11] = -1.0f;
	m[14] = float( 2.0 * kNear * kFar / ( kNear - kFar ) );
}

// can light l give anything at view point P (cell_lights.glsl cellLightE: 0 at d >= radius, outside the cone, behind the plane)
bool lights( const WwClusterLight & l, const double P[3] )
{
	const double v[3] = { P[0] - l.pos[0], P[1] - l.pos[1], P[2] - l.pos[2] };
	const double d = std::sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
	if ( d >= l.radius || d < 0.001 )
		return false;
	const double c = ( v[0] * l.dir[0] + v[1] * l.dir[1] + v[2] * l.dir[2] ) / d;
	if ( l.shape == 1 && c <= l.cosOuter )
		return false;
	if ( l.shape == 2 && c < 0.0 )
		return false;
	return true;
}

struct Set
{
	std::string name;
	float proj[16];
	int vp[4];
	std::vector<WwClusterLight> lights;
};

// an interior: n lights within `reach` of the eye, small radii, a third each omni / spot / hemisphere
Set interior( int n, unsigned seed, int W, int H, double reach )
{
	Set s;
	s.name = std::to_string( n ) + " lights within " + std::to_string( int( reach ) ) + " u, " + std::to_string( W ) + "x"
		+ std::to_string( H );
	perspective( s.proj, W, H, 70.0 );
	s.proj[8] = 0.37f / float( W );	// a TAA jitter
	s.proj[9] = -0.21f / float( H );
	s.vp[0] = 0, s.vp[1] = 0, s.vp[2] = W, s.vp[3] = H;
	std::mt19937 rng( seed );
	std::uniform_real_distribution<double> U( 0.0, 1.0 );
	for ( int i = 0; i < n; i++ ) {
		WwClusterLight l;
		const double d = 30.0 + reach * U( rng ), a = ( U( rng ) - 0.5 ) * M_PI * 2.0;
		l.pos[0] = d * std::sin( a ), l.pos[1] = ( U( rng ) - 0.5 ) * 600.0, l.pos[2] = -d * std::cos( a );
		l.radius = 96.0 * std::pow( 12.0, U( rng ) );
		l.shape = int( U( rng ) * 3.0 );
		double dx = U( rng ) - 0.5, dy = U( rng ) - 0.5, dz = U( rng ) - 0.5, len = std::sqrt( dx * dx + dy * dy + dz * dz );
		l.dir[0] = dx / len, l.dir[1] = dy / len, l.dir[2] = dz / len;
		l.cosOuter = l.shape == 1 ? std::cos( ( 10.0 + 70.0 * U( rng ) ) * M_PI / 180.0 ) : -1.0;
		s.lights.push_back( l );
	}
	return s;
}

// the shader's pick (cell_volpart.glsl vpLightsBegin), in float: P's own pixel through the projection; -1 = every light
int pickAt( const WwClusterGrid & g, const float m[16], const int vp[4], const double P[3], float fragX, float fragY )
{
	const float x = float( P[0] ), y = float( P[1] ), z = float( P[2] );
	const float cx = m[0] * x + m[4] * y + m[8] * z + m[12], cy = m[1] * x + m[5] * y + m[9] * z + m[13];
	const float cz = m[2] * x + m[6] * y + m[10] * z + m[14], cw = m[3] * x + m[7] * y + m[11] * z + m[15];
	if ( cw <= 0.0f || std::fabs( cx ) > cw || std::fabs( cy ) > cw || std::fabs( cz ) > cw )
		return -1;
	float px = float( vp[0] ) + ( cx / cw * 0.5f + 0.5f ) * float( vp[2] ), py = float( vp[1] ) + ( cy / cw * 0.5f + 0.5f ) * float( vp[3] );
#ifdef VOLPART_CLUSTER_RED
	px = fragX, py = fragY;	// the red: the fragment's own pixel (cellLightsBegin's gl_FragCoord)
#else
	(void)fragX, (void)fragY;
#endif
	return g.clusterOf( px, py, -z );
}
}	// namespace

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	std::vector<Set> sets = { interior( 308, 7u, 1920, 1026, 4000.0 ), interior( 308, 19u, 1246, 991, 2500.0 ),
		interior( 64, 3u, 1920, 1080, 3000.0 ), interior( 1024, 5u, 1920, 1026, 6000.0 ) };
	for ( const Set & s : sets ) {
		WwClusterParams prm;
		prm.jitterPadPx = 1.0;
		WwClusterGrid g;
		if ( !wwClusterBuild( s.proj, s.vp, s.lights, prm, g ) ) {
			check( false, s.name + ": build" );
			continue;
		}
		std::mt19937 rng( 1234u );
		std::uniform_real_distribution<double> U( 0.0, 1.0 );
		long missing = 0, pointsLit = 0, flat = 0, walked = 0;
		const int kPoints = 200000;
		for ( int k = 0; k < kPoints; k++ ) {
			// a sprite centre: anywhere in the frustum (and a bit past it, for the fallback), depth log-uniform 20..8000
			const double nx = ( U( rng ) * 2.0 - 1.0 ) * 1.1, ny = ( U( rng ) * 2.0 - 1.0 ) * 1.1;
			const double depth = 20.0 * std::pow( 400.0, U( rng ) );
			const double P[3] = { nx * depth / s.proj[0], ny * depth / s.proj[5], -depth };
			// the fragment being shaded: up to a 300 px sprite's half size away from the centre's pixel
			const double cxp = ( nx * 0.5 + 0.5 ) * s.vp[2], cyp = ( ny * 0.5 + 0.5 ) * s.vp[3];
			const float fx = float( std::clamp( cxp + ( U( rng ) - 0.5 ) * 300.0, 0.5, s.vp[2] - 0.5 ) );
			const float fy = float( std::clamp( cyp + ( U( rng ) - 0.5 ) * 300.0, 0.5, s.vp[3] - 0.5 ) );
			const int cl = pickAt( g, s.proj, s.vp, P, fx, fy );
			const int cnt = cl < 0 ? -1 : g.cells[size_t( 2 * cl + 1 )];
			if ( cnt < 0 ) {
				flat++;
				walked += long( s.lights.size() );
				continue;
			}
			walked += cnt;
			const std::int32_t * lst = g.index.data() + g.cells[size_t( 2 * cl )];
			for ( int i = 0; i < int( s.lights.size() ); i++ ) {
				if ( !lights( s.lights[size_t( i )], P ) )
					continue;
				pointsLit++;
				if ( !std::binary_search( lst, lst + cnt, i ) )
					missing++;
			}
		}
		char buf[320];
		std::snprintf( buf, sizeof buf, ": %d points, %ld light hits, %ld missing from the picked list, %ld points walk every light",
			kPoints, pointsLit, missing, flat );
		check( missing == 0 && pointsLit > 0, s.name + buf );
		std::snprintf( buf, sizeof buf, ": lights walked per point %.2f (flat loop %zu), %.1fx fewer", double( walked ) / kPoints,
			s.lights.size(), double( s.lights.size() ) * kPoints / double( std::max( walked, 1L ) ) );
		check( walked < long( s.lights.size() ) * kPoints / 4, s.name + buf );
	}
	std::printf( "%d checks, %d failures\n", checks, fails );
	return fails ? 1 : 0;
}
