/* lane FO76WEATHER2: unit test of src/gl/rainwetlaw.h exposed (the rain wetness's cover test over preciproof.h's
 * roof map). Build: g++ -std=c++17 -I<tree>/src tests/raincover/raincover.cpp. Red: -DRAINCOVER_RED = the base
 * (FO76WEATHER1: no cover test, every pixel open), which must fail. The fixture is built here (no game files):
 * a flat roof, a bridge deck, a sloped roof, a wall under an eave and a wall in the open, at 16 u texels. */
#include "gl/rainwetlaw.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
int fails = 0, checks = 0;

void check( const char * what, double got, double want, double tol = 1e-5 )
{
	checks++;
	const bool ok = std::fabs( got - want ) <= tol;
	if ( !ok )
		fails++;
	std::printf( "%s %-46s got %.4f want %.4f\n", ok ? "ok  " : "FAIL", what, got, want );
}

void quad( WwRoofMap & m, const float a[3], const float b[3], const float c[3], const float d[3] )
{
	m.addTri( a, b, c );
	m.addTri( a, c, d );
}

void quadXY( WwRoofMap & m, float x0, float y0, float x1, float y1, float z )
{
	const float a[3] = { x0, y0, z }, b[3] = { x1, y0, z }, c[3] = { x1, y1, z }, d[3] = { x0, y1, z };
	quad( m, a, b, c, d );
}

// a vertical wall in the plane x = x, y0..y1, z0..z1 (seen edge-on from above)
void wallX( WwRoofMap & m, float x, float y0, float y1, float z0, float z1 )
{
	const float a[3] = { x, y0, z0 }, b[3] = { x, y1, z0 }, c[3] = { x, y1, z1 }, d[3] = { x, y0, z1 };
	quad( m, a, b, c, d );
}

float expo( const WwRoofMap & m, float x, float y, float z, float nx = 0.0f, float ny = 0.0f, float nz = 1.0f )
{
	const float l = std::sqrt( nx * nx + ny * ny + nz * nz );
	const float p[3] = { x, y, z }, n[3] = { nx / l, ny / l, nz / l };
#ifdef RAINCOVER_RED
	(void)m; (void)p; (void)n;
	return 1.0f;
#else
	return WwRainWet::exposed( m, p, n );
#endif
}

WwRoofMap fixture()
{
	WwRoofMap m;
	m.reset( -1000.0f, -1000.0f, 6000.0f, 6000.0f, 16.0f, 1024 );
	quadXY( m, 0.0f, 0.0f, 512.0f, 512.0f, 300.0f );	// a flat roof at 300
	quadXY( m, 1000.0f, -500.0f, 1200.0f, 1500.0f, 600.0f );	// a bridge deck at 600
	{	// a sloped roof: 400 at x 2000 up to 600 at x 2256
		const float a[3] = { 2000.0f, 0.0f, 400.0f }, b[3] = { 2256.0f, 0.0f, 600.0f }, c[3] = { 2256.0f, 256.0f, 600.0f },
			d[3] = { 2000.0f, 256.0f, 400.0f };
		quad( m, a, b, c, d );
	}
	wallX( m, 3000.0f, 0.0f, 256.0f, 0.0f, 300.0f );	// a wall facing +x under an eave
	quadXY( m, 2900.0f, 0.0f, 3064.0f, 256.0f, 320.0f );	// the eave: 64 u past the wall
	wallX( m, 4000.0f, 0.0f, 256.0f, 0.0f, 300.0f );	// a wall in the open
	return m;
}

/* `dump`: the map (header "map nx ny x0 y0 cell", then ny rows of z, open written as wwRoofTexel writes it) and N
 * points "p x y z nx ny nz exposed margin" for tests/raincover/raincover_gpu.py; margin = the least distance of a
 * tapped texel's height from the open line (a test that close may flip in float on the GPU). */
