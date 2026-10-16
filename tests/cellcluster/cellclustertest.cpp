// LIGHTSYS2 unit test of the cluster builder (src/gl/cellcluster.*), no GL and no game data.
// A synthetic camera and depth buffer, random omni / spot / hemisphere lights at 16, 64, 256 and 1024 lights.
// Per pixel: every light that reaches the pixel must be in its cluster's list (or the cluster walks every
// light), and the lighting summed over the list must equal the flat loop's bit for bit.
// `--red` builds with the slice-middle depth range: lights near a slice edge get dropped, and it must FAIL.

#include "cellcluster.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

namespace {

constexpr int W = 1280, H = 720;
constexpr double kNear = 10.0, kFar = 60000.0, kFovY = 70.0;

struct Light
{
	WwClusterLight v;
	float color[3];
	float pos[3], dir[3], radius, cosOuter;
	int shape;
};

void perspective( float m[16] )
{
	// Matrix4::fromFrustum, column-major m[col * 4 + row]
	const double h2 = std::tan( kFovY / 360.0 * M_PI ) * kNear, w2 = h2 * W / H;
	std::memset( m, 0, 16 * sizeof( float ) );
	m[0] = float( kNear / w2 );
	m[5] = float( kNear / h2 );
	m[10] = float( ( kNear + kFar ) / ( kNear - kFar ) );
	m[11] = -1.0f;
	m[14] = float( 2.0 * kNear * kFar / ( kNear - kFar ) );
}

// The scene: a ground plane, a wall stepping in depth, and boxes, as view-space depth per pixel.
float depthAt( int px, int py, float & nx, float & ny, float & nz )
{
	const double u = ( px + 0.5 ) / W, v = ( py + 0.5 ) / H;
	nx = 0.0f, ny = 0.0f, nz = 1.0f;
	double d = 200.0 + 30000.0 * std::pow( u, 3.0 );    // a far wall receding to the right
	if ( v < 0.45 ) {                                   // the ground: depth grows toward the horizon
		const double g = 40.0 / std::max( 0.45 - v, 1e-3 );
		if ( g < d ) {
			d = g;
			nx = 0.0f, ny = 1.0f, nz = 0.0f;
		}
	}
	if ( ( int( u * 12 ) + int( v * 7 ) ) % 3 == 0 ) {  // boxes in front, their sides facing x
		d = std::min( d, 150.0 + 4000.0 * u * v );
		nx = ( int( u * 24 ) & 1 ) ? 1.0f : -1.0f, ny = 0.0f, nz = 0.0f;
	}
	return float( d );
}

void viewPoint( const float proj[16], int px, int py, float d, float P[3] )
{
	const double nx = 2.0 * ( px + 0.5 ) / W - 1.0, ny = 2.0 * ( py + 0.5 ) / H - 1.0;
	P[0] = float( nx * d / proj[0] );
	P[1] = float( ny * d / proj[5] );
	P[2] = -d;
}

std::vector<Light> makeLights( int n, unsigned seed, const float proj[16] )
{
	std::mt19937 rng( seed );
	std::uniform_real_distribution<double> U( 0.0, 1.0 );
	std::vector<Light> out;
	for ( int i = 0; i < n; i++ ) {
		Light l{};
		// on a random pixel's surface, pulled toward the camera: lights sit where they light something
		const int px = int( U( rng ) * W ), py = int( U( rng ) * H );
		float nx, ny, nz, P[3];
		viewPoint( proj, px, py, depthAt( px, py, nx, ny, nz ) * float( 0.5 + 0.5 * U( rng ) ), P );
		for ( int k = 0; k < 3; k++ )
			l.pos[k] = P[k];
		l.radius = float( 64.0 * std::pow( 40.0, U( rng ) ) );   // 64 .. 2560
		l.shape = int( U( rng ) * 3.0 );
		double dx = U( rng ) - 0.5, dy = U( rng ) - 0.5, dz = U( rng ) - 0.5, len = std::sqrt( dx * dx + dy * dy + dz * dz );
		l.dir[0] = float( dx / len ), l.dir[1] = float( dy / len ), l.dir[2] = float( dz / len );
		l.cosOuter = l.shape == 1 ? float( std::cos( ( 10.0 + 70.0 * U( rng ) ) * M_PI / 180.0 ) ) : -2.0f;
		for ( int k = 0; k < 3; k++ )
			l.color[k] = float( 0.2 + U( rng ) );
		for ( int k = 0; k < 3; k++ ) {
			l.v.pos[k] = l.pos[k];
			l.v.dir[k] = l.dir[k];
		}
		l.v.radius = l.radius;
		l.v.shape = l.shape;
		l.v.cosOuter = l.cosOuter;
		out.push_back( l );
	}
	return out;
}

// One light at view point P, normal N, in float as the shader does it: 0 when out of reach (the culled case).
bool reaches( const Light & l, const float P[3] )
{
	const float Lv[3] = { l.pos[0] - P[0], l.pos[1] - P[1], l.pos[2] - P[2] };
	const float d = std::sqrt( Lv[0] * Lv[0] + Lv[1] * Lv[1] + Lv[2] * Lv[2] );
	if ( d >= l.radius )
		return false;
	if ( l.shape == 2 )
		return -( Lv[0] * l.dir[0] + Lv[1] * l.dir[1] + Lv[2] * l.dir[2] ) >= 0.0f;
	if ( l.shape == 1 ) {
		const float c = -( Lv[0] * l.dir[0] + Lv[1] * l.dir[1] + Lv[2] * l.dir[2] ) / std::max( d, 0.001f );
		return 1.0f - ( 1.0f - c ) / std::max( 1.0f - l.cosOuter, 1e-4f ) > 0.0f;
	}
	return true;
}

void shade( const Light & l, const float P[3], const float N[3], float E[3] )
{
	if ( !reaches( l, P ) )
		return;
	const float Lv[3] = { l.pos[0] - P[0], l.pos[1] - P[1], l.pos[2] - P[2] };
	const float d = std::sqrt( Lv[0] * Lv[0] + Lv[1] * Lv[1] + Lv[2] * Lv[2] );
	const float L[3] = { Lv[0] / std::max( d, 0.001f ), Lv[1] / std::max( d, 0.001f ), Lv[2] / std::max( d, 0.001f ) };
	const float ndl = N[0] * L[0] + N[1] * L[1] + N[2] * L[2];
	if ( ndl <= 0.0f )
		return;
	float a = std::pow( std::max( 1.0f - d / l.radius, 0.0f ), 2.2f );
	if ( l.shape == 1 ) {
		const float c = -( L[0] * l.dir[0] + L[1] * l.dir[1] + L[2] * l.dir[2] );
		a *= std::min( std::pow( std::clamp( 1.0f - ( 1.0f - c ) / std::max( 1.0f - l.cosOuter, 1e-4f ), 0.0f, 1.0f ), 2.0f ), 1.0f );
	}
	for ( int k = 0; k < 3; k++ )
		E[k] += l.color[k] * a * ndl;
}

struct Result
{
	long misses = 0, unequal = 0, overflowPixels = 0, walked = 0;
	double buildMs = 0.0;
	int refs = 0, overflowed = 0, maxCount = 0;
};

Result runOne( int n, bool red )
{
	float proj[16];
	perspective( proj );
	const int vp[4] = { 0, 0, W, H };
	const std::vector<Light> lights = makeLights( n, 1234u + unsigned( n ), proj );
	std::vector<WwClusterLight> cl;
	for ( const Light & l : lights )
		cl.push_back( l.v );
	WwClusterParams prm;
	prm.redMidSlice = red;
	WwClusterGrid g;
	Result r;
	if ( !wwClusterBuild( proj, vp, cl, prm, g ) ) {
		r.misses = -1;
		return r;
	}
	r.buildMs = g.buildMs, r.refs = g.refs, r.overflowed = g.overflowed, r.maxCount = g.maxCount;
	for ( int py = 0; py < H; py++ )
		for ( int px = 0; px < W; px++ ) {
			float N[3], P[3];
			const float d = depthAt( px, py, N[0], N[1], N[2] );
			viewPoint( proj, px, py, d, P );
			const int c = g.clusterOf( px + 0.5f, py + 0.5f, -P[2] );
			const int off = g.cells[size_t( c ) * 2], cnt = g.cells[size_t( c ) * 2 + 1];
			float flat[3] = {}, clus[3] = {};
			for ( size_t i = 0; i < lights.size(); i++ )
				shade( lights[i], P, N, flat );
			if ( cnt < 0 ) {
				r.overflowPixels++;
				continue;   // the shader walks every light here: the flat loop itself
			}
			r.walked += cnt;
			std::vector<char> listed( lights.size(), 0 );
			for ( int k = 0; k < cnt; k++ ) {
				listed[size_t( g.index[size_t( off + k )] )] = 1;
				shade( lights[size_t( g.index[size_t( off + k )] )], P, N, clus );
			}
			for ( size_t i = 0; i < lights.size(); i++ )
				if ( !listed[i] && reaches( lights[i], P ) )
					r.misses++;
			if ( std::memcmp( flat, clus, sizeof( flat ) ) != 0 )
				r.unequal++;
		}
	return r;
}

} // namespace

int main( int argc, char ** argv )
{
	const bool red = argc > 1 && std::strcmp( argv[1], "--red" ) == 0;
	std::printf( "cluster builder, %dx%d, %s\n", W, H, red ? "RED slice-middle depth range" : "green" );
	bool ok = true;
	for ( int n : { 16, 64, 256, 1024 } ) {
		const Result r = runOne( n, red );
		const bool pass = r.misses == 0 && r.unequal == 0;
		ok &= pass;
		std::printf( "  %4d lights: missed %ld, sums unequal %ld px, overflow px %ld, mean walked %.1f, refs %d, "
			"clusters over cap %d, max list %d, build %.3f ms  %s\n", n, r.misses, r.unequal, r.overflowPixels,
			double( r.walked ) / double( W * H ), r.refs, r.overflowed, r.maxCount, r.buildMs, pass ? "ok" : "FAIL" );
	}
	std::printf( "%s\n", ok ? "PASS" : "FAIL" );
	return ok ? 0 : 1;
}
