/* lane PERFFIX3 D (2026-11-01): the CPU side of the GPU precipitation field, printed for tests/features/weather/precipgpu/precipgpu_check.py.
 * usage: precipgpu_dump <type> <sizeMm> <rateMmH> <seed> <t> <camx> <camy> <camz> <windx> <windy> <far 0|1> <drawScale>
 *        <spin> <sway> <windShare> <weight>
 * prints one `run` line (precipmath.h gpuRun: the uniforms the shader gets) then one `p` line per particle of the tier
 * (particleAt: the reference): i inside relx rely relz velx vely velz half angle variant. Every number %.17g. */
#include "features/weather/precipmath.h"

#include <cstdio>
#include <cstdlib>

using namespace WwPrecip;

int main( int argc, char ** argv )
{
	std::setvbuf( stdout, nullptr, _IOFBF, 1 << 20 );
	if ( argc < 17 ) {
		std::fprintf( stderr, "usage: see the header\n" );
		return 2;
	}
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
	const TypeInfo & ti = typeInfo( la.type );
	std::printf( "run %u %.17g %.17g %.17g %.17g %.17g", unsigned( g.seedS ), g.L, g.inner, g.thin, g.offXY[0], g.offXY[1] );
	for ( double v : g.offZ )
		std::printf( " %.17g", v );
	std::printf( " %d %d", g.nCand, g.best );
	for ( int v : g.cand )
		std::printf( " %d", v );
	for ( double v : g.fallU )
		std::printf( " %.17g", v );
	for ( double v : g.halfU )
		std::printf( " %.17g", v );
	std::printf( " %.17g %.17g %.17g %.17g %ld %d %d\n", g.wind[0], g.wind[1], g.sway, g.spin, g.count, ti.cols, ti.cols * ti.rows );
	for ( long i = 0; i < n; i++ ) {
		const Particle p = particleAt( la, b, i, farTier, t, cam, wv );
		std::printf( "p %ld %d %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %d\n", i, p.inside ? 1 : 0, p.pos[0] - cam[0],
			p.pos[1] - cam[1], p.pos[2] - cam[2], p.vel[0], p.vel[1], p.vel[2], p.half, p.angle, p.variant );
	}
	return 0;
}
