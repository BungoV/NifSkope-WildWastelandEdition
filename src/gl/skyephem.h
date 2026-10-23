#ifndef SKYEPHEM_H
#define SKYEPHEM_H

/* lane EPHEM1 (10-23): the real sun and moon over Boston, the SAME arithmetic as FO4CS [Solar]
 * (fallout4-community-shaders, branch bungo, src/Sky/SolarPosition.h + SolarMoonLight.h): the NOAA solar
 * sequence (Meeus ch. 22/25/28) and Meeus ch. 47 truncated (6 longitude + 4 latitude terms), geometric (no
 * refraction, no lunar parallax, as FO4CS), the game clock read as local mean time at the longitude.
 * World axes +X east, +Y north, +Z up, azimuth clockwise from north. Pure header: tests/skyephem checks it. */

#include <cmath>
#include <cstdint>

namespace wwephem {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDeg = 180.0 / kPi;
constexpr double kRad = kPi / 180.0;

// FO4CS res/Solar/00_FO4CS_Places.json, the Commonwealth entry
constexpr double kBostonLat = 42.36;
constexpr double kBostonLon = -71.06;	// negative = west

struct Date
{
	int year = 2287, month = 10, day = 23;	// month and day one-based
	double hour = 12.0;	// [0, 24), local mean time
};

struct Body
{
	bool valid = false;
	double elevDeg = 0.0, azDeg = 0.0;	// geometric elevation; azimuth clockwise from north
	double dir[3] = { 0.0, 0.0, 1.0 };	// unit, towards the body
	double declDeg = 0.0;
	double julianDay = 0.0;
};

// Fallout's calendar has no leap years (FO4CS SolarPosition.h, the calendar mapping); the game starts 2287-10-23
inline Date dateFromGameDay( double gameDay, double hour )
{
	static const int dim[13] = { 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	Date d;
	long long n = (long long) std::floor( gameDay > 0.0 ? gameDay : 0.0 );
	while ( n > 0 ) {
		const int left = dim[d.month] - d.day;	// days to the month's last day
		if ( n <= left ) {
			d.day += int( n );
			n = 0;
		} else {
			n -= left + 1;
			d.day = 1;
			if ( ++d.month > 12 ) {
				d.month = 1;
				d.year++;
			}
		}
	}
	double h = std::isfinite( hour ) ? std::fmod( hour, 24.0 ) : 12.0;
	d.hour = h < 0.0 ? h + 24.0 : h;
	return d;
}

// Meeus 7.1, Gregorian
inline double julianDay( int year, int month, double dayFrac )
{
	double y = year, m = month;
	if ( m <= 2.0 ) {
		y -= 1.0;
		m += 12.0;
	}
	const double A = std::floor( y / 100.0 ), B = 2.0 - A + std::floor( A / 4.0 );
	return std::floor( 365.25 * ( y + 4716.0 ) ) + std::floor( 30.6001 * ( m + 1.0 ) ) + dayFrac + B - 1524.5;
}

inline double centuries( double jd ) { return ( jd - 2451545.0 ) / 36525.0; }

inline double obliquityDeg( double t )
{
	const double mean = 23.0 + ( 26.0 + ( ( 21.448 - t * ( 46.815 + t * ( 0.00059 - t * 0.001813 ) ) ) ) / 60.0 ) / 60.0;
	return mean + 0.00256 * std::cos( ( 125.04 - 1934.136 * t ) * kRad );
}

inline double sunApparentLonDeg( double t )
{
	const double L0 = std::fmod( 280.46646 + t * ( 36000.76983 + t * 0.0003032 ), 360.0 );
	const double M = 357.52911 + t * ( 35999.05029 - 0.0001537 * t );
	const double C = std::sin( M * kRad ) * ( 1.914602 - t * ( 0.004817 + 0.000014 * t ) )
		+ std::sin( 2.0 * M * kRad ) * ( 0.019993 - 0.000101 * t ) + std::sin( 3.0 * M * kRad ) * 0.000289;
	return L0 + C - 0.00569 - 0.00478 * std::sin( ( 125.04 - 1934.136 * t ) * kRad );
}

// minutes (NOAA closed form, Meeus 28.3)
inline double equationOfTimeMin( double t )
{
	const double L0 = std::fmod( 280.46646 + t * ( 36000.76983 + t * 0.0003032 ), 360.0 );
	const double M = 357.52911 + t * ( 35999.05029 - 0.0001537 * t );
	const double e = 0.016708634 - t * ( 0.000042037 + 0.0000001267 * t );
	const double obl = obliquityDeg( t );
	const double y = std::tan( obl * 0.5 * kRad ) * std::tan( obl * 0.5 * kRad );
	return 4.0 * kDeg * ( y * std::sin( 2.0 * L0 * kRad ) - 2.0 * e * std::sin( M * kRad )
		+ 4.0 * e * y * std::sin( M * kRad ) * std::cos( 2.0 * L0 * kRad ) - 0.5 * y * y * std::sin( 4.0 * L0 * kRad )
		- 1.25 * e * e * std::sin( 2.0 * M * kRad ) );
}

inline void horizontal( double latDeg, double declDeg, double haDeg, Body & b )
{
	const double sl = std::sin( latDeg * kRad ), cl = std::cos( latDeg * kRad );
	const double sd = std::sin( declDeg * kRad ), cd = std::cos( declDeg * kRad ), ch = std::cos( haDeg * kRad );
	double se = sl * sd + cl * cd * ch;
	se = se < -1.0 ? -1.0 : ( se > 1.0 ? 1.0 : se );
	// FO4CS keeps elevation/azimuth as floats; rounding here gives its numbers exactly
	b.elevDeg = double( float( std::asin( se ) * kDeg ) );
	double az = std::fmod( std::atan2( -cd * std::sin( haDeg * kRad ), sd * cl - cd * sl * ch ) * kDeg, 360.0 );
	if ( az < 0.0 )
		az += 360.0;
	b.azDeg = double( float( az ) );
	const double ce = std::cos( b.elevDeg * kRad );
	b.dir[0] = std::sin( b.azDeg * kRad ) * ce;
	b.dir[1] = std::cos( b.azDeg * kRad ) * ce;
	b.dir[2] = std::sin( b.elevDeg * kRad );
	b.declDeg = declDeg;
	b.valid = std::isfinite( b.dir[0] ) && std::isfinite( b.dir[1] ) && std::isfinite( b.dir[2] );
}

// UT Julian day of a local-mean-time date at a longitude
inline double julianDayLmt( const Date & d, double lonDeg )
{
	return julianDay( d.year, d.month, d.day + ( d.hour - lonDeg / 15.0 ) / 24.0 );
}

inline Body sun( const Date & d, double latDeg = kBostonLat, double lonDeg = kBostonLon )
{
	Body b;
	const double tz = lonDeg / 15.0;
	b.julianDay = julianDayLmt( d, lonDeg );
	const double t = centuries( b.julianDay );
	double sdecl = std::sin( obliquityDeg( t ) * kRad ) * std::sin( sunApparentLonDeg( t ) * kRad );
	sdecl = sdecl < -1.0 ? -1.0 : ( sdecl > 1.0 ? 1.0 : sdecl );
	const double decl = std::asin( sdecl ) * kDeg;
	double tst = std::fmod( d.hour * 60.0 + equationOfTimeMin( t ) + 4.0 * lonDeg - 60.0 * tz, 1440.0 );
	if ( tst < 0.0 )
		tst += 1440.0;
	const double ha = tst / 4.0 < 0.0 ? tst / 4.0 + 180.0 : tst / 4.0 - 180.0;
	horizontal( latDeg, decl, ha, b );
	return b;
}

inline double gmstDeg( double jd )
{
	double g = std::fmod( 280.46061837 + 360.98564736629 * ( jd - 2451545.0 ), 360.0 );
	return g < 0.0 ? g + 360.0 : g;
}

inline void moonEcliptic( double jd, double & lonDeg, double & latDeg )
{
	const double d = jd - 2451545.0;
	const double Lp = 218.3164477 + 13.17639648 * d, M = 134.9633964 + 13.06499295 * d;
	const double Ms = 357.5291092 + 0.98560028 * d, De = 297.8501921 + 12.19074912 * d;
	const double F = 93.2720950 + 13.22935024 * d;
	double lon = Lp + 6.289 * std::sin( M * kRad ) + 1.274 * std::sin( ( 2.0 * De - M ) * kRad )
		+ 0.658 * std::sin( 2.0 * De * kRad ) + 0.214 * std::sin( 2.0 * M * kRad ) - 0.186 * std::sin( Ms * kRad )
		- 0.114 * std::sin( 2.0 * F * kRad );
	lon = std::fmod( lon, 360.0 );
	lonDeg = lon < 0.0 ? lon + 360.0 : lon;
	latDeg = 5.128 * std::sin( F * kRad ) + 0.281 * std::sin( ( M + F ) * kRad ) - 0.278 * std::sin( ( F - M ) * kRad )
		- 0.173 * std::sin( ( F - 2.0 * De ) * kRad );
}

// sun-moon elongation, 0 = new, 180 = full, rising = waxing
inline double elongationDeg( double jd )
{
	double lon, lat;
	moonEcliptic( jd, lon, lat );
	double e = std::fmod( lon - sunApparentLonDeg( centuries( jd ) ), 360.0 );
	return e < 0.0 ? e + 360.0 : e;
}

inline Body moon( const Date & d, double latDeg = kBostonLat, double lonDeg = kBostonLon )
{
	Body b;
	b.julianDay = julianDayLmt( d, lonDeg );
	double lon, lat;
	moonEcliptic( b.julianDay, lon, lat );
	const double obl = obliquityDeg( centuries( b.julianDay ) );
	const double sl = std::sin( lon * kRad ), cl = std::cos( lon * kRad ), sb = std::sin( lat * kRad ), cb = std::cos( lat * kRad );
	const double se = std::sin( obl * kRad ), ce = std::cos( obl * kRad );
	double ra = std::fmod( std::atan2( sl * ce - ( sb / cb ) * se, cl ) * kDeg, 360.0 );
	if ( ra < 0.0 )
		ra += 360.0;
	double sdecl = sb * ce + cb * se * sl;
	sdecl = sdecl < -1.0 ? -1.0 : ( sdecl > 1.0 ? 1.0 : sdecl );
	const double lst = std::fmod( gmstDeg( b.julianDay ) + lonDeg, 360.0 );
	horizontal( latDeg, std::asin( sdecl ) * kDeg, std::fmod( lst - ra + 540.0, 360.0 ) - 180.0, b );
	return b;
}

// lit fraction of the disc (Meeus 48.1 with phase angle i = 180 - elongation, the FO4CS SolarMoonLight.h angle)
inline double illuminatedFraction( double elongDeg )
{
	return 0.5 * ( 1.0 - std::cos( elongDeg * kRad ) );
}

// the vanilla moon texture of an elongation: 0 full, 1 three_wan .. 4 new .. 7 three_wax (wwMoonPhaseSuffix order)
inline int phaseIndex( double elongDeg )
{
	double a = std::fmod( elongDeg - 180.0, 360.0 );
	if ( a < 0.0 )
		a += 360.0;
	return int( std::floor( a / 45.0 + 0.5 ) ) % 8;
}

// local sidereal time, degrees
inline double localSiderealDeg( double jd, double lonDeg )
{
	double l = std::fmod( gmstDeg( jd ) + lonDeg, 360.0 );
	return l < 0.0 ? l + 360.0 : l;
}

}	// namespace wwephem

#endif // SKYEPHEM_H
