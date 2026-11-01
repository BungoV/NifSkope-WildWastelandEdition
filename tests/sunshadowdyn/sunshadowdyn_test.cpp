/* lane PERFFIX2 b (item 8a): gl/sunshadowdyn.h, the Hit restore cut to the dynamic casters' footprint.
 * Green:
 *   A. a triangle's covered texels (pixel centres inside, edges inclusive) all lie in texels(addBox(its view box));
 *      a sphere's points in texels(addSphere(...)), through random orthographic cascade rows;
 *   B. over 600 paints of moving dynamic casters (and static rewrites now and then), the live map built by the cut
 *      restore equals, texel for texel, the old one (whole static copy + the same draws), every paint.
 * Reds (each must FAIL): -DSUNDYN_RED_TRUNC (texels truncated, no margin: A fails) and -DSUNDYN_RED_NORESTORE (the
 * old footprint never copied back: B fails). Usage: sunshadowdyn_test */
#include "gl/sunshadowdyn.h"

#include <cmath>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

static int fails = 0;

static void check( bool ok, const char * what, const std::string & detail = std::string() )
{
	std::printf( "%s %s%s%s\n", ok ? "ok" : "FAIL", what, detail.empty() ? "" : ": ", detail.c_str() );
	if ( !ok )
		fails++;
}

struct V3
{
	float x, y, z;
};

// an orthographic cascade: rotation about a random axis, scale, offset (column-major, x / y / z rows; w = 1)
static void randomOrtho( std::mt19937 & g, float m[16] )
{
	std::uniform_real_distribution<float> u( -1.0f, 1.0f );
	float a[3] = { u( g ), u( g ), u( g ) };
	const float l = std::sqrt( a[0] * a[0] + a[1] * a[1] + a[2] * a[2] ) + 1e-6f;
	for ( float & v : a )
		v /= l;
	const float t = 3.14159f * u( g ), c = std::cos( t ), s = std::sin( t ), k = 1.0f - c;
	const float R[3][3] = { { c + a[0] * a[0] * k, a[0] * a[1] * k - a[2] * s, a[0] * a[2] * k + a[1] * s },
		{ a[1] * a[0] * k + a[2] * s, c + a[1] * a[1] * k, a[1] * a[2] * k - a[0] * s },
		{ a[2] * a[0] * k - a[1] * s, a[2] * a[1] * k + a[0] * s, c + a[2] * a[2] * k } };
	const float sc = 1.0f / ( 200.0f + 800.0f * ( u( g ) * 0.5f + 0.5f ) );
	for ( int col = 0; col < 3; col++ )
		for ( int row = 0; row < 3; row++ )
			m[col * 4 + row] = R[row][col] * sc;
	m[12] = 0.3f * u( g );
	m[13] = 0.3f * u( g );
	m[14] = 0.0f;
	m[3] = m[7] = m[11] = 0.0f;
	m[15] = 1.0f;
}

static void project( const float m[16], const V3 & p, int vw, int vh, double & wx, double & wy, double & wz )
{
	const double x = double( m[0] ) * p.x + double( m[4] ) * p.y + double( m[8] ) * p.z + m[12];
	const double y = double( m[1] ) * p.x + double( m[5] ) * p.y + double( m[9] ) * p.z + m[13];
	const double z = double( m[2] ) * p.x + double( m[6] ) * p.y + double( m[10] ) * p.z + m[14];
	wx = ( x * 0.5 + 0.5 ) * vw;
	wy = ( y * 0.5 + 0.5 ) * vh;
	wz = z;
}

