// lane SKYDISC1: dumps src/gl/skydisc.h + the skyephem.h libration for tests/spells/skydisc_check.py (one "key v..." line each)
#include "gl/skydisc.h"
#include "gl/skyephem.h"

#include <cstdio>

using namespace wwskydisc;

int main()
{
	for ( int a = 0; a <= 180; a += 15 )
		std::printf( "phase %d %.6f %.6f %.6f %.6g\n", a, lsDiscMean( a ), ksFactor( a ), jensenBS( a ) / jensenBS( 0 ), earthshineRatio( a ) );
	// the ecliptic helper against moon(): the same direction
	double worst = 0.0, worstB = 0.0, worstPole = 0.0;
	for ( int day = 0; day < 60; day++ )
		for ( int h = 0; h < 24; h += 3 ) {
			wwephem::Date d = wwephem::dateFromGameDay( day, h );
			const wwephem::Body m = wwephem::moon( d );
			double lon, lat, v[3];
			wwephem::moonEcliptic( m.julianDay, lon, lat );
			wwephem::eclipticToHorizontal( m.julianDay, lon, lat, wwephem::kBostonLat, wwephem::kBostonLon, v );
			worst = std::max( worst, std::acos( std::clamp( dot( v, m.dir ), -1.0, 1.0 ) ) / kRad );
			// the pole seen from the Earth: o . P = sin b' (the sub-observer latitude)
			const wwephem::Libration L = wwephem::libration( m.julianDay );
			double P[3];
			wwephem::eclipticToHorizontal( m.julianDay, L.poleLon, L.poleLat, wwephem::kBostonLat, wwephem::kBostonLon, P );
			const double o[3] = { -v[0], -v[1], -v[2] };
			worstB = std::max( worstB, std::fabs( std::asin( dot( o, P ) ) / kRad - L.bDeg ) );
			// l' from the body frame: the observer at longitude l'
			double ex[3], ey[3], ez[3];
			bodyFrame( v, P, L.lDeg, ex, ey, ez );
			const double lo = std::atan2( dot( o, ey ), dot( o, ex ) ) / kRad;
			worstPole = std::max( worstPole, std::fabs( lo - L.lDeg ) );
			if ( day < 30 && h == 0 )
				std::printf( "lib %d %.4f %.4f\n", day, L.lDeg, L.bDeg );
		}
	std::printf( "ecl2hor_max_deg %.6g\n", worst );
	std::printf( "lat_vs_pole_max_deg %.6g\n", worstB );
	std::printf( "lon_vs_frame_max_deg %.6g\n", worstPole );
	// the game moon: phase angle from the elongation construction
	const double P[3] = { 0.0, std::cos( 42.36 * kRad ), std::sin( 42.36 * kRad ) };
	const double M[3] = { 0.3, -0.5, 0.81 };
	double Mn[3] = { M[0], M[1], M[2] };
	normalize( Mn );
	for ( int p = 0; p < 8; p++ ) {
		const double e = 180.0 + 45.0 * p;
		double S[3];
		sunFromElongation( Mn, P, e, S );
		double w[3];
		cross( Mn, P, w );
		normalize( w );
		std::printf( "game %d alpha %.4f sun_west %.4f\n", p, phaseAngleDeg( Mn, S ), dot( S, w ) );
	}
	return 0;
}
