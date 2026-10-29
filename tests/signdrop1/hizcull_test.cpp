/* lane SIGNDROP1 (2026-10-29): a CPU twin of the GPU cull's occlusion test (src/gl/cellgpucull.cpp step B) on a
 * synthetic frame, with no GL: the depth prepass (only the draws gl/cellgpucullgate.h admits, written depth-only),
 * kHiz0 (MS: the farthest sample), kHizN (2x2 max, ceil sizes, clamped), the level table of prepare(), and kCull's
 * screen box, level pick and depth compare. The frame is 333 x 187 (odd at every level), 4 samples, a TAA-jittered
 * projection; window y grows up from the bottom row as GL's does.
 *
 * The scene (view space, the camera looks down -z):
 *   backwall   z -10, the room's far wall                                   solid occluder
 *   nearwall   z -3, a broken wall whose hole is cut by its BGSM's alpha    queued Opaque, the draw alpha-tests
 *   plank      z -2, lower left                                            solid occluder
 *   sign       z -7, seen through the hole, in front of the back wall      occluder and candidate: must be KEPT
 *   hidden     z -6, wholly behind the plank                               candidate: must be CULLED (the test culls)
 *   mirror     z -6, the hidden one mirrored in y (nothing in front)       candidate: must be kept (y orientation)
 *   behindwall z -8, behind the near wall's solid part                     candidate: the cull the fix gives up (info)
 *   edge       z -6, half off the right edge                               candidate: must be kept (box clamp)
 *
 * Checks: the pyramid represents every level-0 pixel at every level by its texel (x >> L, y >> L) with a value no
 * nearer; every 1x1 box and 20000 random boxes read no nearer than the brute-force max; no cluster the frame shows
 * is culled. Controls (each must be caught, or the check proves nothing): floor-halved level sizes, level 0 = the
 * NEAREST sample, a y-flipped box.
 *
 * Green: SIGNDROP1 PASS, exit 0. Red (-DSIGNDROP1_RED: the gate before the fix, WW_GPUDRIVEN_B_RED=atocc):
 * SIGNDROP1 FAIL (the sign is visible and culled), exit 1. */

#include "../../src/gl/cellgpucullgate.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#ifdef SIGNDROP1_RED
constexpr bool kRed = true;
#else
constexpr bool kRed = false;
#endif