// covered texels of a triangle: pixel centre inside or on an edge (more than GL's rule: conservative for the test)
template <typename F>
static void raster( const float m[16], const V3 t[3], int vw, int vh, F f )
{
	double X[3], Y[3], Z[3];
	for ( int i = 0; i < 3; i++ )
		project( m, t[i], vw, vh, X[i], Y[i], Z[i] );
	const double area = ( X[1] - X[0] ) * ( Y[2] - Y[0] ) - ( X[2] - X[0] ) * ( Y[1] - Y[0] );
	if ( std::fabs( area ) < 1e-12 )
		return;
	const int x0 = std::max( 0, int( std::floor( std::min( { X[0], X[1], X[2] } ) ) ) - 1 );
	const int x1 = std::min( vw - 1, int( std::ceil( std::max( { X[0], X[1], X[2] } ) ) ) + 1 );
	const int y0 = std::max( 0, int( std::floor( std::min( { Y[0], Y[1], Y[2] } ) ) ) - 1 );
	const int y1 = std::min( vh - 1, int( std::ceil( std::max( { Y[0], Y[1], Y[2] } ) ) ) + 1 );
	for ( int py = y0; py <= y1; py++ ) {
		for ( int px = x0; px <= x1; px++ ) {
			const double cx = px + 0.5, cy = py + 0.5;
			const double w0 = ( ( X[1] - cx ) * ( Y[2] - cy ) - ( X[2] - cx ) * ( Y[1] - cy ) ) / area;
			const double w1 = ( ( X[2] - cx ) * ( Y[0] - cy ) - ( X[0] - cx ) * ( Y[2] - cy ) ) / area;
			const double w2 = 1.0 - w0 - w1;
			if ( w0 >= -1e-9 && w1 >= -1e-9 && w2 >= -1e-9 )
				f( px, py, w0 * Z[0] + w1 * Z[1] + w2 * Z[2] );
		}
	}
}

