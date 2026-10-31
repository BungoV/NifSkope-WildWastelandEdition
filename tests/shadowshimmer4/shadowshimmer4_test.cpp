/* lane SHADOWSHIMMER4 (FIX1031A): the unit of FARSWAYCOST2 (no GL, no Qt). It runs gl/farswayslice.h and
 * gl/sunshadowfit.h (the code gl/sunshadow.cpp and gl/cellfarinst.cpp run) on a Concord-shaped far field and reads the
 * shipped sources (mirror rule).
 *
 * The field: Concord's loaded 5x5 block (cells -17..-13 x 15..19) and FAR3's census rings 0-2 cut into gl/lodinative.cpp's
 * 4x4-cell tiles, each ring's sway triangles spread evenly over its cells (the census: 77764 / 2995013 / 364454). Views:
 * the eye at the block centre, 8240, 2600, 2048 and 1024 units from its border; 6 headings (2 diagonal); the sun at 65/40/20/10 degrees from
 * two azimuths; Cascade Quality Ultra (D 16000, splits 2000/6500, blend 400, map 4096); fov 55 vertical, 16:9.
 *
 *   G1 cost          the far sway triangles drawn over the static layers a still paint (60 fps): today's pass draws
 *                    every tile drawCascade's box cull keeps, in every cascade, every paint; the fix draws only the
 *                    tiles meeting each cascade's view slice, and only on that cascade's clock step (0.05 s = every
 *                    3rd paint). Bar: the median view's ratio fix / today <= 0.25 (the offline GPU bench measured 0.21 in
 *                    ms: notes fix1031a/shadowshimmer4) and the fix above today in no view.
 *   G2 composition   for every split caster state (dynamic or not, tiles in the slice or not) and job set, the static
 *                    layer + the live layer's draws hold every tile exactly once a pass, still or swaying as the cache-off
 *                    pass (set 0) draws it, and a still caster's out-of-slice tiles sit in the static layer.
 *   G3 clock step    over 6 s of 60 fps paints each cascade's caster clock ticks <= 1 / step + 1 times a second, at most
 *                    one cascade ticks a paint, and it lags the colour pass by < one step (<= 1.41 amp step = 0.71 units).
 *   G4 reach         reachInside() never empties a slice that holds a point outside the block square (2e5 random points
 *                    of the slice, every view), and tileMeets() never misses a sphere that has a point in the slice
 *                    (2e5 random spheres; brute force over 64 points of each).
 *   G5 mirror        sunshadow.cpp / cellfarinst.cpp run these functions (the source lines).
 *
 * Reds (each must FAIL): -DSS4_RED_TODAY (SHADOWSHIMMER3's pass: no split, no step -- G1, G2's static layer, G3),
 * -DSS4_RED_REACH (a doctored reach without the frustum's corner factor -- G4).
 * Build: g++ -std=c++17 -I<tree>/src tests/shadowshimmer4/shadowshimmer4_test.cpp; run: <exe> <tree>. */