namespace
{

struct V3
{
	float x, y, z;
};
struct V4
{
	float x, y, z, w;
};
// column-major, as the GL uniform and FloatVector4 pm[column][row]
struct M4
{
	float m[16];
};

V4 mul( const M4 & a, const V4 & v )
{
	return { a.m[0] * v.x + a.m[4] * v.y + a.m[8] * v.z + a.m[12] * v.w,
		a.m[1] * v.x + a.m[5] * v.y + a.m[9] * v.z + a.m[13] * v.w,
		a.m[2] * v.x + a.m[6] * v.y + a.m[10] * v.z + a.m[14] * v.w,
		a.m[3] * v.x + a.m[7] * v.y + a.m[11] * v.z + a.m[15] * v.w };
}

// a GL perspective; the TAA jitter rides in m[8], m[9] (cellclustergl.cpp jitterNear reads the same two)
M4 perspective( float tanHalfY, float aspect, float n, float f, float jx, float jy )
{
	M4 p = {};
	p.m[0] = 1.0f / ( tanHalfY * aspect );
	p.m[5] = 1.0f / tanHalfY;
	p.m[8] = jx;
	p.m[9] = jy;
	p.m[10] = ( f + n ) / ( n - f );
	p.m[11] = -1.0f;
	p.m[14] = 2.0f * f * n / ( n - f );
	return p;
}

// cellgpucull.cpp standardProjection
bool standardProjection( const M4 & p )
{
	return p.m[3] == 0.0f && p.m[7] == 0.0f && p.m[11] == -1.0f && p.m[15] == 0.0f && p.m[2] == 0.0f && p.m[6] == 0.0f;
}

constexpr int kW = 333, kH = 187, kS = 4;
// the standard 4x pattern, in pixels from the pixel's lower-left corner
const float kSamp[4][2] = { { 0.375f, 0.125f }, { 0.875f, 0.375f }, { 0.125f, 0.625f }, { 0.625f, 0.875f } };
// glPolygonOffset( 1, 2 ) on a face-on quad: 2 steps of a 24-bit depth (the prepass and the frame both have it)
constexpr float kOffset = 2.0f / 16777216.0f;
constexpr float kDepthEps = 4e-6f;	// prepare()'s depthEps

struct Target
{
	std::vector<float> depth;	// [sample][y][x], y = 0 the bottom row
	std::vector<int> id;
	void clear()
	{
		depth.assign( std::size_t( kS ) * kW * kH, 1.0f );
		id.assign( depth.size(), -1 );
	}
	std::size_t at( int s, int x, int y ) const
	{
		return ( std::size_t( s ) * kH + std::size_t( y ) ) * kW + std::size_t( x );
	}
};

struct Quad
{
	const char * name;
	V3 c;
	float hx, hy;
	bool hole;	// |x - c.x| < holeX && |y - c.y| < holeY has alpha 0 (the BGSM's cut)
	float holeX, holeY;
	GpuDrivenBGate::Seen seen;
	bool candidate;
	int expect;	// 0 none, 1 must be kept and visible, 2 must be culled and hidden, 3 info (a cull the fix gives up)
};

// one triangle, facing either way; alphaQ != nullptr runs that quad's alpha test (the frame), else depth-only
void tri( Target & t, const M4 & P, const V3 & a, const V3 & b, const V3 & c, int idv, const Quad * alphaQ, float offset )
{
	const V3 v[3] = { a, b, c };
	float X[3], Y[3], Z[3], iw[3];
	for ( int i = 0; i < 3; i++ ) {
		const V4 q = mul( P, { v[i].x, v[i].y, v[i].z, 1.0f } );
		if ( q.w <= 0.0f || q.z < -q.w )
			return;	// the scene keeps everything in front of the near plane
		iw[i] = 1.0f / q.w;
		X[i] = ( q.x * iw[i] * 0.5f + 0.5f ) * float( kW );
		Y[i] = ( q.y * iw[i] * 0.5f + 0.5f ) * float( kH );
		Z[i] = q.z * iw[i] * 0.5f + 0.5f;
	}
	const float area = ( X[1] - X[0] ) * ( Y[2] - Y[0] ) - ( Y[1] - Y[0] ) * ( X[2] - X[0] );
	if ( area == 0.0f )
		return;
	const int x0 = std::max( 0, int( std::floor( std::min( { X[0], X[1], X[2] } ) ) ) );
	const int x1 = std::min( kW - 1, int( std::ceil( std::max( { X[0], X[1], X[2] } ) ) ) );
	const int y0 = std::max( 0, int( std::floor( std::min( { Y[0], Y[1], Y[2] } ) ) ) );
	const int y1 = std::min( kH - 1, int( std::ceil( std::max( { Y[0], Y[1], Y[2] } ) ) ) );
	for ( int py = y0; py <= y1; py++ ) {
		for ( int px = x0; px <= x1; px++ ) {
			for ( int s = 0; s < kS; s++ ) {
				const float sx = float( px ) + kSamp[s][0], sy = float( py ) + kSamp[s][1];
				const float e0 = ( ( X[2] - X[1] ) * ( sy - Y[1] ) - ( Y[2] - Y[1] ) * ( sx - X[1] ) ) / area;
				const float e1 = ( ( X[0] - X[2] ) * ( sy - Y[2] ) - ( Y[0] - Y[2] ) * ( sx - X[2] ) ) / area;
				const float e2 = ( ( X[1] - X[0] ) * ( sy - Y[0] ) - ( Y[1] - Y[0] ) * ( sx - X[0] ) ) / area;
				if ( e0 < 0.0f || e1 < 0.0f || e2 < 0.0f )
					continue;
				if ( alphaQ && alphaQ->hole ) {
					// perspective-correct view position of the sample
					const float w = e0 * iw[0] + e1 * iw[1] + e2 * iw[2];
					const float vx = ( e0 * v[0].x * iw[0] + e1 * v[1].x * iw[1] + e2 * v[2].x * iw[2] ) / w;
					const float vy = ( e0 * v[0].y * iw[0] + e1 * v[1].y * iw[1] + e2 * v[2].y * iw[2] ) / w;
					if ( std::fabs( vx - alphaQ->c.x ) < alphaQ->holeX && std::fabs( vy - alphaQ->c.y ) < alphaQ->holeY )
						continue;	// discard
				}
				const float z = e0 * Z[0] + e1 * Z[1] + e2 * Z[2] + offset;
				const std::size_t k = t.at( s, px, py );
				if ( z <= t.depth[k] ) {	// GL_LEQUAL
					t.depth[k] = z;
					t.id[k] = idv;
				}
			}
		}
	}
}

void quad( Target & t, const M4 & P, const Quad & q, int idv, bool alphaTest, float offset )
{
	const V3 v[4] = { { q.c.x - q.hx, q.c.y - q.hy, q.c.z }, { q.c.x + q.hx, q.c.y - q.hy, q.c.z },
		{ q.c.x + q.hx, q.c.y + q.hy, q.c.z }, { q.c.x - q.hx, q.c.y + q.hy, q.c.z } };
	tri( t, P, v[0], v[1], v[2], idv, alphaTest ? &q : nullptr, offset );
	tri( t, P, v[0], v[2], v[3], idv, alphaTest ? &q : nullptr, offset );
}

// ---- the Hi-Z: prepare()'s level table, kHiz0, kHizN

struct Pyr
{
	int levels = 0;
	int off[16] = {};
	int dim[16][2] = {};
	std::vector<float> hiz;
};

Pyr build( const Target & t, bool floorHalve, bool nearestSample )
{
	Pyr p;
	int w = kW, h = kH;
	std::size_t total = 0;
	while ( p.levels < 16 ) {
		p.off[p.levels] = int( total );
		p.dim[p.levels][0] = w;
		p.dim[p.levels][1] = h;
		total += std::size_t( w ) * std::size_t( h );
		p.levels++;
		if ( w == 1 && h == 1 )
			break;
		w = floorHalve ? std::max( 1, w / 2 ) : ( w + 1 ) / 2;
		h = floorHalve ? std::max( 1, h / 2 ) : ( h + 1 ) / 2;
	}
	p.hiz.assign( total, 0.0f );
	for ( int y = 0; y < kH; y++ ) {
		for ( int x = 0; x < kW; x++ ) {
			float m = nearestSample ? 1.0f : 0.0f;
			for ( int s = 0; s < kS; s++ )
				m = nearestSample ? std::min( m, t.depth[t.at( s, x, y )] ) : std::max( m, t.depth[t.at( s, x, y )] );
			p.hiz[std::size_t( y ) * kW + x] = m;
		}
	}
	for ( int l = 1; l < p.levels; l++ ) {
		const int sw = p.dim[l - 1][0], sh = p.dim[l - 1][1], so = p.off[l - 1];
		for ( int y = 0; y < p.dim[l][1]; y++ ) {
			for ( int x = 0; x < p.dim[l][0]; x++ ) {
				const int ax = x * 2, ay = y * 2;
				const int bx = std::min( ax + 1, sw - 1 ), by = std::min( ay + 1, sh - 1 );
				const float m = std::max( std::max( p.hiz[so + ay * sw + ax], p.hiz[so + ay * sw + bx] ),
					std::max( p.hiz[so + by * sw + ax], p.hiz[so + by * sw + bx] ) );
				p.hiz[std::size_t( p.off[l] ) + std::size_t( y ) * p.dim[l][0] + x] = m;
			}
		}
	}
	return p;
}

// the reference level 0: the farthest sample of each pixel
std::vector<float> level0( const Target & t )
{
	std::vector<float> r( std::size_t( kW ) * kH, 0.0f );
	for ( int y = 0; y < kH; y++ )
		for ( int x = 0; x < kW; x++ )
			for ( int s = 0; s < kS; s++ )
				r[std::size_t( y ) * kW + x] = std::max( r[std::size_t( y ) * kW + x], t.depth[t.at( s, x, y )] );
	return r;
}

// every level-0 pixel must have its texel (x >> L, y >> L) at every level, no nearer than itself
long coverageShort( const Pyr & p, const std::vector<float> & ref )
{
	long bad = 0;
	for ( int L = 0; L < p.levels; L++ )
		for ( int y = 0; y < kH; y++ )
			for ( int x = 0; x < kW; x++ ) {
				const int tx = x >> L, ty = y >> L;
				if ( tx >= p.dim[L][0] || ty >= p.dim[L][1]
					|| p.hiz[std::size_t( p.off[L] ) + std::size_t( ty ) * p.dim[L][0] + tx] < ref[std::size_t( y ) * kW + x] )
					bad++;
			}
	return bad;
}

// kCull's level pick and read over the clamped box a..b (level-0 pixels, inclusive)
float boxMax( const Pyr & p, int ax, int ay, int bx, int by )
{
	const int span = std::max( bx - ax, by - ay ) + 1;
	int L = 0;
	while ( L < p.levels - 1 && ( 1 << L ) < span )
		L++;
	const int tax = ax >> L, tay = ay >> L, tbx = bx >> L, tby = by >> L;
	float h = 0.0f;
	for ( int y = tay; y <= tby; y++ )
		for ( int x = tax; x <= tbx; x++ )
			h = std::max( h, p.hiz[std::size_t( p.off[L] ) + std::size_t( y ) * p.dim[L][0] + x] );
	return h;
}

long boxesShort( const Pyr & p, const std::vector<float> & ref, long & tested )
{
	long bad = 0;
	tested = 0;
	auto brute = [&]( int ax, int ay, int bx, int by ) {
		float m = 0.0f;
		for ( int y = ay; y <= by; y++ )
			for ( int x = ax; x <= bx; x++ )
				m = std::max( m, ref[std::size_t( y ) * kW + x] );
		return m;
	};
	for ( int y = 0; y < kH; y++ )
		for ( int x = 0; x < kW; x++ ) {
			tested++;
			bad += boxMax( p, x, y, x, y ) < brute( x, y, x, y ) ? 1 : 0;
		}
	std::mt19937 rng( 1029u );
	for ( int k = 0; k < 20000; k++ ) {
		int ax = int( rng() % kW ), bx = int( rng() % kW ), ay = int( rng() % kH ), by = int( rng() % kH );
		if ( k % 2 ) {	// half of them small: a few pixels each way
			bx = std::min( kW - 1, ax + int( rng() % 9 ) );
			by = std::min( kH - 1, ay + int( rng() % 9 ) );
		}
		if ( ax > bx )
			std::swap( ax, bx );
		if ( ay > by )
			std::swap( ay, by );
		tested++;
		bad += boxMax( p, ax, ay, bx, by ) < brute( ax, ay, bx, by ) ? 1 : 0;
	}
	return bad;
}

// kCull's occlusion branch (mode 1) for a cluster sphere in view space; flipY = the control's wrong row order
bool occluded( const M4 & P, const Pyr & p, const V3 & c, float r, bool flipY )
{
	float lo[2] = { 1e30f, 1e30f }, hi[2] = { -1e30f, -1e30f };
	for ( int k = 0; k < 8; k++ ) {
		const V3 o = { ( k & 1 ) ? r : -r, ( k & 2 ) ? r : -r, ( k & 4 ) ? r : -r };
		const V4 q = mul( P, { c.x + o.x, c.y + o.y, c.z + o.z, 1.0f } );
		if ( q.w <= 0.0f || q.z < -q.w )
			return false;	// not wholly in front: never culled
		lo[0] = std::min( lo[0], q.x / q.w );
		lo[1] = std::min( lo[1], q.y / q.w );
		hi[0] = std::max( hi[0], q.x / q.w );
		hi[1] = std::max( hi[1], q.y / q.w );
	}
	if ( flipY ) {
		const float l = -hi[1], h = -lo[1];
		lo[1] = l;
		hi[1] = h;
	}
	const float p0x = ( lo[0] * 0.5f + 0.5f ) * float( kW ) - 1.0f, p0y = ( lo[1] * 0.5f + 0.5f ) * float( kH ) - 1.0f;
	const float p1x = ( hi[0] * 0.5f + 0.5f ) * float( kW ) + 1.0f, p1y = ( hi[1] * 0.5f + 0.5f ) * float( kH ) + 1.0f;
	if ( !( p1x >= 0.0f && p1y >= 0.0f && p0x < float( kW ) && p0y < float( kH ) ) )
		return false;
	const int ax = std::clamp( int( std::floor( p0x ) ), 0, kW - 1 ), ay = std::clamp( int( std::floor( p0y ) ), 0, kH - 1 );
	const int bx = std::clamp( int( std::floor( p1x ) ), 0, kW - 1 ), by = std::clamp( int( std::floor( p1y ) ), 0, kH - 1 );
	const float h = boxMax( p, ax, ay, bx, by );
	const V4 q = mul( P, { 0.0f, 0.0f, c.z + r, 1.0f } );	// the sphere's nearest depth
	return ( q.z / q.w ) * 0.5f + 0.5f > h + kDepthEps;
}

// cellgpucull.cpp bounds() for a face-on quad, times the job's radius scale (identity model-view: 1.0002)
float radiusOf( const Quad & q )
{
	const float cl = std::sqrt( q.c.x * q.c.x + q.c.y * q.c.y + q.c.z * q.c.z );
	return ( std::sqrt( q.hx * q.hx + q.hy * q.hy ) * 1.0005f + 1e-5f * std::max( 1.0f, cl ) ) * 1.0002f;
}

GpuDrivenBGate::Seen solid()
{
	GpuDrivenBGate::Seen s;
	s.occluderProgram = s.opaque = s.cullOn = s.cullBack = s.ccw = true;
	return s;
}

GpuDrivenBGate::Seen candidateOnly()
{
	GpuDrivenBGate::Seen s;	// a PBR program: a candidate, never an occluder
	return s;
}

}

