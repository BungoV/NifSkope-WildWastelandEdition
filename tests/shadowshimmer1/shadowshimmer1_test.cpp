/* lane SHADOWSHIMMER1: the stable sun shadows' unit (no GL, no Qt): it runs gl/sunshadowfit.h, the very fit and kernel
 * rotation gl/sunshadow.cpp and ww_sunshadow.glsl run, and reads the shipped numbers out of the sources (mirror rule).
 *
 *   G1 texel stability  a still camera over 30 TAA frames, a sub-texel slide, a 30-degree turn: the windows' origins
 *                       move in whole texels (depth in whole 64-quantum steps), the texel never changes, and the kernel
 *                       rotation of every still world point stays the same.
 *   G2 detachment       the contact gap of a thin caster, B sin(e) cos(e) / texel with B the caster's slope + unit offset
 *                       plus the receiver's constant, <= 1 shadow-map texel per cascade at every sun 5..85 degrees; and
 *                       the acne guard: the depth margin (normal offset + offsets) >= the plane's slope x 1 texel + half
 *                       a D16 quantum for the ground and a sun-facing wall.
 *   G3 leaf motion      the caster programs read no per-frame uniform (only the clip / model-view matrices, the far ring
 *                       flag and the alpha test's inputs); the shader text carries the stable rotation.
 *
 * Reds (each must FAIL): -DSS_RED_FIT (CSM1's box fit), -DSS_RED_KERNEL (the pixel + frame % 61 rotation),
 * -DSS_RED_BIAS (slope 6, 12 quanta, 1.5 texels, no normal offset), -DSS_RED_NONORMAL (the new bias without the normal
 * offset: the acne guard), -DSS_RED_G3 (a caster shader that reads the TAA frame's clock; since SHADOWSHIMMER2 the far
 * sway, the colour pass's own, is the one per-frame input a caster may read).
 * Build: g++ -std=c++17 -I<tree>/src tests/shadowshimmer1/shadowshimmer1_test.cpp; run: <exe> <tree>. */

#include "gl/sunshadowfit.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace WwCsmFit;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int g_fail = 0;
static void check( bool ok, const char * what, const std::string & detail )
{
	std::printf( "%s %s  %s\n", ok ? "ok  " : "FAIL", what, detail.c_str() );
	if ( !ok )
		g_fail++;
}

static std::string readFile( const std::string & p )
{
	std::ifstream f( p, std::ios::binary );
	std::stringstream s;
	s << f.rdbuf();
	return s.str();
}

static void cross3( const double * a, const double * b, double * o )
{
	o[0] = a[1] * b[2] - a[2] * b[1];
	o[1] = a[2] * b[0] - a[0] * b[2];
	o[2] = a[0] * b[1] - a[1] * b[0];
}
static void norm3( double * v )
{
	const double l = std::sqrt( dot3( v, v ) );
	for ( int i = 0; i < 3; i++ )
		v[i] /= l;
}

// ---- the default setup: Cascade Quality Ultra under FO4CS-level shadows (Shadow Distance 14000), 1920 x 1080, fov 55 ----
struct Setup
{
	int map = 4096;
	double D = 14000, split0 = 2000 * 0.875, split1 = 6500 * 0.875, blend = 400 * 0.875, lightBack = 28000;
	double tanY = std::tan( 27.5 * M_PI / 180.0 ), tanX = std::tan( 27.5 * M_PI / 180.0 ) * 1920.0 / 1080.0;
	double camNear = 5.0;
	int W = 1920, H = 1080;
};

struct Camera
{
	double C[3], fwd[3], rc[3], uc[3];
};

static Camera makeCamera( const double C[3], double yawDeg, double pitchDeg )
{
	Camera k;
	const double y = yawDeg * M_PI / 180.0, p = pitchDeg * M_PI / 180.0;
	const double f[3] = { std::cos( p ) * std::sin( y ), std::cos( p ) * std::cos( y ), std::sin( p ) };
	const double Z[3] = { 0, 0, 1 };
	double r[3];
	cross3( f, Z, r );
	norm3( r );
	double u[3];
	cross3( r, f, u );
	for ( int j = 0; j < 3; j++ ) {
		k.C[j] = C[j];
		k.fwd[j] = f[j];
		k.rc[j] = r[j];
		k.uc[j] = u[j];
	}
	return k;
}

