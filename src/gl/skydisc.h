#ifndef SKYDISC_H
#define SKYDISC_H

/* lane SKYDISC1 (10-27): our sun disc and moon globe, the pure math (no GL, no Qt; tests/skydisc dumps it).
 * Sizes: sun half-angle 0.2665 deg (atmo_sky.frag kSunCos), moon 0.26 deg (0.52 deg at mean distance, Frostbite 2016
 * sec. 4.2). The moon's surface law is Lommel-Seeliger mu0 / (mu0 + mu) (Jensen et al. 2001, "A Physically-Based Night
 * Sky Model"); its whole-disc brightness follows Krisciunas & Schaefer 1991 (atmomath.h moonPhaseFactor), the curve the
 * night key light uses, so the drawn disc and the moonlight agree. Earthshine: Jensen eq. 1 over 1905 W/m2.
 * Notes and sources: E:/Projects/Claude/notes/skydisc1/RESEARCH.md. World axes +X east, +Y north, +Z up. */

#include <algorithm>
#include <cmath>

namespace wwskydisc {

constexpr double kPi = 3.14159265358979323846;
constexpr double kRad = kPi / 180.0;
constexpr double kSunHalfDeg = 0.2665;
constexpr double kMoonHalfDeg = 0.26;
constexpr double kAlbedoMean = 0.30;	// tools/moon_texture.py KMEAN: the stored albedo's linear mean
constexpr double kMoonOverSun = 2500.0 / 1.6e9;	// full moon / noon solar disc luminance (Wikipedia, orders of luminance)

inline double ksFactor( double alphaDeg )	// Krisciunas & Schaefer 1991, against the full moon
{
	const double a = std::fabs( alphaDeg );
	return std::pow( 10.0, -0.4 * ( 0.026 * a + 4e-9 * a * a * a * a ) );
}

//! the Lommel-Seeliger disc mean at phase angle alpha (sun lit, smooth sphere), mean over the disc's screen area
inline double lsDiscMean( double alphaDeg, int n = 160 )
{
	const double sa = std::sin( alphaDeg * kRad ), ca = std::cos( alphaDeg * kRad );
	double sum = 0.0;
	int cnt = 0;
	for ( int j = 0; j < n; j++ )
		for ( int i = 0; i < n; i++ ) {
			const double x = ( i + 0.5 ) / n * 2.0 - 1.0, y = ( j + 0.5 ) / n * 2.0 - 1.0, r2 = x * x + y * y;
			if ( r2 >= 1.0 )
				continue;
			const double z = std::sqrt( 1.0 - r2 );	// toward the viewer
			const double mu0 = std::max( 0.0, x * sa + z * ca );
			sum += mu0 > 0.0 ? mu0 / ( mu0 + z ) : 0.0;
			cnt++;
		}
	return cnt ? sum / cnt : 0.0;
}

//! Jensen 2001 eq. 1: the Earth's light at the Moon against the Sun's (1905 W/m2); alpha = the moon's phase angle
inline double earthshineRatio( double alphaDeg )
{
	const double x = ( kPi - std::fabs( alphaDeg ) * kRad );	// pi - phi
	double v = 1.0;
	if ( x > 1e-6 )
		v = 1.0 - std::sin( x * 0.5 ) * std::tan( x * 0.5 ) * std::log( 1.0 / std::tan( x * 0.25 ) );
	return std::max( 0.0, 0.19 * 0.5 * v ) / 1905.0;
}

//! Jensen's Hapke-lite phase terms B(alpha) S(alpha) (g 0.6, t 0.1): INFO beside ksFactor, not drawn
inline double jensenBS( double alphaDeg, double g = 0.6, double t = 0.1 )
{
	const double a = std::fabs( alphaDeg ) * kRad;
	double B = 1.0;
	if ( a < kPi * 0.5 ) {
		const double ta = std::max( 1e-6, std::tan( a ) ), e = std::exp( -g / ta );
		B = 2.0 - ta / ( 2.0 * g ) * ( 1.0 - e ) * ( 3.0 - e );
	}
	const double S = ( std::sin( a ) + ( kPi - a ) * std::cos( a ) ) / kPi + t * ( 1.0 - 0.5 * std::cos( a ) ) * ( 1.0 - 0.5 * std::cos( a ) );
	return B * S;
}

inline void cross( const double a[3], const double b[3], double o[3] )
{
	o[0] = a[1] * b[2] - a[2] * b[1];
	o[1] = a[2] * b[0] - a[0] * b[2];
	o[2] = a[0] * b[1] - a[1] * b[0];
}

inline double dot( const double a[3], const double b[3] ) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

inline bool normalize( double v[3] )
{
	const double l = std::sqrt( dot( v, v ) );
	if ( !( l > 1e-12 ) )
		return false;
	for ( int k = 0; k < 3; k++ )
		v[k] /= l;
	return true;
}

/*! the moon's body axes in the world: ez = its spin pole, ex = selenographic longitude 0, ey = +90 east (right-handed).
 *  D = TO the moon, pole = the spin pole (world), lDeg = the sub-observer longitude (libration). */
inline bool bodyFrame( const double D[3], const double pole[3], double lDeg, double ex[3], double ey[3], double ez[3] )
{
	double P[3] = { pole[0], pole[1], pole[2] };
	if ( !normalize( P ) )
		return false;
	const double o[3] = { -D[0], -D[1], -D[2] };	// moon to observer
	const double op = dot( o, P );
	double u[3] = { o[0] - op * P[0], o[1] - op * P[1], o[2] - op * P[2] };
	if ( !normalize( u ) )
		return false;
	double pu[3];
	cross( P, u, pu );
	const double c = std::cos( lDeg * kRad ), s = std::sin( lDeg * kRad );
	for ( int k = 0; k < 3; k++ ) {
		ex[k] = c * u[k] - s * pu[k];
		ez[k] = P[k];
	}
	cross( ez, ex, ey );
	return true;
}

/*! the game clock's moon: the sun `elongDeg` west of the moon along the celestial equator's direction (east of it
 *  past 180 = waning), so the lit limb faces the sun. P = the celestial pole (world). */
inline void sunFromElongation( const double M[3], const double P[3], double elongDeg, double S[3] )
{
	double w[3];
	cross( M, P, w );	// west along the parallel
	if ( !normalize( w ) ) {
		w[0] = -1.0;
		w[1] = w[2] = 0.0;
	}
	const double c = std::cos( elongDeg * kRad ), s = std::sin( elongDeg * kRad );
	for ( int k = 0; k < 3; k++ )
		S[k] = c * M[k] + s * w[k];
	normalize( S );
}

//! the phase angle (sun-moon-observer) from the two world directions, degrees (0 = full)
inline double phaseAngleDeg( const double M[3], const double S[3] )
{
	const double c = std::clamp( dot( M, S ), -1.0, 1.0 );
	return 180.0 - std::acos( c ) / kRad;
}

inline double smoothstep( double e0, double e1, double x )
{
	const double t = std::clamp( ( x - e0 ) / ( e1 - e0 ), 0.0, 1.0 );
	return t * t * ( 3.0 - 2.0 * t );
}

constexpr double kSunSolid = 6.7967e-5;	// atmo_sky.frag: 2 pi (1 - cos 0.2665 deg) sr
constexpr double kMoonPeak = 0.6;	// the full moon's centre on screen (x exposure) at most: the eye adapts to it locally
constexpr double kEarthGain = 200.0;	// the same local adaptation for the ashen light (physical 5e-5 of full is one 8-bit step)

//! the globe shader's light constants in scene units (sky_moon.frag sunTerm / earthTerm)
struct MoonTerms
{
	double R = 0.0, sunTerm = 0.0, earthTerm = 0.0;
};

/*! sunK = the sky pass's pi k (atSunK), exposure = the backdrop's. R = the full moon's disc radiance: the real
 *  2500 / 1.6e9 of the sun's disc, never past kMoonPeak on screen. The disc's mean follows KS(alpha). */
inline MoonTerms moonTerms( double alphaDeg, double sunK, double exposure )
{
	MoonTerms t;
	const double day = sunK / kSunSolid * kMoonOverSun;
	t.R = std::min( day, kMoonPeak / std::max( exposure, 1e-9 ) );
	const double ls = lsDiscMean( alphaDeg, 120 );
	t.sunTerm = ls > 1e-6 ? t.R * ksFactor( alphaDeg ) / ls : 0.0;
	t.earthTerm = t.R * earthshineRatio( alphaDeg ) * kEarthGain;	// x 0.5 (LS at zero phase) / lsDiscMean(0) = 1
	return t;
}

//! the mip of a texture `width` texels round the moon for one pixel of pxAng radians (the disc's centre)
inline double moonLod( double width, double pxAng )
{
	const double perPx = width / ( 2.0 * kPi ) * pxAng / std::sin( kMoonHalfDeg * kRad );
	return perPx > 1.0 ? std::log2( perPx ) : 0.0;
}

}	// namespace wwskydisc

#endif // SKYDISC_H