void dump( const WwRoofMap & m )
{
	std::printf( "map %d %d %.6f %.6f %.6f\n", m.nx, m.ny, double( m.x0 ), double( m.y0 ), double( m.cell ) );
	for ( int j = 0; j < m.ny; j++ ) {
		std::printf( "z" );
		for ( int i = 0; i < m.nx; i++ )
			std::printf( " %.9g", double( wwRoofTexel( m.z[size_t( j ) * size_t( m.nx ) + size_t( i )] ) ) );
		std::printf( "\n" );
	}
	unsigned s = 12345u;
	auto rnd = [&s]() { s = s * 1664525u + 1013904223u; return float( s >> 8 ) / float( 1u << 24 ); };
	for ( int k = 0; k < 40000; k++ ) {
		const float x = -200.0f + rnd() * 4600.0f, y = -600.0f + rnd() * 2300.0f;
		const float zs[4] = { 0.0f, 150.0f, 300.0f, 600.0f };
		const float z = zs[k % 4] + ( rnd() - 0.5f ) * 40.0f;
		float n[3] = { 0.0f, 0.0f, 1.0f };
		if ( k % 3 == 1 ) {	// a wall: a horizontal normal
			const float a = rnd() * 6.2831853f;
			n[0] = std::cos( a ); n[1] = std::sin( a ); n[2] = 0.0f;
		} else if ( k % 3 == 2 ) {	// a slope
			const float a = rnd() * 6.2831853f, t = rnd() * 0.8f;
			n[0] = std::cos( a ) * t; n[1] = std::sin( a ) * t; n[2] = std::sqrt( 1.0f - t * t );
		}
		const float p[3] = { x, y, z };
		const float e = WwRainWet::exposed( m, p, n );
		const float qx = ( p[0] + n[0] * WwRainWet::kCoverStep * m.cell - m.x0 ) / m.cell - 0.5f;
		const float qy = ( p[1] + n[1] * WwRainWet::kCoverStep * m.cell - m.y0 ) / m.cell - 0.5f;
		const int i = int( std::floor( qx ) ), j = int( std::floor( qy ) );
		const float line = z + WwRainWet::coverBias( m.cell, n[0], n[1], n[2] );
		float margin = 1e30f;
		for ( int dj = 0; dj < 2; dj++ )
			for ( int di = 0; di < 2; di++ )
				if ( i + di >= 0 && j + dj >= 0 && i + di < m.nx && j + dj < m.ny )
					margin = std::min( margin, std::fabs( m.z[size_t( j + dj ) * size_t( m.nx ) + size_t( i + di )] - line ) );
		std::printf( "p %.6f %.6f %.6f %.7f %.7f %.7f %.7f %.6g\n", double( x ), double( y ), double( z ), double( n[0] ),
			double( n[1] ), double( n[2] ), double( e ), double( margin ) );
	}
}
}	// namespace

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IONBF, 0 );
	if ( argc > 1 && std::strcmp( argv[1], "dump" ) == 0 ) {
		dump( fixture() );
		return 0;
	}
	const WwRoofMap m = fixture();
	check( "texel side", m.cell, 16.0 );

	// covered: dry
	check( "ground under the flat roof", expo( m, 256.0f, 256.0f, 0.0f ), 0.0 );
	check( "floor just inside the roof's edge", expo( m, 464.0f, 256.0f, 0.0f ), 0.0 );
	check( "ground under the bridge", expo( m, 1100.0f, 500.0f, 0.0f ), 0.0 );
	check( "ground under the sloped roof", expo( m, 2128.0f, 128.0f, 0.0f ), 0.0 );
	check( "wall face under the eave", expo( m, 3000.0f, 128.0f, 150.0f, 1.0f, 0.0f, 0.0f ), 0.0 );
	check( "ground under the eave", expo( m, 3032.0f, 128.0f, 0.0f ), 0.0 );
	// open: wet
	check( "open ground", expo( m, 700.0f, 700.0f, 0.0f ), 1.0 );
	check( "the flat roof's top", expo( m, 256.0f, 256.0f, 300.0f ), 1.0 );
	check( "the bridge deck's top", expo( m, 1100.0f, 500.0f, 600.0f ), 1.0 );
	check( "ground past the bridge's end", expo( m, 1100.0f, 1600.0f, 0.0f ), 1.0 );
	{
		const float nx = -200.0f / 256.0f;	// the sloped roof's normal (downhill = -x)
		check( "the sloped roof's top", expo( m, 2128.0f, 128.0f, 400.0f + 128.0f * 200.0f / 256.0f, nx, 0.0f, 1.0f ), 1.0 );
	}
	check( "the eave's top", expo( m, 3032.0f, 128.0f, 320.0f ), 1.0 );
	check( "open wall face", expo( m, 4000.0f, 128.0f, 150.0f, 1.0f, 0.0f, 0.0f ), 1.0 );
	check( "ground 64 u past the open wall", expo( m, 4064.0f, 128.0f, 0.0f ), 1.0 );
	check( "off the map", expo( m, 9000.0f, 9000.0f, 0.0f ), 1.0 );
	// an edge blends (bilinear over 4 texels): between, never outside 0..1
	{
		const float e = expo( m, 520.0f, 256.0f, 0.0f );
		check( "roof edge blends within 0..1", e >= 0.0f && e <= 1.0f ? 1.0 : 0.0, 1.0 );
	}
	// no map: everything open (the shader's rainWet.w = 0)
	WwRoofMap none;
	check( "no map is open", expo( none, 256.0f, 256.0f, 0.0f ), 1.0 );
	std::printf( "%d of %d failed\n", fails, checks );
	return fails ? 1 : 0;
}