struct Light
{
	double L[3], R[3], U[3];
	double elevDeg;
};

static Light makeLight( double elevDeg, double azDeg )
{
	Light l;
	const double e = elevDeg * M_PI / 180.0, a = azDeg * M_PI / 180.0;
	const double to[3] = { std::cos( e ) * std::sin( a ), std::cos( e ) * std::cos( a ), std::sin( e ) };
	for ( int j = 0; j < 3; j++ )
		l.L[j] = -to[j];
	const double Y[3] = { 0, 1, 0 };
	cross3( l.L, Y, l.R );	// gl/sunshadow.cpp: right = normalize(L x Y), up = normalize(right x L)
	norm3( l.R );
	cross3( l.R, l.L, l.U );
	norm3( l.U );
	l.elevDeg = elevDeg;
	return l;
}

static void fitAll( const Setup & S, const Camera & k, const Light & l, Out o[3] )
{
	const double far[3] = { S.split0, S.split1, S.D };
	for ( int i = 0; i < 3; i++ ) {
		In in;
		for ( int j = 0; j < 3; j++ ) {
			in.C[j] = k.C[j];
			in.fwd[j] = k.fwd[j];
			in.rc[j] = k.rc[j];
			in.uc[j] = k.uc[j];
			in.R[j] = l.R[j];
			in.U[j] = l.U[j];
			in.L[j] = l.L[j];
		}
		in.tanX = S.tanX;
		in.tanY = S.tanY;
		in.zn = i == 0 ? S.camNear : far[i - 1] - S.blend;
		in.zf = far[i] + S.blend;
		in.first = i == 0;
		in.map = S.map;
		in.lightBack = S.lightBack;
#ifdef SS_RED_FIT
		fitOld( in, o[i] );
#else
		fitStable( in, o[i] );
#endif
	}
}

//! the pixel (x right, y up, from the bottom-left) world point Q lands in, with the TAA jitter (pixels)
static bool project( const Setup & S, const Camera & k, const double Q[3], double jx, double jy, double & fx, double & fy )
{
	double d[3];
	for ( int j = 0; j < 3; j++ )
		d[j] = Q[j] - k.C[j];
	const double z = dot3( d, k.fwd );
	if ( z <= S.camNear )
		return false;
	const double px = S.W * 0.5 + dot3( d, k.rc ) / z / S.tanX * S.W * 0.5 + jx;
	const double py = S.H * 0.5 + dot3( d, k.uc ) / z / S.tanY * S.H * 0.5 + jy;
	if ( px < 0 || py < 0 || px >= S.W || py >= S.H )
		return false;
	fx = std::floor( px ) + 0.5;
	fy = std::floor( py ) + 0.5;
	return true;
}

//! the cascade the point is in (by planar view depth, the pair's first)
static int cascadeOf( const Setup & S, const Camera & k, const double Q[3] )
{
	double d[3];
	for ( int j = 0; j < 3; j++ )
		d[j] = Q[j] - k.C[j];
	const double z = dot3( d, k.fwd );
	return z < S.split0 ? 0 : z < S.split1 ? 1 : 2;
}

//! the kernel rotation the receiver gives world point Q this frame (frame index for the TAA phase, jitter in pixels)
static bool rotationOf( const Setup & S, const Camera & k, const Light & l, const Out o[3], const double Q[3], int frame,
	double jx, double jy, double & rot, int & ci )
{
	double fx, fy;
	if ( !project( S, k, Q, jx, jy, fx, fy ) )
		return false;
	ci = cascadeOf( S, k, Q );	// a point that changes cascade gets that cascade's lattice: compared per (point, cascade)
#ifdef SS_RED_KERNEL
	( void ) o;
	( void ) l;
	rot = rotOld( fx, fy, double( frame % 61 ) );
#else
	( void ) frame;
	rot = rotStable( o[ci], l.R, l.U, Q );
#endif
	return true;
}

static double halton( int i, int b )
{
	double f = 1, r = 0;
	for ( ; i > 0; i /= b ) {
		f /= b;
		r += f * ( i % b );
	}
	return r;
}