static bool inRect( const WwSunDyn::Rect & r, int x, int y )
{
	return x >= r.x0 && x < r.x1 && y >= r.y0 && y < r.y1;
}

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	std::mt19937 g( 0x8a8au );
	std::uniform_real_distribution<float> u( -1.0f, 1.0f ), u01( 0.0f, 1.0f );
	const int vw = 240, vh = 200;

	// A. footprints cover the raster
	long missed = 0, covered = 0;
	for ( int n = 0; n < 3000; n++ ) {
		float m[16];
		randomOrtho( g, m );
		const float half = 2.0f + 300.0f * u01( g );
		const V3 c { 400.0f * u( g ), 400.0f * u( g ), 400.0f * u( g ) };
		float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
		std::vector<V3> tri;
		for ( int k = 0; k < 24; k++ ) {
			const V3 p { c.x + half * u( g ), c.y + half * u( g ), c.z + half * u( g ) };
			tri.push_back( p );
			lo[0] = std::min( lo[0], p.x );
			lo[1] = std::min( lo[1], p.y );
			lo[2] = std::min( lo[2], p.z );
			hi[0] = std::max( hi[0], p.x );
			hi[1] = std::max( hi[1], p.y );
			hi[2] = std::max( hi[2], p.z );
		}
		WwSunDyn::Ndc nd;
		WwSunDyn::addBox( nd, m, lo, hi, 0.0f );
		const WwSunDyn::Rect r = WwSunDyn::texels( nd, vw, vh );
		for ( size_t k = 0; k + 2 < tri.size(); k += 3 ) {
			const V3 t3[3] = { tri[k], tri[k + 1], tri[k + 2] };
			raster( m, t3, vw, vh, [&]( int x, int y, double ) {
				covered++;
				missed += inRect( r, x, y ) ? 0 : 1;
			} );
		}
		// a sphere's points (a far tile)
		WwSunDyn::Ndc ns;
		const float cs[3] = { c.x, c.y, c.z };
		WwSunDyn::addSphere( ns, m, cs, half );
		const WwSunDyn::Rect rs = WwSunDyn::texels( ns, vw, vh );
		for ( int k = 0; k < 64; k++ ) {
			V3 d { u( g ), u( g ), u( g ) };
			const float l = std::sqrt( d.x * d.x + d.y * d.y + d.z * d.z ) + 1e-6f;
			const V3 p { c.x + d.x / l * half, c.y + d.y / l * half, c.z + d.z / l * half };
			double wx, wy, wz;
			project( m, p, vw, vh, wx, wy, wz );
			const int px = int( std::floor( wx ) ), py = int( std::floor( wy ) );	// the texel the point lies in
			if ( px >= 0 && py >= 0 && px < vw && py < vh ) {
				covered++;
				missed += inRect( rs, px, py ) ? 0 : 1;
			}
		}
	}
	check( missed == 0, "every covered texel inside the footprint", std::to_string( missed ) + " of " + std::to_string( covered ) );

	// B. the cut restore equals the whole copy, paint by paint
	{
		const int W = 96, H = 80;
		std::vector<float> S( size_t( W * H ) ), liveOld, liveNew( size_t( W * H ), 1.0f );
		float m[16];
		randomOrtho( g, m );
		auto drawTris = [&]( std::vector<float> & map, const std::vector<V3> & tris ) {
			for ( size_t k = 0; k + 2 < tris.size(); k += 3 ) {
				const V3 t3[3] = { tris[k], tris[k + 1], tris[k + 2] };
				raster( m, t3, W, H, [&]( int x, int y, double z ) {
					float & d = map[size_t( y * W + x )];
					d = std::min( d, float( z ) );
				} );
			}
		};
		auto staticLayer = [&]() {
			std::fill( S.begin(), S.end(), 1.0f );
			std::vector<V3> st;
			for ( int k = 0; k < 60; k++ )
				st.push_back( { 500.0f * u( g ), 500.0f * u( g ), 300.0f * u( g ) } );
			drawTris( S, st );
		};
		staticLayer();
		WwSunDyn::Live live;
		unsigned long long gen = 1, key = 7;
		long badTexels = 0, badPaints = 0, partial = 0;
		V3 actor { 0, 0, 0 };
		for ( int paint = 0; paint < 600; paint++ ) {
			bool full = paint == 0;
			if ( paint % 97 == 50 ) {	// a static rewrite (Full): a new key and layer
				staticLayer();
				gen++;
				key++;
				full = true;
			}
			// the dynamic casters of this paint: an actor walking, sometimes none
			actor = { actor.x + 9.0f * u( g ), actor.y + 9.0f * u( g ), 20.0f * u( g ) };
			std::vector<V3> dyn;
			if ( paint % 13 != 5 )
				for ( int k = 0; k < 18; k++ )
					dyn.push_back( { actor.x + 40.0f * u( g ), actor.y + 40.0f * u( g ), actor.z - 100.0f + 40.0f * u( g ) } );
			// old: the whole static copy, then the dynamic casters
			liveOld = S;
			drawTris( liveOld, dyn );
			// new
			if ( !full && WwSunDyn::partialOk( live, gen, key, false ) ) {
				partial++;
#ifndef SUNDYN_RED_NORESTORE
				for ( int y = live.dirty.y0; y < live.dirty.y1; y++ )
					for ( int x = live.dirty.x0; x < live.dirty.x1; x++ )
						liveNew[size_t( y * W + x )] = S[size_t( y * W + x )];
#endif
			} else {
				liveNew = S;
			}
			drawTris( liveNew, dyn );
			WwSunDyn::Ndc nd;
			if ( !dyn.empty() ) {
				float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f };
				for ( const V3 & p : dyn ) {
					lo[0] = std::min( lo[0], p.x );
					lo[1] = std::min( lo[1], p.y );
					lo[2] = std::min( lo[2], p.z );
					hi[0] = std::max( hi[0], p.x );
					hi[1] = std::max( hi[1], p.y );
					hi[2] = std::max( hi[2], p.z );
				}
				WwSunDyn::addBox( nd, m, lo, hi, 0.0f );
			}
			live.known = true;
			live.gen = gen;
			live.key = key;
			live.dirty = WwSunDyn::texels( nd, W, H );
			long bad = 0;
			for ( size_t k = 0; k < S.size(); k++ )
				bad += liveOld[k] != liveNew[k] ? 1 : 0;
			badTexels += bad;
			badPaints += bad ? 1 : 0;
		}
		check( partial > 400, "most paints take the cut restore", std::to_string( partial ) + " of 600" );
		check( badTexels == 0, "the live map equals the whole-copy one every paint",
			std::to_string( badTexels ) + " texels in " + std::to_string( badPaints ) + " paints differ" );
	}

	// C. the bookkeeping: a rewritten static layer or a new key forces the whole copy
	{
		WwSunDyn::Live l;
		l.known = true;
		l.gen = 3;
		l.key = 9;
		check( WwSunDyn::partialOk( l, 3, 9, false ) && !WwSunDyn::partialOk( l, 4, 9, false )
			&& !WwSunDyn::partialOk( l, 3, 8, false ) && !WwSunDyn::partialOk( l, 3, 9, true ),
			"partialOk only for the same static write and key, never under the red" );
		WwSunDyn::Ndc none;
		check( WwSunDyn::texels( none, 100, 100 ).empty(), "no dynamic caster: an empty footprint" );
	}
	std::printf( "SUNSHADOWDYN %s (%d failed)\n", fails ? "FAIL" : "PASS", fails );
	return fails ? 1 : 0;
}
