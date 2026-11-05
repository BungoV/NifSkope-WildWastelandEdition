// lane INTSKY1: gl/preciproof.h, the sunlit interior's roof height map (rain only under open sky). A hall 1000 x 600
// with a floor at z 0, walls to z 400, a roof over x < 500 (z 400), open sky over x >= 500, a glass skylight over
// 700..800 (it stops rain too) and a balcony slab at z 200 over 500..600. PRECIPROOF_RED=noroof: the map ignores
// triangles seen from above (corners only) -> the roof's middle reads open and "roofed" FAILs.
#include "features/weather/preciproof.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static int fails = 0;
static void check( const char * name, bool ok, double a = 0, double b = 0 )
{
	std::printf( "%s %s %.3f %.3f\n", ok ? "ok" : "FAIL", name, a, b );
	fails += ok ? 0 : 1;
}

struct Quad
{
	float x0, y0, x1, y1, z;
};

static void addQuad( WwRoofMap & m, const Quad & q, bool red )
{
	const float a[3] = { q.x0, q.y0, q.z }, b[3] = { q.x1, q.y0, q.z }, c[3] = { q.x1, q.y1, q.z }, d[3] = { q.x0, q.y1, q.z };
	if ( red ) {	// corners only
		m.splat( a[0], a[1], a[2] ); m.splat( b[0], b[1], b[2] ); m.splat( c[0], c[1], c[2] ); m.splat( d[0], d[1], d[2] );
		return;
	}
	m.addTri( a, b, c );
	m.addTri( a, c, d );
}

static void addWall( WwRoofMap & m, float x0, float y0, float x1, float y1, float h )
{
	const float a[3] = { x0, y0, 0 }, b[3] = { x1, y1, 0 }, c[3] = { x1, y1, h }, d[3] = { x0, y0, h };
	m.addTri( a, b, c );
	m.addTri( a, c, d );
}

int main()
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	const char * r = std::getenv( "PRECIPROOF_RED" );
	const bool red = r && std::string( r ) == "noroof";
	WwRoofMap m;
	m.reset( 0, 0, 1000, 600, 32.0f );
	addQuad( m, { 0, 0, 1000, 600, 0 }, red );		// floor
	addQuad( m, { 0, 0, 500, 600, 400 }, red );		// roof over the west half
	addQuad( m, { 700, 200, 800, 400, 400 }, red );	// glass skylight
	addQuad( m, { 500, 0, 600, 600, 200 }, red );	// balcony slab
	addWall( m, 0, 0, 1000, 0, 400 );
	addWall( m, 0, 600, 1000, 600, 400 );
	addWall( m, 0, 0, 0, 600, 400 );
	addWall( m, 1000, 0, 1000, 600, 400 );
	auto open = [&]( double x, double y, double z ) {
		const double p[3] = { x, y, z };
		return m.open( p );
	};
	check( "roofed", !open( 250, 300, 100 ) && !open( 250, 300, 390 ), m.roofAt( 250, 300 ) );
	check( "abovetheroof", open( 250, 300, 450 ), m.roofAt( 250, 300 ) );
	check( "opensky", open( 900, 300, 100 ) && open( 650, 300, 10 ), m.roofAt( 900, 300 ) );
	check( "glass", !open( 750, 300, 100 ), m.roofAt( 750, 300 ) );
	check( "underbalcony", !open( 550, 300, 100 ) && open( 550, 300, 250 ), m.roofAt( 550, 300 ) );
	// a vertical wall is no roof: rain just inside the east wall, under the open half, falls
	check( "wallnoroof", open( 1000.0 - 10.0, 300, 100 ), m.roofAt( 990, 300 ) );
	check( "outside", open( 5000, 5000, -100 ) );

	// lane BUGS1102 (PRECIPROOF-GPU): the field the pass picks, with and without a roof map. Under a roof no field may
	// draw a particle; under open sky both do. PRECIPROOF_RED=gpuroof: the GPU field with no roof test (pre-BUGS1102).
	// lane BUGS1102B (PRECIPROOF-GPU2): the GPU field reads the roof map, so a roofed interior keeps the GPU field
	// (pick_roofed); PRECIPROOF_RED=cpuroof = BUGS1102's pick (the CPU field under a roof: same drops, pick_roofed FAILs).
	const bool redGpu = r && std::string( r ) == "gpuroof";
	const bool redCpu = r && std::string( r ) == "cpuroof";
	for ( int cpuForced = 0; cpuForced < 2; cpuForced++ )
		for ( int noWrap = 0; noWrap < 2; noWrap++ ) {
			const bool exterior = wwPrecipGpuField( cpuForced, noWrap, false, redCpu );
			const bool roofed = wwPrecipGpuField( cpuForced, noWrap, true, redCpu );
			const bool noTest = roofed && !wwPrecipGpuRoofTest( roofed, true, redGpu );
			const std::string tag = std::string( cpuForced ? "cpu" : "-" ) + ( noWrap ? "+nowrap" : "" );
			check( ( "pick_exterior_" + tag ).c_str(), exterior == ( !cpuForced && !noWrap ), exterior, 0 );
			check( ( "pick_roofed_" + tag ).c_str(), roofed == ( !cpuForced && !noWrap ), roofed, 0 );
			const double under[3] = { 250, 300, 100 }, sky[3] = { 900, 300, 100 };
			check( ( "gpuroofed_" + tag ).c_str(), !wwPrecipDrawn( &m, noTest, under ), roofed, noTest );
			check( ( "gpuopensky_" + tag ).c_str(), wwPrecipDrawn( &m, noTest, sky ) && wwPrecipDrawn( nullptr, exterior, under ), roofed, noTest );
		}
	// a grid under the roof: the field the pass picks for a roofed interior draws none of it
	{
		const bool gpu = wwPrecipGpuField( false, false, true, redCpu );
		const bool noTest = gpu && !wwPrecipGpuRoofTest( gpu, true, redGpu );
		long under = 0, drawnUnder = 0;
		for ( int y = 20; y < 600; y += 40 )
			for ( int x = 20; x < 500; x += 40 )
				for ( int z = 20; z < 400; z += 60 ) {
					const double p[3] = { double( x ), double( y ), double( z ) };
					under++;
					drawnUnder += wwPrecipDrawn( &m, noTest, p ) ? 1 : 0;
				}
		check( "gpuroofed_grid", drawnUnder == 0, double( drawnUnder ), double( under ) );
	}
	// the GPU texel: open sky uploads as a finite floor every particle is above, a roof as itself
	check( "texel", wwRoofTexel( -INFINITY ) < -1e29f && wwRoofTexel( 400.0f ) == 400.0f, wwRoofTexel( m.roofAt( 900, 300 ) ) );
	std::printf( "%s: %d failed%s\n", fails ? "FAIL" : "PASS", fails, red ? " under red noroof"
		: redGpu ? " under red gpuroof" : redCpu ? " under red cpuroof" : "" );
	return fails ? 1 : 0;
}
