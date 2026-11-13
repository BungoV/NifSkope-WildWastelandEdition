// lane WEATHERLOOK1 phase B: the precipitation field's unit test (src/features/weather/precipmath.h is the code the cell view draws
// with; src/features/weather/precipspec.h its .recx parser). One test per rule; PRECIP_RED=<bits> runs the same exe with a rule
// removed (precipmath.h Red: 1 constdensity, 2 nowrap, 4 onespeed, 8 nowind; PRECIPLOOK1: 16 = the look's old path,
// rule 6) and that rule's test must FAIL. Rule 7 (the drop phase table vs its Monte Carlo fixture) has no red.
// The expected numbers are airpart_data.md's table rows (notes/weatherlook1), typed here, not computed from the laws.
#include "features/weather/precipmath.h"
#include "features/weather/precipspec.h"

#include <QJsonDocument>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

#ifndef PRECIP_ROOT
#define PRECIP_ROOT "."	// the tree (run.sh passes it)
#endif

using namespace WwPrecip;

static int g_fail = 0, g_checks = 0;
static int g_failRule[8] = {};

static void check( int rule, bool ok, const char * what, double got, double want )
{
	g_checks++;
	if ( !ok ) {
		g_fail++;
		g_failRule[rule]++;
	}
	std::printf( "%s rule%d %s got %.6g want %.6g\n", ok ? "ok  " : "FAIL", rule, what, got, want );
}

static bool within( double got, double want, double rel )
{
	return std::fabs( got - want ) <= rel * std::fabs( want );
}

