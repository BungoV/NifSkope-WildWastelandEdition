/* lane SHADOWSHIMMER5 (FARTILESPLIT, 2026-11-02): unit for gl/fartile.h, the instanced far rows' cull tile size.
 *
 *   G1 key        WwFarTile::key groups exactly the cells with equal floor(cell / N) (negative cells too); shiftOf maps
 *                 1/2/4/8 cells, unset to the default (2), and the red to 4.
 *   G2 bound      on a synthetic far field (rows of radius 300-900 u, 6 a cell, cells -42..12 x -10..44 round the
 *                 Concord block), tiles built as src/lodinative.cpp builds them (AABB of the row spheres): every row
 *                 sphere lies inside its tile's sphere (the cull stays conservative), and the default's median tile
 *                 radius is <= 0.6 x the 4 x 4 tiles'.
 *   G3 cost       400 random cull boxes (2-16 k a side, a cascade's footprint): the rows a whole-tile cull lets through,
 *                 default / 4 x 4, median <= 0.8; and never a row the box meets left out.
 *   G4 mirror     src/lodinative.cpp keys its rows by WwFarTile::key with the switch's shift, and the old `>> 2` is gone.
 *
 * Red (must FAIL): -DFTS_RED (the default reads as the red: 4 x 4 tiles -- G2's and G3's floors).
 *     bash tests/fartilesplit/run.sh <out dir>
 */
#include "gl/fartile.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

static int g_fail = 0;
static void check( bool ok, const char * what, const std::string & detail )
{
	std::printf( "%s %s (%s)\n", ok ? "ok  " : "FAIL", what, detail.c_str() );
	if ( !ok )
		g_fail++;
}

#ifdef FTS_RED
static const bool kRed = true;
#else
static const bool kRed = false;
#endif

struct Row { double c[3]; double r; int cx, cy; };
struct Tile { double c[3]; double r; std::vector<int> rows; };

static std::vector<Row> field()
{
	std::mt19937 rng( 1102 );
	std::uniform_real_distribution<double> u( 0.0, 1.0 );
	std::vector<Row> rows;
	for ( int cx = -42; cx <= 12; cx++ )
		for ( int cy = -10; cy <= 44; cy++ ) {
			if ( cx >= -17 && cx <= -13 && cy >= 15 && cy <= 19 )
				continue;	// the loaded block has no far rows
			for ( int k = 0; k < 6; k++ ) {
				Row w;
				w.cx = cx;
				w.cy = cy;
				w.c[0] = ( cx + u( rng ) ) * 4096.0;
				w.c[1] = ( cy + u( rng ) ) * 4096.0;
				w.c[2] = 7000.0 + 600.0 * u( rng );
				w.r = 300.0 + 600.0 * u( rng );
				rows.push_back( w );
			}
		}
	return rows;
}

// src/lodinative.cpp: one tile a key, its sphere = the AABB of its rows' spheres
static std::vector<Tile> tiles( const std::vector<Row> & rows, int shift )
{
	std::map<uint32_t, Tile> by;
	std::map<uint32_t, std::pair<std::vector<double>, std::vector<double>>> box;
	for ( int i = 0; i < int( rows.size() ); i++ ) {
		const Row & w = rows[size_t( i )];
		const uint32_t k = WwFarTile::key( w.cx, w.cy, shift );
		auto & b = box[k];
		if ( b.first.empty() ) {
			b.first.assign( 3, 3.4e38 );
			b.second.assign( 3, -3.4e38 );
		}
		for ( int a = 0; a < 3; a++ ) {
			b.first[size_t( a )] = std::min( b.first[size_t( a )], w.c[a] - w.r );
			b.second[size_t( a )] = std::max( b.second[size_t( a )], w.c[a] + w.r );
		}
		by[k].rows.push_back( i );
	}
	std::vector<Tile> out;
	for ( auto & kv : by ) {
		Tile t = kv.second;
		const auto & b = box[kv.first];
		double h2 = 0.0;
		for ( int a = 0; a < 3; a++ ) {
			t.c[a] = 0.5 * ( b.first[size_t( a )] + b.second[size_t( a )] );
			const double h = 0.5 * ( b.second[size_t( a )] - b.first[size_t( a )] );
			h2 += h * h;
		}
		t.r = std::sqrt( h2 );
		out.push_back( t );
	}
	return out;
}

static double median( std::vector<double> v )
{
	std::sort( v.begin(), v.end() );
	return v.empty() ? 0.0 : v[v.size() / 2];
}

static std::string fmt( const char * f, double a, double b = 0, double c = 0 )
{
	char s[256];
	std::snprintf( s, sizeof( s ), f, a, b, c );
	return s;
}

static void gateG1()
{
	int bad = 0;
	for ( int s = 0; s <= 3; s++ )
		for ( int a = -70; a <= 70; a++ )
			for ( int b = -70; b <= 70; b += 7 ) {
				const int n = 1 << s;
				const bool same = int( std::floor( double( a ) / n ) ) == int( std::floor( double( b ) / n ) );
				if ( ( WwFarTile::key( a, 3, s ) == WwFarTile::key( b, 3, s ) ) != same )
					bad++;
				if ( ( WwFarTile::key( 3, a, s ) == WwFarTile::key( 3, b, s ) ) != same )
					bad++;
			}
	check( bad == 0, "G1 key: equal keys exactly for equal floor(cell / N), both axes, negative cells too", fmt( "%.0f mismatches", bad ) );
	const bool map = WwFarTile::shiftOf( 1, false ) == 0 && WwFarTile::shiftOf( 2, false ) == 1
		&& WwFarTile::shiftOf( 4, false ) == 2 && WwFarTile::shiftOf( 8, false ) == 3
		&& WwFarTile::shiftOf( 0, false ) == 1 && WwFarTile::shiftOf( 3, false ) == 1 && WwFarTile::shiftOf( 1, true ) == 2;
	check( map, "G1 shiftOf: 1/2/4/8 cells, unset or other = 2, red = 4", map ? "as written" : "differs" );
}