// world points on the ground (z = 0) in front of the camera, a fixed pseudo-random set
static std::vector<std::array<double, 3>> groundPoints( const Camera & k, int n )
{
	std::vector<std::array<double, 3>> pts;
	unsigned s = 12345u;
	auto rnd = [&s]() { s = s * 1664525u + 1013904223u; return double( s >> 8 ) / double( 1u << 24 ); };
	while ( int( pts.size() ) < n ) {
		const double dist = 60.0 + 1800.0 * rnd() * rnd(), side = ( rnd() - 0.5 ) * 1.6 * dist;
		std::array<double, 3> q;
		for ( int j = 0; j < 2; j++ )
			q[j] = k.C[j] + k.fwd[j] * dist / std::hypot( k.fwd[0], k.fwd[1] ) + k.rc[j] * side;
		q[2] = 0.0;
		pts.push_back( q );
	}
	return pts;
}

static std::string fmt( const char * f, double a, double b = 0, double c = 0, double d = 0 )
{
	char buf[256];
	std::snprintf( buf, sizeof( buf ), f, a, b, c, d );
	return buf;
}

// ---- G1 ----
static void gateG1( const Setup & S )
{
	const Light l = makeLight( 40.0, 130.0 );
	const double C0[3] = { 1234.5, 2345.25, 180.0 };
	const Camera k0 = makeCamera( C0, 35.0, -15.0 );
	const auto pts = groundPoints( k0, 4000 );

	// (a) still camera, 30 TAA frames (Halton 2,3 jitter, the frame index runs)
	{
		Out o0[3];
		fitAll( S, k0, l, o0 );
		std::vector<double> rot0( pts.size() * 3, -1.0 );
		int movedOrigin = 0, changedRot = 0, seen = 0;
		for ( int f = 0; f < 30; f++ ) {
			Out o[3];
			fitAll( S, k0, l, o );
			for ( int i = 0; i < 3; i++ )
				movedOrigin += ( o[i].pb[0] + o[i].l != o0[i].pb[0] + o0[i].l || o[i].pb[1] + o[i].b != o0[i].pb[1] + o0[i].b
					|| o[i].pb[2] != o0[i].pb[2] || o[i].texel != o0[i].texel ) ? 1 : 0;
			const double jx = halton( f + 1, 2 ) - 0.5, jy = halton( f + 1, 3 ) - 0.5;
			for ( size_t p = 0; p < pts.size(); p++ ) {
				double r;
				int ci = 0;
				if ( !rotationOf( S, k0, l, o, pts[p].data(), f, jx, jy, r, ci ) )
					continue;
				double & r0 = rot0[p * 3 + size_t( ci )];
				if ( r0 < 0.0 ) {
					r0 = r;
					seen++;
				} else if ( r != r0 ) {
					changedRot++;
					r0 = r;
				}
			}
		}
		check( movedOrigin == 0, "G1a still: window origins / texels changed (frames x cascades)", fmt( "%.0f", movedOrigin ) );
		check( changedRot == 0 && seen > 1000, "G1a still: kernel rotation changes per world point over 30 TAA frames",
			fmt( "%.0f changes over %.0f points", changedRot, seen ) );
	}
	// (b) a sub-texel slide: 0.37 of cascade 0's texel per frame, oblique (along the light too), 30 frames
	{
		Out o0[3];
		fitAll( S, k0, l, o0 );
		const double step[3] = { 0.29 * o0[0].texel, -0.21 * o0[0].texel, 0.11 * o0[0].texel };
		std::vector<double> rot0( pts.size() * 3, -1.0 );
		int fracOrigin = 0, texelChanged = 0, fracDepth = 0, changedRot = 0, seen = 0, moves = 0;
		double maxFrac = 0.0;
		for ( int f = 0; f < 30; f++ ) {
			double C[3];
			for ( int j = 0; j < 3; j++ )
				C[j] = C0[j] + step[j] * f;
			const Camera k = makeCamera( C, 35.0, -15.0 );
			Out o[3];
			fitAll( S, k, l, o );
			for ( int i = 0; i < 3; i++ ) {
				texelChanged += o[i].texel != o0[i].texel ? 1 : 0;
				const double dx = ( o[i].pb[0] + o[i].l - o0[i].pb[0] - o0[i].l ) / o[i].texel;
				const double dy = ( o[i].pb[1] + o[i].b - o0[i].pb[1] - o0[i].b ) / o[i].texel;
				const double fr = std::max( std::fabs( dx - std::round( dx ) ), std::fabs( dy - std::round( dy ) ) );
				maxFrac = std::max( maxFrac, fr );
				fracOrigin += fr > 1e-6 ? 1 : 0;
				moves += ( std::round( dx ) != 0.0 || std::round( dy ) != 0.0 ) ? 1 : 0;
#ifndef SS_RED_FIT
				const double dz = ( o[i].pb[2] - o0[i].pb[2] ) / ( o[i].maxZ / 65535.0 * kDepthStep );
				fracDepth += std::fabs( dz - std::round( dz ) ) > 1e-6 ? 1 : 0;
#endif
			}
			for ( size_t p = 0; p < pts.size(); p++ ) {
				double r;
				int ci = 0;
				if ( !rotationOf( S, k, l, o, pts[p].data(), 7, 0.0, 0.0, r, ci ) )
					continue;
				double & r0 = rot0[p * 3 + size_t( ci )];
				if ( r0 < 0.0 ) {
					r0 = r;
					seen++;
				} else if ( r != r0 ) {
					changedRot++;
					r0 = r;
				}
			}
		}
		check( fracOrigin == 0 && moves > 0, "G1b slide: the windows move by whole texels only",
			fmt( "%.0f fractional moves (max %.3g texel), %.0f whole moves", fracOrigin, maxFrac, moves ) );
		check( texelChanged == 0, "G1b slide: texel unchanged", fmt( "%.0f", texelChanged ) );
		check( fracDepth == 0, "G1b slide: the depth origin moves in whole 64-quantum steps", fmt( "%.0f", fracDepth ) );
		check( changedRot == 0 && seen > 1000, "G1b slide: kernel rotation changes per world point",
			fmt( "%.0f changes over %.0f points", changedRot, seen ) );
	}
	// (c) a turn on the spot: yaw 35 -> 65 degrees, 1 degree a frame
	{
		Out o0[3];
		fitAll( S, k0, l, o0 );
		std::vector<double> rot0( pts.size() * 3, -1.0 );
		int texelChanged = 0, sizeChanged = 0, fracOrigin = 0, changedRot = 0, seen = 0;
		for ( int f = 0; f <= 30; f++ ) {
			const Camera k = makeCamera( C0, 35.0 + f, -15.0 );
			Out o[3];
			fitAll( S, k, l, o );
			for ( int i = 0; i < 3; i++ ) {
				texelChanged += o[i].texel != o0[i].texel ? 1 : 0;
				sizeChanged += ( o[i].vw != o0[i].vw || o[i].vh != o0[i].vh ) ? 1 : 0;
				const double dx = ( o[i].pb[0] + o[i].l - o0[i].pb[0] - o0[i].l ) / o[i].texel;
				fracOrigin += std::fabs( dx - std::round( dx ) ) > 1e-6 ? 1 : 0;
			}
			for ( size_t p = 0; p < pts.size(); p++ ) {
				double r;
				int ci = 0;
				if ( !rotationOf( S, k, l, o, pts[p].data(), 7, 0.0, 0.0, r, ci ) )
					continue;
				double & r0 = rot0[p * 3 + size_t( ci )];
				if ( r0 < 0.0 ) {
					r0 = r;
					seen++;
				} else if ( r != r0 ) {
					changedRot++;
					r0 = r;
				}
			}
		}
		check( texelChanged == 0 && sizeChanged == 0, "G1c turn: texel and window size unchanged (frames x cascades)",
			fmt( "texel %.0f size %.0f", texelChanged, sizeChanged ) );
		check( fracOrigin == 0, "G1c turn: the windows move by whole texels only", fmt( "%.0f", fracOrigin ) );
		check( changedRot == 0 && seen > 1000, "G1c turn: kernel rotation changes per world point",
			fmt( "%.0f changes over %.0f points", changedRot, seen ) );
	}
}