int main()
{
	const float aspect = float( kW ) / float( kH );
	const M4 P = perspective( 1.2f, aspect, 0.5f, 200.0f, 0.6f / float( kW ), -0.4f / float( kH ) );
	int fails = 0;
	auto check = [&]( bool ok, const std::string & what ) {
		std::printf( "%s %s\n", ok ? "ok  " : "FAIL", what.c_str() );
		fails += ok ? 0 : 1;
	};
	check( standardProjection( P ), "the jittered projection passes standardProjection()" );

	GpuDrivenBGate::Seen wall = solid();
	wall.discards = GpuDrivenBGate::alphaDiscards( 4 );	// renderer.cpp: a BGSM alpha test sets alphaFlags |= 4
	wall.alphaTestQueue = false;	// its NiAlphaProperty does not test: queued Opaque
	std::vector<Quad> quads = {
		{ "backwall", { 0.0f, 0.0f, -10.0f }, 30.0f, 30.0f, false, 0, 0, solid(), false, 0 },
		{ "nearwall", { 0.0f, 0.0f, -3.0f }, 4.0f, 3.0f, true, 1.0f, 0.8f, wall, false, 0 },
		{ "plank", { -2.8f, -1.9f, -2.0f }, 0.8f, 0.5f, false, 0, 0, solid(), false, 0 },
		{ "sign", { 0.0f, 0.0f, -7.0f }, 0.8f, 0.5f, false, 0, 0, solid(), true, 1 },
		{ "hidden", { -8.4f, -5.7f, -6.0f }, 0.05f, 0.05f, false, 0, 0, candidateOnly(), true, 2 },
		{ "mirror", { -8.4f, 5.7f, -6.0f }, 0.05f, 0.05f, false, 0, 0, candidateOnly(), true, 1 },
		{ "behindwall", { 6.667f, 5.067f, -8.0f }, 0.1f, 0.1f, false, 0, 0, candidateOnly(), true, 3 },
		{ "edge", { 12.6f, 0.0f, -6.0f }, 0.6f, 0.6f, false, 0, 0, candidateOnly(), true, 1 },
	};

	// the frame: every draw with its own alpha test
	Target frame;
	frame.clear();
	for ( std::size_t i = 0; i < quads.size(); i++ )
		quad( frame, P, quads[i], int( i ), quads[i].seen.discards, kOffset );
	std::vector<long> vis( quads.size(), 0 );
	for ( int id : frame.id )
		if ( id >= 0 )
			vis[std::size_t( id )]++;

	// the prepass: the gate's occluders, depth-only (no discard)
	Target pre;
	pre.clear();
	int nOcc = 0, nRefused = 0;
	for ( std::size_t i = 0; i < quads.size(); i++ ) {
		if ( GpuDrivenBGate::occluder( quads[i].seen, kRed ) ) {
			quad( pre, P, quads[i], int( i ), false, kOffset );
			nOcc++;
		} else if ( GpuDrivenBGate::occluder( quads[i].seen, true ) ) {
			nRefused++;
		}
	}
	std::printf( "gate %s: occluders %d, refused for their alpha test %d\n", kRed ? "RED atocc (the old gate)" : "new",
		nOcc, nRefused );

	const std::vector<float> ref = level0( pre );
	const Pyr pyr = build( pre, false, false );
	std::printf( "pyramid %dx%d, %d samples, %d levels:", kW, kH, kS, pyr.levels );
	for ( int L = 0; L < pyr.levels; L++ )
		std::printf( " %dx%d", pyr.dim[L][0], pyr.dim[L][1] );
	std::printf( "\n" );
	check( coverageShort( pyr, ref ) == 0, "every level-0 pixel is in its texel at every level, no nearer" );
	long tested = 0;
	const long shortBoxes = boxesShort( pyr, ref, tested );
	check( shortBoxes == 0, "boxes read no nearer than the brute-force max (" + std::to_string( tested ) + " boxes)" );

	// the controls: each must be caught
	const long ctlFloor = coverageShort( build( pre, true, false ), ref );
	check( ctlFloor > 0, "control floor-halved sizes caught (" + std::to_string( ctlFloor ) + " pixels unrepresented)" );
	long t2 = 0;
	const long ctlNear = boxesShort( build( pre, false, true ), ref, t2 );
	check( ctlNear > 0, "control nearest-sample level 0 caught (" + std::to_string( ctlNear ) + " boxes short)" );

	// the clusters
	for ( std::size_t i = 0; i < quads.size(); i++ ) {
		const Quad & q = quads[i];
		if ( !q.candidate )
			continue;
		const bool culled = occluded( P, pyr, q.c, radiusOf( q ), false );
		std::printf( "cluster %-10s visible %5ld samples, %s\n", q.name, vis[i], culled ? "CULLED" : "kept" );
		if ( vis[i] > 0 && culled )
			check( false, std::string( q.name ) + ": visible in the frame but culled" );
		if ( q.expect == 1 )
			check( vis[i] > 0 && !culled, std::string( q.name ) + " visible and kept" );
		if ( q.expect == 2 )
			check( vis[i] == 0 && culled, std::string( q.name ) + " hidden and culled (the test does cull)" );
		if ( q.expect == 3 )
			std::printf( "info %s: hidden behind an alpha-tested wall, %s\n", q.name,
				culled ? "culled" : "kept (the cull the fix gives up: its wall no longer occludes)" );
		if ( std::string( q.name ) == "mirror" )
			check( occluded( P, pyr, q.c, radiusOf( q ), true ),
				"control y-flipped box caught (the mirror reads the plank and is culled)" );
	}

	if ( fails ) {
		std::printf( "SIGNDROP1 FAIL (%d) gate=%s\n", fails, kRed ? "red-atocc" : "new" );
		return 1;
	}
	std::printf( "SIGNDROP1 PASS gate=%s\n", kRed ? "red-atocc" : "new" );
	return 0;
}
