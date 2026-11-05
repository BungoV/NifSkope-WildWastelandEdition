/* lane FIX1101A lodch: the far field's sky term against the near field's GI at the last cascade edge (offline twin).
 *
 * A "uniform facade": a surface under a uniform horizon of elevation b (a street of constant height around it). Near
 * (inside the cascades) the GI volume lights it with the sky it sees, weighted by cos about its NORMAL; past the last
 * cascade the far field's ambient is the weather's ambient times the factor the bake's byte gives. With the GI's open
 * value equal to the weather's ambient (the premise), the luminance step across the edge is |far - near| / near.
 *
 * Independent of src/features/far-lod/lodchsky.h: the ray set is built here again from docs/LODGEN_NATIVE_LODO_LODI.md 4.10 (double),
 * the byte is "cast" here (a ray is open when it is on the outer side of the surface and above the horizon), and the
 * GI's share is a numeric integral over the sphere (2880 x 1440). Only LodchSky::giFactorByte (the law under test) is
 * called. Gate: step < 2 % on every band-edge horizon 0..60 deg for a wall and a roof. Mid-band horizons and a 45 deg
 * slope are printed (the bake's ring resolution, not the law).
 *
 * -DLODCH_RED=1: today's far field (no sky factor: 1); -DLODCH_RED=2: the raw byte as the factor. Both must FAIL. */
#include "features/far-lod/lodchsky.h"

#include <cmath>
#include <cstdio>
#include <vector>

static const double kPi = 3.14159265358979323846;

struct Ray { double d[3], w; };

static std::vector<Ray> bakeRays()
{
	const double bands[8] = { 0, 10, 20, 30, 45, 60, 75, 90 };
	std::vector<Ray> out;
	for ( int k = 0; k < 7; k++ ) {
		const double a = bands[k] * kPi / 180.0, b = bands[k + 1] * kPi / 180.0;
		const double e = std::asin( std::sqrt( ( std::sin( a ) * std::sin( a ) + std::sin( b ) * std::sin( b ) ) / 2.0 ) );
		for ( int j = 0; j < 8; j++ ) {
			const double ph = ( j + ( k % 2 ? 0.5 : 0.0 ) ) * kPi / 4.0;
			Ray r;
			r.d[0] = std::cos( e ) * std::cos( ph );
			r.d[1] = std::cos( e ) * std::sin( ph );
			r.d[2] = std::sin( e );
			r.w = ( std::sin( b ) * std::sin( b ) - std::sin( a ) * std::sin( a ) ) / 8.0;
			out.push_back( r );
		}
	}
	return out;
}

// the bake's byte for a surface with normal (sqrt(1-nz^2), 0, nz) under a uniform horizon (deg)
static int bakedByte( const std::vector<Ray> & R, double nz, double horizonDeg )
{
	const double nx = std::sqrt( std::max( 0.0, 1.0 - nz * nz ) );
	const double hz = std::sin( horizonDeg * kPi / 180.0 );
	double s = 0.0;
	for ( const Ray & r : R ) {
		if ( r.d[0] * nx + r.d[2] * nz <= -1.0e-4 )
			continue;	// behind the surface: its own building
		if ( r.d[2] <= hz )
			continue;	// under the horizon
		s += r.w;
	}
	return int( std::lround( s * 255.0 ) );
}

// the GI's sky share: integral of max(0, d.n) over the sky above the horizon, over the same at horizon 0
static double giShare( double nz, double horizonDeg )
{
	const double nx = std::sqrt( std::max( 0.0, 1.0 - nz * nz ) );
	const int NA = 2880, NE = 1440;
	double I = 0.0, I0 = 0.0;
	const double hb = horizonDeg * kPi / 180.0;
	for ( int j = 0; j < NE; j++ ) {
		const double el = ( j + 0.5 ) / NE * ( kPi / 2.0 );	// the upper hemisphere: under 0 is ground in both
		const double ce = std::cos( el ), se = std::sin( el );
		for ( int i = 0; i < NA; i++ ) {
			const double az = ( i + 0.5 ) / NA * 2.0 * kPi;
			const double dn = ce * std::cos( az ) * nx + se * nz;
			if ( dn <= 0.0 )
				continue;
			const double w = dn * ce;
			I0 += w;
			if ( el > hb )
				I += w;
		}
	}
	return I0 > 0.0 ? I / I0 : 1.0;
}

static double farFactor( int byte, double nz )
{
#if defined( LODCH_RED ) && LODCH_RED == 1
	(void) byte; (void) nz;
	return 1.0;
#elif defined( LODCH_RED ) && LODCH_RED == 2
	(void) nz;
	return byte / 255.0;
#else
	return LodchSky::giFactorByte( byte, float( nz ) );
#endif
}

int main()
{
	const std::vector<Ray> R = bakeRays();
	double wsum = 0.0;
	for ( const Ray & r : R )
		wsum += r.w;
	std::printf( "ray set: %d rays, weight sum %.6f\n", int( R.size() ), wsum );
	int fails = 0, gated = 0;
	double worst = 0.0;
	struct Case { const char * what; double nz; double h; bool gate; };
	const Case cases[] = {
		{ "wall", 0.0, 0, true }, { "wall", 0.0, 10, true }, { "wall", 0.0, 20, true }, { "wall", 0.0, 30, true },
		{ "wall", 0.0, 45, true }, { "wall", 0.0, 60, true },
		{ "roof", 1.0, 0, true }, { "roof", 1.0, 10, true }, { "roof", 1.0, 20, true }, { "roof", 1.0, 30, true },
		{ "roof", 1.0, 45, true }, { "roof", 1.0, 60, true },
		{ "wall mid-band", 0.0, 5, false }, { "wall mid-band", 0.0, 15, false }, { "wall mid-band", 0.0, 25, false },
		{ "wall mid-band", 0.0, 37, false }, { "wall mid-band", 0.0, 52, false },
		{ "slope 45", 0.7071, 0, false }, { "slope 45", 0.7071, 20, false }, { "slope 45", 0.7071, 30, false },
		{ "slope 45", 0.7071, 45, false },
	};
	for ( const Case & c : cases ) {
		const int b = bakedByte( R, c.nz, c.h );
		const double near = giShare( c.nz, c.h );
		const double far = farFactor( b, c.nz );
		// across the edge the GI's share k fades 1 -> 0: L = k near + (1 - k) far; the step is the far end's
		const double step = std::fabs( far - near ) / std::max( near, 1.0e-6 );
		const bool bad = c.gate && step >= 0.02;
		if ( c.gate ) {
			gated++;
			worst = std::max( worst, step );
		}
		fails += bad ? 1 : 0;
		std::printf( "%-14s nz %.3f horizon %4.1f deg: byte %3d, near (GI) %.4f, far %.4f, step %6.2f %%%s\n", c.what,
			c.nz, c.h, b, near, far, 100.0 * step, c.gate ? ( bad ? "  FAIL" : "  ok" ) : "  (not gated)" );
	}
	std::printf( "LODCHSKY %s: %d of %d gated facades step >= 2 %%, worst gated step %.2f %%\n", fails ? "FAIL" : "PASS",
		fails, gated, 100.0 * worst );
	return fails ? 1 : 0;
}