// ---- G2 ----
struct Bias
{
	double slope, units, recvTexels, normalTexels;
};

static Bias shippedBias( const std::string & tree )
{
#if defined( SS_RED_BIAS )
	( void ) tree;
	return { 6.0, 12.0, 1.5, 0.0 };	// today's (TERRSHADOW1 / SHADOWPAR1) numbers
#else
	// the mirror: the numbers wwShadowCascadePreset ships (src/gl/shadowset.cpp)
	const std::string src = readFile( tree + "/src/gl/shadowset.cpp" );
	const bool ok = src.find( "p.biasTexels = 0.25f;" ) != std::string::npos && src.find( "p.slopeBias = 0.5f;" ) != std::string::npos
		&& src.find( "p.unitBias = 2.0f;" ) != std::string::npos && src.find( "p.normalTexels = 1.0f;" ) != std::string::npos;
	check( ok, "G2 mirror: shadowset.cpp ships slope 0.5, 2 quanta, 0.25 texel, normal offset 1 texel", ok ? "found" : "not found" );
#ifdef SS_RED_NONORMAL
	return { 0.5, 2.0, 0.25, 0.0 };
#else
	return { 0.5, 2.0, 0.25, 1.0 };
#endif
#endif
}

//! the caster's offset in world units along the light for a plane of depth slope s (world depth / world across)
static double casterOffset( const Bias & b, double s, double texel, double wz )
{
	const double unitW = b.units * wz / 65536.0;
	const double off = b.slope * s * texel + unitW;
	const double clampW = unitW + b.slope * texel * 4.0;	// SHADOWPAR1's glPolygonOffsetClamp at a slope of 4
	return std::min( off, clampW );
}

