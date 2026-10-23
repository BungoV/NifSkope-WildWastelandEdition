// skyephem_gate -- lane EPHEM1 (2026-10-23): src/gl/skyephem.h against published numbers.
//
//   g++ -O2 -std=c++17 -I src tests/skyephem_gate.cpp -o skyephem_gate && ./skyephem_gate
//
// Prints one PASS/FAIL line per check and exits non-zero on any FAIL. Every check has a control that must FAIL
// the same bound (a December sun for the June numbers, the anti-solar fallback for the moon), so the bound has teeth.
// The numbers: the June solstice noon at Boston (NOAA Solar Calculator, 71.07 deg, due south at transit), four
// published new/full moon instants of 2024 (elongation 0 / 180), the full moon opposite the sun within its
// ecliptic latitude (5.2 deg), and Fallout's 365-day calendar from 2287-10-23.

#include "gl/skyephem.h"

#include <cmath>
#include <cstdio>

using namespace wwephem;

static int fails = 0;

static void check( bool ok, const char * what, double v )
{
	std::printf( "%s %s (%.4f)\n", ok ? "PASS" : "FAIL", what, v );
	if ( !ok )
		fails++;
}

static double gapDeg( double a, double b )
{
	double d = std::fmod( std::fabs( a - b ), 360.0 );
	return d > 180.0 ? 360.0 - d : d;
}

static double angleDeg( const double a[3], const double b[3] )
{
	double c = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
	c = c < -1.0 ? -1.0 : ( c > 1.0 ? 1.0 : c );
	return std::acos( c ) * kDeg;
}

static Date at( int y, int m, int d, double h )
{
	Date r;
	r.year = y;
	r.month = m;
	r.day = d;
	r.hour = h;
	return r;
}

int main()
{
	// sun: June solstice noon (local mean time) at Boston
	const Body j = sun( at( 2024, 6, 21, 12.0 ) );
	check( std::fabs( j.elevDeg - 71.07 ) < 0.03, "sun 2024-06-21 12:00 LMT elevation 71.07", j.elevDeg );
	check( gapDeg( j.azDeg, 180.0 ) < 2.0, "sun 2024-06-21 12:00 LMT azimuth ~180", j.azDeg );
	// transit: the day's peak is 90 - |lat - decl| and due south
	double best = -90.0, bestAz = 0.0, bestDecl = 0.0;
	for ( int i = 0; i < 24 * 600; i++ ) {
		const Body b = sun( at( 2024, 6, 21, i / 600.0 ) );
		if ( b.elevDeg > best ) {
			best = b.elevDeg;
			bestAz = b.azDeg;
			bestDecl = b.declDeg;
		}
	}
	check( std::fabs( best - ( 90.0 - std::fabs( kBostonLat - bestDecl ) ) ) < 0.01, "sun transit = 90 - |lat - decl|", best );
	check( gapDeg( bestAz, 180.0 ) < 0.1, "sun transit due south", bestAz );
	const Body dec = sun( at( 2024, 12, 21, 12.0 ) );
	check( !( std::fabs( dec.elevDeg - 71.07 ) < 0.03 ), "control: December noon fails the June elevation", dec.elevDeg );

	// moon: published 2024 new/full instants (UT; place at lon 0 so the clock is UT)
	struct Ep { int m, d; double h; double want; const char * what; };
	const Ep eps[4] = { { 1, 11, 11 + 57 / 60.0, 0.0, "new 2024-01-11 11:57 UT" }, { 1, 25, 17 + 54 / 60.0, 180.0, "full 2024-01-25 17:54 UT" },
		{ 6, 6, 12 + 38 / 60.0, 0.0, "new 2024-06-06 12:38 UT" }, { 6, 22, 1 + 8 / 60.0, 180.0, "full 2024-06-22 01:08 UT" } };
	for ( const Ep & e : eps ) {
		const double el = elongationDeg( julianDay( 2024, e.m, e.d + e.h / 24.0 ) );
		check( gapDeg( el, e.want ) < 0.3, e.what, el );
	}
	// full moon: opposite the sun within the moon's ecliptic latitude; a day off must fail the elongation bound
	{
		const Body s = sun( at( 2024, 1, 25, 17 + 54 / 60.0 ), kBostonLat, 0.0 );
		const Body m = moon( at( 2024, 1, 25, 17 + 54 / 60.0 ), kBostonLat, 0.0 );
		const double anti[3] = { -s.dir[0], -s.dir[1], -s.dir[2] };
		check( angleDeg( m.dir, anti ) < 5.3, "full moon opposite the sun (< 5.3 deg)", angleDeg( m.dir, anti ) );
		const double off = elongationDeg( julianDay( 2024, 1, 26 + ( 17 + 54 / 60.0 ) / 24.0 ) );
		check( !( gapDeg( off, 180.0 ) < 0.3 ), "control: a day after full fails the full bound", off );
	}
	// phase texture and lit fraction
	check( phaseIndex( 180.0 ) == 0 && phaseIndex( 0.0 ) == 4 && phaseIndex( 90.0 ) == 6 && phaseIndex( 270.0 ) == 2,
		"phase index full 0 / new 4 / first quarter half_wax 6 / last quarter half_wan 2", phaseIndex( 90.0 ) );
	check( std::fabs( illuminatedFraction( 180.0 ) - 1.0 ) < 1e-9 && std::fabs( illuminatedFraction( 90.0 ) - 0.5 ) < 1e-9,
		"lit fraction full 1, quarter 0.5", illuminatedFraction( 90.0 ) );
	// continuity: one minute moves the sun < 0.3 deg and the moon < 0.3 deg, across midnight too
	{
		double worstS = 0.0, worstM = 0.0;
		Body ps = sun( dateFromGameDay( 0.0, 0.0 ) ), pm = moon( dateFromGameDay( 0.0, 0.0 ) );
		for ( int i = 1; i <= 48 * 60; i++ ) {
			const double t = i / 60.0;
			const Date d = dateFromGameDay( std::floor( t / 24.0 ), std::fmod( t, 24.0 ) );
			const Body s = sun( d ), m = moon( d );
			worstS = std::fmax( worstS, angleDeg( ps.dir, s.dir ) );
			worstM = std::fmax( worstM, angleDeg( pm.dir, m.dir ) );
			ps = s;
			pm = m;
		}
		check( worstS < 0.3 && worstM < 0.3, "sun + moon continuous over two game days (midnight included)", std::fmax( worstS, worstM ) );
	}
	// Fallout's calendar
	{
		const Date a = dateFromGameDay( 0.0, 7.5 ), b = dateFromGameDay( 9.0, 0.0 ), c = dateFromGameDay( 70.0, 0.0 ),
			   d = dateFromGameDay( 365.0, 0.0 ), e = dateFromGameDay( 129.0, 0.0 );
		check( a.year == 2287 && a.month == 10 && a.day == 23 && a.hour == 7.5, "day 0 = 2287-10-23", a.day );
		check( b.month == 11 && b.day == 1, "day 9 = 2287-11-01", b.day );
		check( c.year == 2288 && c.month == 1 && c.day == 1, "day 70 = 2288-01-01", c.day );
		check( e.year == 2288 && e.month == 3 && e.day == 1, "day 129 = 2288-03-01 (no 29 February)", e.day );
		check( d.year == 2288 && d.month == 10 && d.day == 23, "day 365 = 2288-10-23", d.day );
	}
	std::printf( "%s: %d failed\n", fails ? "FAIL" : "PASS", fails );
	return fails ? 1 : 0;
}
