/* lane BUGS1102B (PRECIPROOF-GPU2): the CPU side of the GPU field's roof test, for tests/features/weather/precipgpuroof/precipgpuroof_check.py.
 * usage: precipgpuroof_dump <the 16 args of tests/features/weather/precipgpu/precipgpu_dump>          -> its `run` and `p` lines
 *        precipgpuroof_dump <the 16 args> roof                                      -> the fixture roof map and the reference
 * The fixture: a WwRoofMap (gl/preciproof.h) over an off-centre, non-square part of the particle box (some particles fall
 * off the map: open), blocks of 3 x 3 texels each a roof quad at a hashed height across the box's height, 30% left open.
 * Prints `map nx ny camx camy camz inv` (wwRoofCam, the uniforms), `z` lines (one texel row each, wwRoofTexel), then
 * `o i open edge zmargin` per particle: open = WwRoofMap::open of the CPU position (the reference), edge = the distance to
 * the nearest texel edge in units, zmargin = |z - the roof| (1e30 when nothing is over it). */
#define main precipgpu_dump_main
#include "../precipgpu/precipgpu_dump.cpp"
#undef main

#include "features/weather/preciproof.h"

#include <cstring>

static std::uint32_t mix( std::uint32_t x )
{
	x ^= x >> 16; x *= 0x7feb352du; x ^= x >> 15; x *= 0x846ca68bu; x ^= x >> 16;
	return x;
}

int main( int argc, char ** argv )
{
	if ( argc < 18 || std::strcmp( argv[17], "roof" ) != 0 )
		return precipgpu_dump_main( argc, argv );
	std::setvbuf( stdout, nullptr, _IOFBF, 1 << 20 );
	Layer la;
	la.type = std::atoi( argv[1] );
	la.sizeMm = std::atof( argv[2] );
	const double rate = std::atof( argv[3] );
	la.seed = std::uint32_t( std::strtoul( argv[4], nullptr, 10 ) );
	const double t = std::atof( argv[5] );
	const double cam[3] = { std::atof( argv[6] ), std::atof( argv[7] ), std::atof( argv[8] ) };
	const double wv[2] = { std::atof( argv[9] ), std::atof( argv[10] ) };
	const bool farTier = std::atoi( argv[11] ) != 0;
	la.drawScale = std::atof( argv[12] );
	la.spin = std::atof( argv[13] );
	la.swayU = std::atof( argv[14] );
	la.windShare = std::atof( argv[15] );
	const double w = std::atof( argv[16] );
	prepare( la );
	const double dens = densityFromRate( la.type, la.sizeMm, rate );
	const Box nb = nearBox( dens ), fb = farBox( dens, nb );
	const Box & b = farTier ? fb : nb;
	const long n = drawCount( b.count, w );
	const GpuRun g = gpuRun( la, b, farTier, t, cam, wv, n );
	const double L = g.L;

	WwRoofMap m;
	m.reset( float( cam[0] - 0.6 * L ), float( cam[1] - 0.45 * L ), float( cam[0] + 0.35 * L ), float( cam[1] + 0.6 * L ),
		float( L / 40.0 ) );
	const float c3 = 3.0f * m.cell;
	for ( int bj = 0; bj * 3 < m.ny; bj++ )
		for ( int bi = 0; bi * 3 < m.nx; bi++ ) {
			const std::uint32_t h = mix( std::uint32_t( bi * 7919 + bj * 104729 ) ^ la.seed );
			if ( h % 10 < 3 )
				continue;	// open
			const float z = float( cam[2] + ( double( ( h >> 8 ) & 0xFFFF ) / 65535.0 - 0.5 ) * 1.2 * L );
			const float x0 = m.x0 + bi * c3, y0 = m.y0 + bj * c3, x1 = x0 + c3, y1 = y0 + c3;
			const float a[3] = { x0, y0, z }, bb[3] = { x1, y0, z }, c[3] = { x1, y1, z }, d[3] = { x0, y1, z };
			m.addTri( a, bb, c );
			m.addTri( a, c, d );
		}
	float rc[3];
	wwRoofCam( m, cam, rc );
	std::printf( "map %d %d %.9g %.9g %.9g %.9g\n", m.nx, m.ny, rc[0], rc[1], rc[2], 1.0f / m.cell );
	for ( int j = 0; j < m.ny; j++ ) {
		std::printf( "z" );
		for ( int i = 0; i < m.nx; i++ )
			std::printf( " %.9g", wwRoofTexel( m.z[size_t( j ) * size_t( m.nx ) + size_t( i )] ) );
		std::printf( "\n" );
	}
	for ( long i = 0; i < n; i++ ) {
		const Particle p = particleAt( la, b, i, farTier, t, cam, wv );
		const double fx = ( p.pos[0] - double( m.x0 ) ) / m.cell, fy = ( p.pos[1] - double( m.y0 ) ) / m.cell;
		const double ex = std::min( fx - std::floor( fx ), std::ceil( fx ) - fx ), ey = std::min( fy - std::floor( fy ), std::ceil( fy ) - fy );
		const float r = m.roofAt( float( p.pos[0] ), float( p.pos[1] ) );
		std::printf( "o %ld %d %.9g %.9g\n", i, m.open( p.pos ) ? 1 : 0, std::min( ex, ey ) * m.cell,
			std::isinf( r ) ? 1e30 : std::fabs( p.pos[2] - double( r ) ) );
	}
	return 0;
}