static int defaultShift() { return WwFarTile::shiftOf( 0, kRed ); }

static void gateG2( const std::vector<Row> & rows )
{
	const std::vector<Tile> t4 = tiles( rows, 2 ), td = tiles( rows, defaultShift() );
	int outside = 0;
	for ( const Tile & t : td )
		for ( int i : t.rows ) {
			const Row & w = rows[size_t( i )];
			const double d = std::sqrt( ( w.c[0] - t.c[0] ) * ( w.c[0] - t.c[0] ) + ( w.c[1] - t.c[1] ) * ( w.c[1] - t.c[1] )
				+ ( w.c[2] - t.c[2] ) * ( w.c[2] - t.c[2] ) );
			if ( d + w.r > t.r * ( 1.0 + 1e-9 ) )
				outside++;
		}
	check( outside == 0, "G2 bound: every row sphere inside its tile sphere (default tiles)", fmt( "%.0f outside", outside ) );
	std::vector<double> r4, rd;
	for ( const Tile & t : t4 )
		r4.push_back( t.r );
	for ( const Tile & t : td )
		rd.push_back( t.r );
	const double k = median( rd ) / median( r4 );
	check( k <= 0.6, "G2 bound: default median tile radius <= 0.6 x the 4 x 4 tiles'",
		fmt( "%.0f / %.0f = %.3f", median( rd ), median( r4 ), k ) + fmt( "; tiles %.0f vs %.0f", double( td.size() ), double( t4.size() ) ) );
}

// a whole-tile cull against an axis-aligned box (xy, the cascade footprint): sphere vs box
static bool meets( const double * c, double r, const double * lo, const double * hi )
{
	double d2 = 0.0;
	for ( int a = 0; a < 2; a++ ) {
		const double q = std::max( lo[a] - c[a], std::max( 0.0, c[a] - hi[a] ) );
		d2 += q * q;
	}
	return d2 <= r * r;
}

static void gateG3( const std::vector<Row> & rows )
{
	const std::vector<Tile> t4 = tiles( rows, 2 ), td = tiles( rows, defaultShift() );
	std::mt19937 rng( 7 );
	std::uniform_real_distribution<double> u( 0.0, 1.0 );
	std::vector<double> ratio;
	int missed = 0;
	for ( int b = 0; b < 400; b++ ) {
		const double s = 2048.0 + 14336.0 * u( rng );
		double lo[2], hi[2];
		lo[0] = ( -42.0 + 55.0 * u( rng ) ) * 4096.0;
		lo[1] = ( -10.0 + 55.0 * u( rng ) ) * 4096.0;
		hi[0] = lo[0] + s;
		hi[1] = lo[1] + s;
		auto through = [&]( const std::vector<Tile> & ts, std::vector<char> & in ) {
			double n = 0;
			in.assign( rows.size(), 0 );
			for ( const Tile & t : ts )
				if ( meets( t.c, t.r, lo, hi ) ) {
					n += double( t.rows.size() );
					for ( int i : t.rows )
						in[size_t( i )] = 1;
				}
			return n;
		};
		std::vector<char> in4, ind;
		const double n4 = through( t4, in4 ), nd = through( td, ind );
		for ( size_t i = 0; i < rows.size(); i++ )
			if ( !ind[i] && meets( rows[i].c, rows[i].r, lo, hi ) )
				missed++;
		if ( n4 > 0 )
			ratio.push_back( nd / n4 );
	}
	check( missed == 0, "G3 cost: no row the box meets left out by the default's tile cull", fmt( "%.0f missed", missed ) );
	const double m = median( ratio );
	check( m <= 0.8, "G3 cost: rows through a whole-tile cull, default / 4 x 4, median of 400 boxes <= 0.8",
		fmt( "median %.3f over %.0f boxes", m, double( ratio.size() ) ) );
}

static std::string readFile( const std::string & p )
{
	std::ifstream f( p, std::ios::binary );
	std::stringstream s;
	s << f.rdbuf();
	return s.str();
}

static size_t countOf( const std::string & s, const std::string & w )
{
	size_t n = 0;
	for ( size_t at = s.find( w ); at != std::string::npos; at = s.find( w, at + 1 ) )
		n++;
	return n;
}

static void gateG4( const std::string & tree )
{
	const std::string src = readFile( tree + "/src/lodinative.cpp" );
	const bool ok = !src.empty() && countOf( src, "const quint32 tile = WwFarTile::key( cellX, cellY, farTileShift );" ) == 1
		&& countOf( src, "WwFarTile::shiftOf( wwEnvInt( \"WW_CELL_FARINST_TILE\" ), wwEnvInt( \"WW_CELL_FARINST_TILE_RED\" ) == 1 )" ) == 1
		&& countOf( src, "( cellX >> 2 ) + 32768" ) == 0;
	check( ok, "G4 mirror: lodinative.cpp keys its rows by WwFarTile::key with the switch's shift", ok ? "found" : "not found" );
}

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	const std::string tree = argc > 1 ? argv[1] : ".";
	const std::vector<Row> rows = field();
	gateG1();
	gateG2( rows );
	gateG3( rows );
	gateG4( tree );
	std::printf( "FARTILESPLIT UNIT %s (%d failed)\n", g_fail ? "FAIL" : "PASS", g_fail );
	return g_fail ? 1 : 0;
}