int main()
{
	if ( const char * r = std::getenv( "PRECIP_RED" ) )
		redBits() = std::atoi( r );
	std::printf( "precip unit: red bits %d\n", redBits() );

	// ---- rule 1: fall speed per type = airpart_data.md (m/s), within 7 % (WEATHERRECX2: the one law's stated tolerance;
	// 5 % while the per-type switch drew it)
	const double kRule1Tol = 0.07;
	struct Row { int type; double D, v; };
	const Row speeds[] = {
		{ Rain, 0.5, 2.02 }, { Rain, 1, 4.00 }, { Rain, 2, 6.55 }, { Rain, 3, 7.95 }, { Rain, 5, 9.14 }, { Rain, 6, 9.17 },
		{ SnowFine, 2, 0.89 }, { SnowFine, 3, 0.95 }, { SnowFine, 5, 1.03 },
		{ SnowWetClump, 6, 1.28 }, { SnowWetClump, 10, 1.47 }, { SnowWetClump, 12, 1.55 }, { SnowWetClump, 20, 1.77 },
		{ Hail, 5, 9.04 }, { Hail, 10, 12.79 }, { Hail, 20, 18.08 }, { Hail, 30, 22.15 }, { Hail, 50, 28.59 },
		{ SnowBigClump, 20, 1.3 }, { SnowBigClump, 50, 1.5 } };
	char what[160];
	for ( const Row & r : speeds ) {
		std::snprintf( what, sizeof what, "fall %s %.1f mm (m/s)", typeInfo( r.type ).name, r.D );
		check( 1, within( fallSpeedMps( r.type, r.D ), r.v, kRule1Tol ), what, fallSpeedMps( r.type, r.D ), r.v );
	}
	// the field moves each particle at its own variant's speed: measured from two times, no wrap in between
	{
		const double cam[3] = { 0, 0, 0 }, wv[2] = { 0, 0 };
		// sizes an atlas cell has: the table's row where one exists, else (wet 11 mm, hail 12 mm) the table's law at that
		// size, written out (0.79 x 11^0.27; sqrt(4 x 9.81 x 900 x 0.012 / (3 x 0.6 x 1.2)))
		const struct { int type; double size, v; } fieldRows[] = { { Rain, 2.0, 6.55 }, { SnowFine, 3.0, 0.95 },
			{ SnowWetClump, 11.0, 1.516 }, { Hail, 12.0, 14.01 }, { SnowBigClump, 20.0, 1.3 } };
		for ( const auto & fr : fieldRows ) {
			Layer la;
			la.type = fr.type;
			la.sizeMm = fr.size;
			const Box b = nearBox( 50.0 );
			double sum = 0.0;
			int n = 0;
			for ( long i = 0; i < b.count && n < 200; i++ ) {
				const Particle p0 = particleAt( la, b, i, false, 10.0, cam, wv );
				if ( std::fabs( typeInfo( fr.type ).v[p0.variant].sizeMm - fr.size ) > 1e-9 )
					continue;
				const double dt = 1e-3;
				const Particle p1 = particleAt( la, b, i, false, 10.0 + dt, cam, wv );
				const double dz = p1.pos[2] - p0.pos[2];
				if ( std::fabs( dz ) > 0.5 * b.side )
					continue;	// wrapped in between
				sum += -dz / dt / kUnitsPerMetre;
				n++;
			}
			const double got = n ? sum / n : 0.0;
			std::snprintf( what, sizeof what, "field fall %s %.1f mm over %d particles (m/s)", typeInfo( fr.type ).name, fr.size, n );
			check( 1, n > 0 && within( got, fr.v, kRule1Tol ), what, got, fr.v );
		}
	}

	// ---- rule 2: number density per type = rate x 277.78 / (m v) with airpart_data.md's mass + speed, within 5 %,
	// measured as the particles in each tier over its volume
	{
		const struct { int type; double D, rate, m, v; } rows[] = { { Rain, 2, 4, 4.19, 6.55 }, { SnowFine, 3, 1, 0.34, 0.95 },
			{ SnowWetClump, 10, 2, 2.94, 1.47 }, { Hail, 10, 10, 471, 12.79 },
			{ SnowFarDot, 5, 1, 0.695, 1.03 }, { SnowBigClump, 50, 1, 17.5, 1.5 } };
		const double cam[3] = { 1234.5, -987.0, 333.0 }, wv[2] = { 80.0, -40.0 };
		for ( const auto & r : rows ) {
			// WEATHERRECX2: the drawn speed is the one law (rule 1 holds it to the table within 7 %); density follows it
			const double want = r.rate * 277.78 / ( massMg( r.type, r.D ) * fallSpeedMps( r.type, r.D ) );
			(void) r.m;
			(void) r.v;
			WwPrecipLayerSpec s = wwPrecipDefault( r.type, QStringLiteral( "test" ) );
			s.sizeMm = r.D;
			s.rateMmH = r.rate;
			const double dens = s.densityM3();
			Layer la;
			la.type = r.type;
			la.sizeMm = r.D;
			la.densityM3 = dens;
			la.seed = 7u + unsigned( r.type );
			const Box nb = nearBox( dens ), fb = farBox( dens, nb );
			long inNear = 0;
			const long nDraw = drawCount( nb.count, 1.0 );
			for ( long i = 0; i < nDraw; i++ ) {
				const Particle p = particleAt( la, nb, i, false, 3.25, cam, wv );
				bool in = true;
				for ( int k = 0; k < 3; k++ )
					in = in && std::fabs( p.pos[k] - cam[k] ) <= 0.5 * nb.side + 1e-6;
				inNear += in ? 1 : 0;
			}
			const double vNear = nb.side * nb.side * nb.side * kM3PerU3;
			const double got = double( inNear ) * nb.thin / vNear;
			std::snprintf( what, sizeof what, "near density %s %.1f mm %.1f mm/h (per m3, box %.0f u, %ld)", typeInfo( r.type ).name,
				r.D, r.rate, nb.side, nb.count );
			check( 2, within( got, want, 0.05 ), what, got, want );
			if ( fb.count > 0 ) {
				double outN = 0.0;
				for ( long i = 0; i < drawCount( fb.count, 1.0 ); i++ ) {
					const Particle p = particleAt( la, fb, i, true, 3.25, cam, wv );
					outN += p.inside ? 0.0 : fb.thin;
				}
				const double vShell = ( fb.side * fb.side * fb.side - fb.inner * fb.inner * fb.inner ) * kM3PerU3;
				std::snprintf( what, sizeof what, "far density %s (per m3, box %.0f u, thin %.2f)", typeInfo( r.type ).name, fb.side, fb.thin );
				check( 2, within( outN / vShell, want, 0.05 ), what, outN / vShell, want );
			}
		}
	}

	// ---- rule 3: the wrap keeps every particle in the box round the moving camera, the count constant
	{
		Layer la;
		la.type = SnowFine;
		la.sizeMm = 5.5;
		la.swayU = 6.0;
		la.spin = 0.5;
		const Box b = nearBox( 300.0 );
		const double wv[2] = { 150.0, 60.0 };
		long worstIn = b.count, worstOut = 0;
		for ( int step = 0; step <= 240; step++ ) {
			const double t = step * 0.25;
			const double cam[3] = { 37.0 * t, -11.0 * t, 5.0 * std::sin( t ) };
			long in = 0;
			for ( long i = 0; i < b.count; i++ ) {
				const Particle p = particleAt( la, b, i, false, t, cam, wv );
				bool ok = true;
				for ( int k = 0; k < 3; k++ )
					ok = ok && p.pos[k] >= cam[k] - 0.5 * b.side - 1e-6 && p.pos[k] < cam[k] + 0.5 * b.side + 1e-6;
				in += ok ? 1 : 0;
			}
			worstIn = std::min( worstIn, in );
			worstOut = std::max( worstOut, b.count - in );
		}
		check( 3, worstOut == 0, "wrap: particles outside the box at the worst of 241 steps (60 s, camera moving)",
			double( worstOut ), 0.0 );
		check( 3, worstIn == b.count, "wrap: count inside at the worst step", double( worstIn ), double( b.count ) );
	}

	// ---- rule 4: a weather blend moves the density monotonically, from the leaving weather's to the new one's
	{
		auto drawn = []( double dens, double w ) {
			const Box b = nearBox( dens );
			return double( drawCount( b.count, w ) ) * b.thin / ( b.side * b.side * b.side * kM3PerU3 );
		};
		WwPrecipLayerSpec heavy = wwPrecipDefault( Rain, QStringLiteral( "test" ) ), light = heavy;
		heavy.rateMmH = 8.0;
		light.rateMmH = 1.0;
		const double dHeavy = heavy.densityM3(), dLight = light.densityM3();
		// clear -> rain: the rain layer at weight pct
		double prev = -1.0;
		bool up = true;
		for ( int k = 0; k <= 20; k++ ) {
			const double d = drawn( dHeavy, blendWeight( false, k / 20.0 ) );
			up = up && d > prev;
			prev = d;
		}
		check( 4, up, "blend clear -> rain: density rises at every one of 21 steps (1 = yes)", up ? 1.0 : 0.0, 1.0 );
		check( 4, drawn( dHeavy, blendWeight( false, 0.0 ) ) == 0.0, "blend clear -> rain at pct 0 (per m3)",
			drawn( dHeavy, blendWeight( false, 0.0 ) ), 0.0 );
		// heavy rain -> light rain: the leaving heavy layer at 1 - pct plus the light one at pct
		prev = 1e30;
		bool down = true;
		for ( int k = 0; k <= 20; k++ ) {
			const double p = k / 20.0;
			const double d = drawn( dHeavy, blendWeight( true, p ) ) + drawn( dLight, blendWeight( false, p ) );
			down = down && d < prev;
			prev = d;
		}
		check( 4, down, "blend heavy -> light rain: density falls at every one of 21 steps (1 = yes)", down ? 1.0 : 0.0, 1.0 );
		check( 4, within( prev, dLight, 0.05 ), "blend heavy -> light rain at pct 1 = the light weather (per m3)", prev, dLight );
	}

	// ---- rule 5: the wind carries every layer at 400 units/s x speed / 255 along the game bearing
	{
		Layer la;
		la.type = Rain;
		la.sizeMm = 2.0;
		const Box b = nearBox( 40.0 );
		const double cam[3] = { 0, 0, 0 };
		const double ang = 2.0 * 3.14159265358979 * 64.0 / 255.0;
		double wv[2];
		windVelocity( 128.0, ang, wv );
		const double want = 400.0 * 128.0 / 255.0;
		double sx = 0.0, sy = 0.0;
		int n = 0;
		for ( long i = 0; i < b.count && n < 300; i++ ) {
			const Particle p0 = particleAt( la, b, i, false, 4.0, cam, wv ), p1 = particleAt( la, b, i, false, 4.001, cam, wv );
			const double dx = p1.pos[0] - p0.pos[0], dy = p1.pos[1] - p0.pos[1];
			if ( std::fabs( dx ) > 0.5 * b.side || std::fabs( dy ) > 0.5 * b.side || std::fabs( p1.pos[2] - p0.pos[2] ) > 0.5 * b.side )
				continue;
			sx += dx / 0.001;
			sy += dy / 0.001;
			n++;
		}
		sx /= std::max( n, 1 );
		sy /= std::max( n, 1 );
		check( 5, within( std::hypot( sx, sy ), want, 0.01 ), "wind drift speed (units/s)", std::hypot( sx, sy ), want );
		const double got = std::atan2( sy, sx );
		check( 5, std::fabs( got - ang ) < 0.01, "wind drift bearing (radians, 0 = +X counter-clockwise)", got, ang );
	}

	// ---- the prepared fast path (what the view draws with) = the slow path, bit for bit; and its cost per frame
	{
		int diff = 0;
		const double cam[3] = { 10.0, 20.0, 30.0 }, wv[2] = { 33.0, -12.0 };
		double ms = 0.0;
		for ( int ty = 0; ty < TypeCount; ty++ ) {
			Layer slow;
			slow.type = ty;
			slow.sizeMm = typeInfo( ty ).sizeMm;
			slow.drawScale = typeInfo( ty ).drawScale;
			slow.spin = typeInfo( ty ).spin;
			slow.swayU = typeInfo( ty ).swayU;
			slow.seed = 99u + unsigned( ty );
			Layer fast = slow;
			prepare( fast );
			const Box nb = nearBox( 200.0 ), fb = farBox( 200.0, nb );
			for ( long i = 0; i < 2000; i++ )
				for ( int tier = 0; tier < 2; tier++ ) {
					const Particle a = particleAt( slow, tier ? fb : nb, i, tier != 0, 7.5, cam, wv );
					const Particle b = particleAt( fast, tier ? fb : nb, i, tier != 0, 7.5, cam, wv );
					diff += std::memcmp( a.pos, b.pos, sizeof a.pos ) || std::memcmp( a.vel, b.vel, sizeof a.vel ) || a.half != b.half
						|| a.angle != b.angle || a.variant != b.variant || a.inside != b.inside;
				}
			if ( ty == SnowFine ) {	// one frame of a dense layer: every near + far particle
				const auto t0 = std::chrono::steady_clock::now();
				double sink = 0.0;
				for ( int tier = 0; tier < 2; tier++ )
					for ( long i = 0; i < ( tier ? fb : nb ).count; i++ )
						sink += particleAt( fast, tier ? fb : nb, i, tier != 0, 7.5, cam, wv ).pos[2];
				ms = std::chrono::duration<double, std::milli>( std::chrono::steady_clock::now() - t0 ).count();
				std::printf( "cost: one frame of snow_fine at 200/m3 (%ld near + %ld far) %.2f ms CPU (sink %g)\n", nb.count, fb.count,
					ms, sink );
			}
		}
		check( 0, diff == 0, "prepared path = slow path (particles differing of 6 x 4000)", double( diff ), 0.0 );
	}

	// ---- rule 6 (PRECIPLOOK1): the look's numbers. Red bit 16 = the old path (WW_CELL_PRECIP_LOOK_RED): a box of uniform
	// alpha 4 r^2 / ((2r + l) 2r) over the streak's rectangle, every quad at its atlas cell's size, the two old clump atlases
	{
		const bool red = ( redBits() & 16 ) != 0;
		// the streak's coverage x area = the disc's area x the shutter (pi r^2), for a still drop and a 7.5 cm streak
		const double r = 1.0, lens[] = { 0.0, 2.0, 75.0 };
		for ( double l : lens ) {
			const int n = 1200;
			const double hs = 0.5 * l + r, du = 2.0 * r / n, ds = 2.0 * hs / n;
			double sum = 0.0;
			for ( int i = 0; i < n; i++ )
				for ( int j = 0; j < n; j++ ) {
					const double s = -hs + ( i + 0.5 ) * ds, u = -r + ( j + 0.5 ) * du;
					sum += ( red ? 4.0 * r * r / ( ( 2.0 * r + l ) * 2.0 * r ) : dropCoverage( s, u, r, l ) ) * ds * du;
				}
			std::snprintf( what, sizeof what, "drop coverage integral, r 1 mm, streak %.1f mm (mm^2; pi r^2)", l );
			check( 6, within( sum, 3.14159265358979, 0.01 ), what, sum, 3.14159265358979 );
		}
		// the drawn sizes: a rain quad as wide as its drop; the wet clump drawn from the merged (big-clump) atlas at its size
		for ( int k = 0; k < typeInfo( Rain ).nVariants; k++ ) {
			const Variant & v = typeInfo( Rain ).v[k];
			const double got = v.cellMm * ( red ? 1.0 : lookHalfScale( Rain, k, true, false ) );
			std::snprintf( what, sizeof what, "rain drawn width, %.1f mm drop (mm)", v.sizeMm );
			check( 6, within( got, v.sizeMm, 0.001 ), what, got, v.sizeMm );
		}
		const TypeInfo & wet = typeInfo( SnowWetClump ), &big = typeInfo( SnowBigClump );
		for ( int k = 0; k < wet.nVariants; k++ ) {
			// the shape fills big.size / big.cell of a merged cell; the old wet atlas's shape filled wet.size / wet.cell
			const double cellDrawn = wet.v[k].cellMm * ( red ? 1.0 : lookHalfScale( SnowWetClump, k, false, true ) );
			const double got = cellDrawn * big.v[k].sizeMm / big.v[k].cellMm;
			std::snprintf( what, sizeof what, "wet clump drawn from the merged atlas, %.0f mm (mm)", wet.v[k].sizeMm );
			check( 6, within( got, wet.v[k].sizeMm, 0.001 ), what, got, wet.v[k].sizeMm );
		}
		// the merged atlas's fit to the old wet atlas (clump_fit.tsv from notes/preciplook1/tools/clumpmerge.py): the
		// red reads the old wet atlas through the dry threshold's shapes unchanged (its metric distance)
		if ( FILE * f = std::fopen( PRECIP_ROOT "/tests/features/weather/precip/clump_fit.tsv", "r" ) ) {
			char line[256], name[32];
			double t, k, err, errDry = -1, errWet = -1, errRed = -1;
			while ( std::fgets( line, sizeof line, f ) )
				if ( line[0] != '#' && std::sscanf( line, "%31s %lf %lf %lf", name, &t, &k, &err ) == 4 ) {
					if ( !std::strcmp( name, "dry" ) )
						errDry = err;
					else if ( !std::strcmp( name, "wet" ) )
						errWet = err;
					else if ( !std::strcmp( name, "red_wet" ) )
						errRed = err;
				}
			std::fclose( f );
			check( 6, errDry >= 0 && errDry < 1.0, "clump fit: dry threshold vs the old big-clump atlas (metric distance, < 1)", errDry, 1.0 );
			const double e = red ? errRed : errWet;
			check( 6, e >= 0 && e < 1.0, "clump fit: wet threshold vs the old wet atlas (metric distance, < 1)", e, 1.0 );
		} else
			check( 6, false, "clump_fit.tsv read", 0.0, 1.0 );
		// the flicker (offline streak render) and the n = 2 oscillation (Rayleigh: 8 sigma / (rho r0^3), sigma 0.0728 N/m)
		check( 6, within( dropFlickerDepth( 2.0 ), 0.51, 0.001 ), "flicker depth, 2 mm", dropFlickerDepth( 2.0 ), 0.51 );
		check( 6, within( dropFlickerDepth( 4.0 ), 0.2703, 0.001 ), "flicker depth, 4 mm", dropFlickerDepth( 4.0 ), 0.2703 );
		check( 6, dropFlickerDepth( 0.5 ) == 0.0, "flicker depth, 0.5 mm (none)", dropFlickerDepth( 0.5 ), 0.0 );
		check( 6, within( dropOscW2( 2.0 ), 763.15, 0.001 ), "n = 2 oscillation, 2 mm drop (rad/s; 121.5 Hz)", dropOscW2( 2.0 ), 763.15 );
	}

	// ---- rule 7 (PRECIPLOOK1): the shader's drop phase table (cell_volpart.glsl PC_DROP_PI, pi x p) against the Monte
	// Carlo ray trace of a water sphere (dropphase_mc.tsv): normalised, g = 0.767, and bin by bin where the MC has weight
	{
		std::vector<double> sh, mc;
		if ( FILE * f = std::fopen( PRECIP_ROOT "/res/shaders/features/air/cell_volpart.glsl", "r" ) ) {
			std::string all;
			char buf[4096];
			size_t got;
			while ( ( got = std::fread( buf, 1, sizeof buf, f ) ) > 0 )
				all.append( buf, got );
			std::fclose( f );
			const size_t at = all.find( "PC_DROP_PI[64] = float[64](" );
			if ( at != std::string::npos ) {
				const char * p = all.c_str() + at + std::strlen( "PC_DROP_PI[64] = float[64](" );
				for ( int i = 0; i < 64; i++ ) {
					char * e = nullptr;
					const double v = std::strtod( p, &e );
					if ( e == p )
						break;
					sh.push_back( v );
					p = e;
					while ( *p == ',' || *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' )
						p++;
				}
			}
		}
		if ( FILE * f = std::fopen( PRECIP_ROOT "/tests/features/weather/precip/dropphase_mc.tsv", "r" ) ) {
			char line[256];
			double th, v;
			while ( std::fgets( line, sizeof line, f ) )
				if ( line[0] != '#' && std::sscanf( line, "%lf %lf", &th, &v ) == 2 )
					mc.push_back( v );
			std::fclose( f );
		}
		check( 7, sh.size() == 64 && mc.size() == 64, "phase table: 64 bins in the shader and the fixture", double( sh.size() ), 64.0 );
		if ( sh.size() == 64 && mc.size() == 64 ) {
			const double pi = 3.14159265358979, dth = pi / 64.0;
			double norm = 0.0, g = 0.0, gmc = 0.0, worst = 0.0;
			int nb = 0;
			for ( int i = 0; i < 64; i++ ) {
				const double th = ( i + 0.5 ) * dth, w = 2.0 * std::sin( th ) * dth;	// (pi p) x 2 pi sin / pi
				norm += sh[i] * w;
				g += sh[i] * w * std::cos( th );
				gmc += mc[i] * w * std::cos( th );
				if ( sh[i] * w > 0.005 ) {	// bins carrying > 0.5 % of the power (dropphase.py's own measure: 26 bins, 3.4 %)
					worst = std::max( worst, std::fabs( sh[i] - mc[i] ) / mc[i] );
					nb++;
				}
			}
			check( 7, within( norm, 1.0, 0.01 ), "phase table: integral of p over the sphere", norm, 1.0 );
			check( 7, within( g, 0.7668, 0.005 ), "phase table: g (cos-weighted mean)", g, 0.7668 );
			check( 7, within( g, gmc, 0.01 ), "phase table: g vs the Monte Carlo's", g, gmc );
			std::snprintf( what, sizeof what, "phase table vs Monte Carlo: worst relative bin of %d carrying > 0.5 %% of the power", nb );
			check( 7, worst < 0.05, what, worst, 0.05 );
		}
	}

	// ---- the parser (no red: it is the format, docs/RECX.md)
	{
		const QByteArray js = R"({ "layers": [ { "type": "snow_fine", "rate": 2, "size": 5.5, "drawScale": 3, "bogus": 1 },
			{ "type": "hail", "density": 0.5, "atlas": [4, 2] }, { "type": "fog" }, { "type": "rain", "rate": 9999 } ] })";
		QStringList notes;
		const QVector<WwPrecipLayerSpec> L = wwPrecipParse( QJsonDocument::fromJson( js ).object(), notes );
		check( 0, L.size() == 3, "parse: layers kept", double( L.size() ), 3.0 );
		// WEATHERRECX2: + one alias line (snow_fine is the old name of snowFine)
		check( 0, notes.size() == 4, "parse: notes (alias, unknown key, bad type, clamp)", double( notes.size() ), 4.0 );
		check( 0, L.size() == 3 && L[0].type == SnowFine && L[1].type == Hail && L[1].density == 0.5 && L[2].rateMmH == 500.0,
			"parse: values (1 = as written)", L.size() == 3 && L[0].type == SnowFine && L[1].density == 0.5 ? 1.0 : 0.0, 1.0 );
		const QVector<WwPrecipLayerSpec> R = wwPrecipFromRecord( 4u | 8u );
		check( 0, R.size() == 2 && R[0].type == Rain && R[1].type == SnowFine, "record fallback: Rainy + Snow flags -> rain + snow_fine",
			double( R.size() ), 2.0 );
		check( 0, wwPrecipFromRecord( 1u | 2u ).isEmpty(), "record fallback: Pleasant + Cloudy -> none", double( wwPrecipFromRecord( 3u ).size() ), 0.0 );
		// PRECIPLOOK1: sleet is no type; a layer naming it is dropped with its own note
		QStringList sn;
		const QVector<WwPrecipLayerSpec> S = wwPrecipParse(
			QJsonDocument::fromJson( R"({ "layers": [ { "type": "sleet", "rate": 2 }, { "type": "rain" } ] })" ).object(), sn );
		const bool sleetNote = sn.size() == 1 && sn[0].contains( QLatin1String( "sleet removed" ) );
		check( 0, S.size() == 1 && S[0].type == Rain && sleetNote, "parse: a sleet layer dropped with the removal note (1 = yes)",
			sleetNote ? 1.0 : 0.0, 1.0 );
		// the field seeds keep the type indices from before sleet went (rain 0, hail 2, snow_fine 3, far_dot 4, wet 5, big 6)
		const int tags[TypeCount] = { 0, 2, 3, 4, 5, 6 };
		int bad = 0;
		for ( int ty = 0; ty < TypeCount; ty++ )
			bad += typeInfo( ty ).seedTag != tags[ty];
		check( 0, bad == 0, "seed tags = the old type indices (types differing)", double( bad ), 0.0 );
	}

	std::printf( "precip unit: %d of %d failed (rule1 %d, rule2 %d, rule3 %d, rule4 %d, rule5 %d, look %d, phase %d, parser %d)\n",
		g_fail, g_checks, g_failRule[1], g_failRule[2], g_failRule[3], g_failRule[4], g_failRule[5], g_failRule[6], g_failRule[7],
		g_failRule[0] );
	std::printf( "PRECIP %s\n", g_fail ? "FAIL" : "PASS" );
	return g_fail ? 1 : 0;
}