static void gateG2( const Setup & S, const std::string & tree )
{
	const Bias b = shippedBias( tree );
	const double C0[3] = { 1234.5, 2345.25, 180.0 };
	const Camera k0 = makeCamera( C0, 35.0, -15.0 );
	double worstGap[3] = { 0, 0, 0 }, worstAt[3] = { 0, 0, 0 };
	double acneShort = 0.0, acneAt = 0.0;
	int acneFails = 0;
	for ( int e = 5; e <= 85; e += 5 ) {
		const Light l = makeLight( e, 130.0 );
		Out o[3];
		fitAll( S, k0, l, o );
		const double se = std::sin( e * M_PI / 180.0 ), ce = std::cos( e * M_PI / 180.0 ), te = se / ce;
		for ( int i = 0; i < 3; i++ ) {
			const double t = o[i].texel, wz = o[i].far - kNear, q = wz / 65535.0;
			// a thin vertical card facing the sun: its depth slope is tan(e) along the light's up axis
			const double B = casterOffset( b, te, t, wz ) + b.recvTexels * t;
			const double gap = B * se * ce / t;	// light-space texels from the card's foot to the shadow's start
			if ( gap > worstGap[i] ) {
				worstGap[i] = gap;
				worstAt[i] = e;
			}
			// acne: the ground (slope 1 / tan e) and a sun-facing wall (slope tan e) against themselves
			for ( double s : { 1.0 / te, te } ) {
				const double margin = b.normalTexels * t * s + casterOffset( b, s, t, wz ) + b.recvTexels * t;
				const double need = s * t + 0.5 * q;
				if ( margin < need ) {
					acneFails++;
					if ( need - margin > acneShort ) {
						acneShort = need - margin;
						acneAt = e;
					}
				}
			}
		}
	}
	for ( int i = 0; i < 3; i++ )
		check( worstGap[i] <= 1.0, i == 0 ? "G2 cascade 0: contact gap <= 1 texel" : i == 1 ? "G2 cascade 1: contact gap <= 1 texel"
			: "G2 cascade 2: contact gap <= 1 texel", fmt( "worst %.3f texel at %.0f deg", worstGap[i], worstAt[i] ) );
	check( acneFails == 0, "G2 acne guard: margin >= slope x texel + half a quantum (ground + wall, 5..85 deg, 3 cascades)",
		fmt( "%.0f short (worst %.3f units at %.0f deg)", acneFails, acneShort, acneAt ) );
}

// ---- G3 ----
static std::set<std::string> uniformsOf( const std::string & text )
{
	std::set<std::string> u;
	std::istringstream in( text );
	std::string line;
	while ( std::getline( in, line ) ) {
		const size_t p = line.find( "uniform " );
		if ( p == std::string::npos || line.find( "//" ) < p )
			continue;
		std::string rest = line.substr( p + 8 );
		const size_t semi = rest.find( ';' );
		if ( semi == std::string::npos )
			continue;
		rest = rest.substr( 0, semi );
		const size_t sp = rest.find_last_of( " \t" );
		std::string name = sp == std::string::npos ? rest : rest.substr( sp + 1 );
		const size_t br = name.find( '[' );
		if ( br != std::string::npos )
			name = name.substr( 0, br );
		u.insert( name );
	}
	return u;
}