#include "gl/farswayslice.h"
#include "gl/sunshadowfit.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace WwFarSwaySlice;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int g_fail = 0;
static void check( bool ok, const std::string & what, const std::string & detail )
{
	std::printf( "%s %s  %s\n", ok ? "ok  " : "FAIL", what.c_str(), detail.c_str() );
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

static std::string fmt( const char * f, double a, double b = 0, double c = 0, double d = 0, double e = 0 )
{
	char buf[320];
	std::snprintf( buf, sizeof( buf ), f, a, b, c, d, e );
	return buf;
}

static size_t countOf( const std::string & s, const std::string & what )
{
	size_t n = 0;
	for ( size_t p = s.find( what ); p != std::string::npos; p = s.find( what, p + 1 ) )
		n++;
	return n;
}

#ifdef SS4_RED_REACH
static bool reachRule( double zf, double tanX, double tanY, double edge )
{
	(void)tanX;
	(void)tanY;
	return edge > 0.0 && zf < edge;	// the doctored red: the slice's depth, not its corner distance
}
#else
static bool reachRule( double zf, double tanX, double tanY, double edge ) { return reachInside( zf, tanX, tanY, edge ); }
#endif

// ---- the field ----
constexpr double kCell = 4096.0, kGround = 7067.0;
constexpr int bx0 = -17, bx1 = -13, by0 = 15, by1 = 19;
struct Tile { double c[3]; double r; double tris; };

static std::vector<Tile> field()
{
	struct Ring { int x0, y0, x1, y1, hx0, hy0, hx1, hy1; double tris; };
	const Ring rings[3] = { { -18, 14, -12, 20, -17, 15, -13, 19, 77764.0 }, { -32, 0, 7, 39, -18, 14, -12, 20, 2995013.0 },
		{ -48, -16, 15, 47, -32, 0, 7, 39, 364454.0 } };
	std::vector<Tile> out;
	for ( const Ring & g : rings ) {
		const int cells = ( g.x1 - g.x0 + 1 ) * ( g.y1 - g.y0 + 1 ) - ( g.hx1 - g.hx0 + 1 ) * ( g.hy1 - g.hy0 + 1 );
		const double per = g.tris / cells;
		const int tx0 = int( std::floor( g.x0 / 4.0 ) ), tx1 = int( std::floor( g.x1 / 4.0 ) );
		const int ty0 = int( std::floor( g.y0 / 4.0 ) ), ty1 = int( std::floor( g.y1 / 4.0 ) );
		for ( int tx = tx0; tx <= tx1; tx++ )
			for ( int ty = ty0; ty <= ty1; ty++ ) {
				double lo[2] = { 1e30, 1e30 }, hi[2] = { -1e30, -1e30 };
				int n = 0;
				for ( int x = tx * 4; x < tx * 4 + 4; x++ )
					for ( int y = ty * 4; y < ty * 4 + 4; y++ ) {
						if ( x < g.x0 || x > g.x1 || y < g.y0 || y > g.y1 )
							continue;
						if ( x >= g.hx0 && x <= g.hx1 && y >= g.hy0 && y <= g.hy1 )
							continue;
						n++;
						lo[0] = std::min( lo[0], x * kCell );
						lo[1] = std::min( lo[1], y * kCell );
						hi[0] = std::max( hi[0], ( x + 1 ) * kCell );
						hi[1] = std::max( hi[1], ( y + 1 ) * kCell );
					}
				if ( !n )
					continue;
				Tile t;
				t.c[0] = 0.5 * ( lo[0] + hi[0] );
				t.c[1] = 0.5 * ( lo[1] + hi[1] );
				t.c[2] = kGround;
				t.r = std::sqrt( 0.25 * ( hi[0] - lo[0] ) * ( hi[0] - lo[0] ) + 0.25 * ( hi[1] - lo[1] ) * ( hi[1] - lo[1] ) + 1000.0 * 1000.0 );
				t.tris = per * n;
				out.push_back( t );
			}
	}
	return out;
}

struct View
{
	double C[3], fwd[3], rgt[3], up[3];
	double tanX, tanY;
	double R[3], U[3], L[3];
	WwCsmFit::Out fo[3];
	double zn[3], zf[3];
	double block[3];	// centre x, y and half size
};

static void cross3( const double * a, const double * b, double * o )
{
	o[0] = a[1] * b[2] - a[2] * b[1];
	o[1] = a[2] * b[0] - a[0] * b[2];
	o[2] = a[0] * b[1] - a[1] * b[0];
}
static void norm3( double * v )
{
	const double l = std::sqrt( v[0] * v[0] + v[1] * v[1] + v[2] * v[2] );
	for ( int i = 0; i < 3; i++ )
		v[i] /= l;
}
static double dot3( const double * a, const double * b ) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

// gl/sunshadow.cpp: the light basis and fitCascades with shadowset.cpp's Ultra preset
static View makeView( const double eye[3], double camAz, double sunEl, double sunAz )
{
	View v;
	for ( int j = 0; j < 3; j++ )
		v.C[j] = eye[j];
	v.fwd[0] = std::sin( camAz ); v.fwd[1] = std::cos( camAz ); v.fwd[2] = 0.0;
	v.rgt[0] = std::cos( camAz ); v.rgt[1] = -std::sin( camAz ); v.rgt[2] = 0.0;
	v.up[0] = 0.0; v.up[1] = 0.0; v.up[2] = 1.0;
	v.tanY = std::tan( 55.0 * M_PI / 360.0 );
	v.tanX = v.tanY * 16.0 / 9.0;
	const double to[3] = { std::cos( sunEl ) * std::sin( sunAz ), std::cos( sunEl ) * std::cos( sunAz ), std::sin( sunEl ) };
	for ( int j = 0; j < 3; j++ )
		v.L[j] = -to[j];
	const double Y[3] = { 0, 1, 0 };
	cross3( v.L, Y, v.R );
	norm3( v.R );
	cross3( v.R, v.L, v.U );
	norm3( v.U );
	const double D = 16000.0, s0 = 2000.0, s1 = 6500.0, blend = 400.0;
	const double far[3] = { s0, s1, D };
	for ( int i = 0; i < 3; i++ ) {
		WwCsmFit::In in;
		for ( int j = 0; j < 3; j++ ) {
			in.C[j] = v.C[j];
			in.fwd[j] = v.fwd[j];
			in.rc[j] = v.rgt[j];
			in.uc[j] = v.up[j];
			in.R[j] = v.R[j];
			in.U[j] = v.U[j];
			in.L[j] = v.L[j];
		}
		in.tanX = v.tanX;
		in.tanY = v.tanY;
		in.zn = i == 0 ? 0.0 : far[i - 1] - blend;
		in.zf = far[i] + blend;
		in.first = i == 0;
		in.map = 4096;
		in.lightBack = 2.0 * D;
		WwCsmFit::fitStable( in, v.fo[i] );
		v.zn[i] = in.zn;
		v.zf[i] = in.zf;
	}
	v.block[0] = 0.5 * ( bx0 + bx1 + 1 ) * kCell;
	v.block[1] = 0.5 * ( by0 + by1 + 1 ) * kCell;
	v.block[2] = std::max( bx1 - bx0, by1 - by0 + 0 ) * 0.5 * kCell + 0.5 * kCell;
	return v;
}

// gl/cellfarinst.cpp drawCascade's box cull: the tile centre in the cascade's clip x/y, its radius per axis
static bool boxKeeps( const View & v, int i, const Tile & t )
{
	const WwCsmFit::Out & o = v.fo[i];
	const double wx = o.vw * o.texel, wy = o.vh * o.texel;
	const double x = dot3( v.R, t.c ) * 2.0 / wx - ( o.pb[0] + o.l ) * 2.0 / wx - 1.0;
	const double y = dot3( v.U, t.c ) * 2.0 / wy - ( o.pb[1] + o.b ) * 2.0 / wy - 1.0;
	return std::fabs( x ) <= 1.0 + t.r * 2.0 / wx && std::fabs( y ) <= 1.0 + t.r * 2.0 / wy;
}

static Slice sliceOf( const View & v, int i )
{
	Slice s;
	s.tanX = v.tanX;
	s.tanY = v.tanY;
	s.zn = v.zn[i];
	s.zf = v.zf[i];
	const double edge = v.block[2] - std::max( std::fabs( v.C[0] - v.block[0] ), std::fabs( v.C[1] - v.block[1] ) );
	s.empty = reachRule( v.zf[i], v.tanX, v.tanY, edge );
	return s;
}

[[maybe_unused]] static void toView( const View & v, const double * p, double & x, double & y, double & z )
{
	const double d[3] = { p[0] - v.C[0], p[1] - v.C[1], p[2] - v.C[2] };
	x = dot3( d, v.rgt );
	y = dot3( d, v.up );
	z = -dot3( d, v.fwd );
}

static std::vector<View> views()
{
	std::vector<View> out;
	const double c[3] = { 0.5 * ( bx0 + bx1 + 1 ) * kCell, 0.5 * ( by0 + by1 + 1 ) * kCell, kGround + 200.0 };
	const double eyes[5][3] = { { c[0], c[1], c[2] }, { c[0] + 8192.0, c[1], c[2] }, { c[0], c[1] - 9216.0, c[2] },
		{ c[0] + 2000.0, c[1], c[2] }, { c[0] - 7640.0, c[1], c[2] } };
	for ( const auto & e : eyes )
		for ( double el : { 65.0, 40.0, 20.0, 10.0 } )
			for ( double az : { 225.0, 135.0 } )
				for ( double cam : { 0.0, 45.0, 90.0, 180.0, 270.0, 315.0 } )
					out.push_back( makeView( e, cam * M_PI / 180.0, el * M_PI / 180.0, az * M_PI / 180.0 ) );
	return out;
}

static void gateG1()
{
	const std::vector<Tile> tiles = field();
	const std::vector<View> vs = views();
	std::vector<double> ratio;
	double worst = 0.0, redMed = 0.0, fixMed = 0.0;
	std::vector<double> reds, fixes;
	for ( const View & v : vs ) {
		double red = 0.0, fix = 0.0;
		for ( int i = 0; i < 3; i++ ) {
			const Slice s = sliceOf( v, i );
			(void)s;
			for ( const Tile & t : tiles ) {
				if ( !boxKeeps( v, i, t ) )
					continue;
				red += t.tris;
#ifdef SS4_RED_TODAY
				fix += t.tris;	// SHADOWSHIMMER3: every kept tile, every cascade, every paint
#else
				double x, y, z;
				toView( v, t.c, x, y, z );
				if ( tileMeets( s, x, y, z, t.r ) )
					fix += t.tris / 3.0;	// the 0.05 s step at 60 fps: each cascade every 3rd paint
#endif
			}
		}
		reds.push_back( red );
		fixes.push_back( fix );
		ratio.push_back( red > 0.0 ? fix / red : 0.0 );
		worst = std::max( worst, red > 0.0 ? fix / red : 0.0 );
	}
	std::vector<double> r = ratio;
	std::sort( r.begin(), r.end() );
	std::sort( reds.begin(), reds.end() );
	std::sort( fixes.begin(), fixes.end() );
	redMed = reds[reds.size() / 2];
	fixMed = fixes[fixes.size() / 2];
	const double med = r[r.size() / 2];
	check( med <= 0.25, "G1 cost: the median view's far sway tris drawn a still paint, fix / today <= 0.25",
		fmt( "%.3f over %.0f views (today median %.0f tris, fix median %.0f tris a paint)", med, double( r.size() ), redMed, fixMed ) );
	check( worst <= 1.0, "G1 cost: the fix draws more than today in no view", fmt( "worst ratio %.3f", worst ) );
}

// G2: which tiles each set draws, mirrored from sunshadow.cpp's inSet + the far branch (parts)
static void gateG2()
{
	int bad = 0, cases = 0, staticMiss = 0;
	for ( int dyn = 0; dyn < 2; dyn++ )
		for ( int meets = 0; meets < 2; meets++ ) {
			// per tile class (in the slice / out), what set 0 draws, and what sets 1 + 2 draw together
			for ( int inSlice = 0; inSlice < 2; inSlice++ ) {
				if ( inSlice && !meets )
					continue;	// a caster with no tile in the slice has no in-slice tile
				cases++;
				auto drawn = [&]( int set ) -> int {	// 0 none, 1 still, 2 swaying
#ifdef SS4_RED_TODAY
					// SHADOWSHIMMER3: a sway caster is dynamic; set 0 / 2 draw it swaying whole, set 1 never
					(void)dyn;
					return set == 1 ? 0 : 2;
#else
					const bool in = ( parts( set, dyn != 0 ) & ( meets ? 3 : 1 ) ) != 0;	// inSet
					if ( !in )
						return 0;
					const int p = parts( set, dyn != 0 );
					if ( inSlice )
						return ( p & Sway ) && meets ? 2 : 0;
					return ( p & Still ) ? 1 : 0;
#endif
				};
				const int a = drawn( 0 ), b1 = drawn( 1 ), b2 = drawn( 2 );
				// once a pass: exactly one of the static or live draws, equal to set 0's
				const int once = ( b1 ? 1 : 0 ) + ( b2 ? 1 : 0 );
				if ( once != 1 || ( b1 ? b1 : b2 ) != a )
					bad++;
				// the target of the fix: a still, settled caster's out-of-slice tiles live in the static layer
				if ( !dyn && !inSlice && b1 != 1 )
					staticMiss++;
			}
		}
	check( bad == 0, "G2 composition: static + live layers draw every tile once a pass, as the cache-off pass draws it",
		fmt( "%.0f of %.0f cases differ", bad, cases ) );
	check( staticMiss == 0, "G2 composition: a settled caster's out-of-slice tiles sit in the static layer",
		fmt( "%.0f cases redrawn every paint instead", staticMiss ) );
}

static void gateG3()
{
#ifdef SS4_RED_TODAY
	const double step = 0.0;
#else
	const double step = 0.05;	// sunshadow.cpp swayStep()'s default (G5 reads it)
#endif
	// a paint rate just over 60 fps, 75 and 144 (exactly 60 puts every tick on a paint boundary: either side is right)
	const double secs = 6.0;
	int multi = 0;
	double lag = 0.0, perSec = 0.0;
	for ( double fps : { 61.0, 75.0, 144.0 } ) {
		int ticks[3] = { 0, 0, 0 };
		double last[3] = { -1, -1, -1 };
		const int paints = int( fps * secs );
		for ( int k = 0; k < paints; k++ ) {
			const double t = double( float( 100.0 + k / fps ) );	// farSway.time is a float
			int n = 0;
			for ( int i = 0; i < 3; i++ ) {
				double tq = t;
				if ( step > 0.0 ) {
					const double o = step * double( i ) / 3.0;
					tq = double( float( std::floor( ( t + o ) / step ) * step - o ) );
				}
				lag = std::max( lag, t - tq );
				if ( k == 0 || tq != last[i] ) {
					if ( k > 0 ) {
						ticks[i]++;
						n++;
					}
					last[i] = tq;
				}
			}
			multi += n > 1 ? 1 : 0;
		}
		perSec = std::max( perSec, std::max( { ticks[0], ticks[1], ticks[2] } ) / secs );
	}
	const double bar = step > 0.0 ? 1.0 / step + 1.0 : 0.0;
	check( step > 0.0 && perSec <= bar, "G3 clock step: each cascade's caster clock ticks <= 1 / step + 1 times a second",
		fmt( "%.1f a second (bar %.1f; step %.3f s)", perSec, bar, step ) );
	check( multi == 0, "G3 clock step: at most one cascade ticks a paint (61 / 75 / 144 fps)", fmt( "%.0f paints with more", multi ) );
	const double amp = 10.0, disp = std::sqrt( 1.3 * 1.3 + 0.54 * 0.54 ) * amp * lag;
	check( lag <= step + 1e-4 && disp <= 0.75, "G3 clock step: the casters lag the colour pass < one step (<= 0.75 units)",
		fmt( "lag %.4f s, <= %.3f units of sway", lag, disp ) );
}

static void gateG4()
{
	const std::vector<View> vs = views();
	std::mt19937 rng( 1031 );
	std::uniform_real_distribution<double> u01( 0.0, 1.0 );
	long outside = 0, tested = 0;
	for ( const View & v : vs ) {
		for ( int i = 0; i < 3; i++ ) {
			const Slice s = sliceOf( v, i );
			if ( !s.empty )
				continue;
			for ( int k = 0; k < 4000; k++ ) {
				// a random point of the view slice: depth in [zn, zf], inside the side planes
				const double d = s.zn + ( s.zf - s.zn ) * u01( rng );
				const double x = ( 2.0 * u01( rng ) - 1.0 ) * s.tanX * d, y = ( 2.0 * u01( rng ) - 1.0 ) * s.tanY * d;
				double p[3];
				for ( int j = 0; j < 3; j++ )
					p[j] = v.C[j] + v.fwd[j] * d + v.rgt[j] * x + v.up[j] * y;
				tested++;
				if ( std::max( std::fabs( p[0] - v.block[0] ), std::fabs( p[1] - v.block[1] ) ) > v.block[2] )
					outside++;
			}
		}
	}
	check( tested > 0 && outside == 0, "G4 reach: an emptied slice holds no point outside the loaded block",
		fmt( "%.0f of %.0f sampled points outside", double( outside ), double( tested ) ) );
	// tileMeets is conservative: no sphere with a point in the slice is called outside
	long missed = 0, spheres = 0;
	Slice s;
	s.tanX = 0.9255;
	s.tanY = 0.5206;
	s.zn = 6100.0;
	s.zf = 16400.0;
	for ( int k = 0; k < 200000; k++ ) {
		const double cx = ( u01( rng ) - 0.5 ) * 60000.0, cy = ( u01( rng ) - 0.5 ) * 30000.0, cz = -u01( rng ) * 30000.0 + 5000.0;
		const double r = 200.0 + u01( rng ) * 12000.0;
		bool any = false;
		for ( int j = 0; j < 64 && !any; j++ ) {
			double dx = u01( rng ) * 2 - 1, dy = u01( rng ) * 2 - 1, dz = u01( rng ) * 2 - 1;
			const double l = std::sqrt( dx * dx + dy * dy + dz * dz ) + 1e-12;
			const double q = r * std::cbrt( u01( rng ) ) / l;
			const double px = cx + dx * q, py = cy + dy * q, pz = cz + dz * q, d = -pz;
			any = d >= s.zn && d <= s.zf && std::fabs( px ) <= s.tanX * d && std::fabs( py ) <= s.tanY * d;
		}
		if ( any ) {
			spheres++;
			if ( !tileMeets( s, cx, cy, cz, r ) )
				missed++;
		}
	}
	check( spheres > 1000 && missed == 0, "G4 tileMeets: no sphere with a point in the slice is missed",
		fmt( "%.0f missed of %.0f spheres with a point inside", double( missed ), double( spheres ) ) );
}

static void gateG5( const std::string & tree )
{
	const std::string sun = readFile( tree + "/src/gl/sunshadow.cpp" ), inst = readFile( tree + "/src/gl/cellfarinst.cpp" );
	const bool a = countOf( sun, "return k.split ? ( WwFarSwaySlice::parts( set, k.dynamic ) & ( k.meets[i] > 0 ? 3 : 1 ) ) != 0" ) == 1
		&& countOf( sun, "const int part = WwFarSwaySlice::parts( J.set, k.dynamic );" ) == 1
		&& countOf( sun, "WwFarSwaySlice::reachInside( s.c[i].zf, s.tanX, s.tanY, edge )" ) == 1
		&& countOf( sun, "isEmpty() ? 0.05" ) == 1
		&& countOf( sun, "q == QLatin1String( \"swayall\" ) ? 256" ) == 1
		&& countOf( sun, "c.split = c.sway && swaySplitOn && sh->wwFarInstData();" ) == 1
		&& countOf( sun, "p->uni1f( \"farSwayTime\", swayTq[J.i] );" ) == 1
		&& countOf( sun, "const double o = swayStepS * double( i ) / double( kCascades );" ) == 1;
	check( a, "G5 mirror: sunshadow.cpp runs the split (parts, inSet), the reach rule, the 0.05 s staggered step, red swayall",
		a ? "found" : "not found" );
	const bool b = countOf( inst, "return WwFarSwaySlice::tileMeets( s, x, y, z, double( t.radius ) * sc );" ) == 1
		&& countOf( inst, "if ( split && tileInSlice( t, mv, *slice ) != ( part == WwFarSwaySlice::Sway ) )" ) == 1;
	check( b, "G5 mirror: cellfarinst.cpp splits a bucket's tiles by tileMeets", b ? "found" : "not found" );
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	const std::string tree = argc > 1 ? argv[1] : ".";
	gateG1();
	gateG2();
	gateG3();
	gateG4();
	gateG5( tree );
	std::printf( "SHADOWSHIMMER4 UNIT %s (%d failed)\n", g_fail ? "FAIL" : "PASS", g_fail );
	return g_fail ? 1 : 0;
}