static void gateG3( const std::string & tree )
{
	const char * files[] = { "lookdev_csmdepth.vert", "lookdev_csmdepth.frag", "lookdev_csmdepth_at.vert", "lookdev_csmdepth_at.frag",
		"lookdev_csmdepth_q.vert", "lookdev_csmdepth_atq.vert" };
	/* lane SHADOWSHIMMER2 (FARSWAYSHADOW): the far trees' wind is the one per-frame input a caster may read, the colour
	 * pass's own (farSway*, lodTreeAnim; tests/shadowshimmer2 G3 holds it to fo4_default.vert's block); the near cells'
	 * leaves stand inside the loaded block and never sway */
	const std::set<std::string> allowed = { "csmClipFromView", "modelViewMatrix", "farInst", "BaseMap", "uvScale", "uvOffset", "alpha",
		"alphaThreshold", "farSwayAmp", "farSwayTime", "farSwayBlock", "lodTreeAnim" };
	int read = 0;
	std::string extra;
	for ( const char * f : files ) {
		std::string text = readFile( tree + "/res/shaders/" + f );
		if ( text.empty() )
			continue;
		read++;
#ifdef SS_RED_G3
		if ( std::strcmp( f, "lookdev_csmdepth.vert" ) == 0 )
			text += "\nuniform float ussCsmPhase;\n";	// the red: a caster that moves by the TAA frame's clock
#endif
		for ( const std::string & n : uniformsOf( text ) ) {
			if ( !allowed.count( n ) )
				extra += std::string( f ) + ":" + n + " ";
		}
	}
	check( read == int( sizeof( files ) / sizeof( files[0] ) ), "G3 caster programs read", fmt( "%.0f of 6", read ) );
	check( extra.empty(), "G3 casters read no per-frame uniform (no sway, clock, jitter in the caster pass)", extra.empty() ? "none" : extra );
	// the caster pass's only CPU deformer is the wind cables' sway (FAR5: the same wind the colour pass uses)
	const std::string sun = readFile( tree + "/src/gl/sunshadow.cpp" );
	size_t n = 0;
	for ( size_t p = sun.find( "cableSway( scene" ); p != std::string::npos; p = sun.find( "cableSway( scene", p + 1 ) )
		n++;
	check( n == 1, "G3 one CPU caster deformer (the wind cables, FAR5)", fmt( "%.0f call(s)", double( n ) ) );
	// the receiver: the shader carries the stable rotation the twin mirrors (kCells 4, wrap 4096)
	const std::string glsl = readFile( tree + "/res/shaders/ww_sunshadow.glsl" );
	const bool mirror = kCells == 4 && kCellWrap == 4096.0
		&& glsl.find( "mod( floor( q.xy * ( csmParams.z * 4.0 ) ) + vec2( ussCsmCellX[i], ussCsmCellY[i] ), 4096.0 )" ) != std::string::npos
		&& glsl.find( "float rot = 6.2831853 * ussIgn( rp );" ) != std::string::npos;
	check( mirror, "G3 mirror: ww_sunshadow.glsl's rotation is the twin's (lattice 4 / texel, wrap 4096)", mirror ? "found" : "not found" );
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	const std::string tree = argc > 1 ? argv[1] : ".";
	Setup S;
	// the default fit, for the record
	{
		const double C0[3] = { 1234.5, 2345.25, 180.0 };
		const Camera k0 = makeCamera( C0, 35.0, -15.0 );
		Out o[3];
		fitAll( S, k0, makeLight( 40.0, 130.0 ), o );
		for ( int i = 0; i < 3; i++ )
			std::printf( "info cascade %d: texel %.4f window %dx%d depth range %.1f (quantum %.4f) sphere r %.0f\n", i, o[i].texel, o[i].vw,
				o[i].vh, o[i].far - kNear, ( o[i].far - kNear ) / 65535.0, o[i].sphereR );
	}
	gateG1( S );
	gateG2( S, tree );
	gateG3( tree );
	std::printf( "SHADOWSHIMMER1 UNIT %s (%d failed)\n", g_fail ? "FAIL" : "PASS", g_fail );
	return g_fail ? 1 : 0;
}
