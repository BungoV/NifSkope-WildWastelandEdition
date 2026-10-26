#ifndef WW_CLOUDSHADOWMATH_H
#define WW_CLOUDSHADOWMATH_H

/* lane CLOUD3: the cloud shadow map's geometry, no Qt and no GL (tests/cloudshadow/cloudshadow_dump.cpp prints it for
 * the offline gate). The map is a Beer shadow map (Hillaire 2016, "Physically Based Sky, Atmosphere and Cloud Rendering
 * in Frostbite", sec. 5.5): one texel per line toward the sun, storing where the cloud starts along that line, the
 * mean extinction and the line's whole optical depth. Light space: e1, e2 across the sun, s = TO the sun (world). */

#include <algorithm>
#include <cmath>

namespace WwCloudShadowMath
{

constexpr double kMinElevDeg = 3.0;	// below this the lines run too long to fit any map: the sun is held at 3 deg

struct Basis
{
	double s[3], e1[3], e2[3];
};

//! sunTo: world direction TO the sun (any length); red vertical swaps in straight up (the shadow under the cloud)
inline Basis basis( const float sunTo[3], bool redVertical )
{
	Basis b{};
	double s[3] = { sunTo[0], sunTo[1], sunTo[2] };
	if ( redVertical ) {
		s[0] = s[1] = 0.0;
		s[2] = 1.0;
	}
	double l = std::sqrt( s[0] * s[0] + s[1] * s[1] + s[2] * s[2] );
	if ( l < 1e-9 ) {
		s[0] = s[1] = 0.0;
		s[2] = l = 1.0;
	}
	for ( double & v : s )
		v /= l;
	const double minZ = std::sin( kMinElevDeg * 3.14159265358979323846 / 180.0 );
	if ( s[2] < minZ ) {
		double h = std::sqrt( s[0] * s[0] + s[1] * s[1] );
		const double k = h > 1e-9 ? std::sqrt( 1.0 - minZ * minZ ) / h : 0.0;
		s[0] *= k;
		s[1] *= k;
		s[2] = minZ;
		if ( h <= 1e-9 )
			s[0] = std::sqrt( 1.0 - minZ * minZ );
	}
	double e1[3] = { -s[1], s[0], 0.0 };
	double l1 = std::sqrt( e1[0] * e1[0] + e1[1] * e1[1] );
	if ( l1 < 1e-9 ) {
		e1[0] = 1.0;
		e1[1] = 0.0;
		l1 = 1.0;
	}
	e1[0] /= l1;
	e1[1] /= l1;
	// e2 = s x e1
	const double e2[3] = { s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2], s[0] * e1[1] - s[1] * e1[0] };
	for ( int i = 0; i < 3; i++ ) {
		b.s[i] = s[i];
		b.e1[i] = e1[i];
		b.e2[i] = e2[i];
	}
	return b;
}

struct Fit
{
	double a0 = 0.0, b0 = 0.0;	//!< the map's centre in light space, km (snapped)
	double extentKm = 48.0;	//!< the side, km
	int size = 512;
	double texelKm() const { return extentKm / double( std::max( size, 1 ) ); }
};

//! centre on the eye's sun line, snapped to an eighth of the side so the map is rebuilt only every few km of travel
inline Fit fit( const double eyeKm[3], double extentKm, int size, const Basis & b )
{
	Fit f;
	f.extentKm = extentKm;
	f.size = size;
	const double snap = extentKm / 8.0;
	const double a = b.e1[0] * eyeKm[0] + b.e1[1] * eyeKm[1] + b.e1[2] * eyeKm[2];
	const double c = b.e2[0] * eyeKm[0] + b.e2[1] * eyeKm[1] + b.e2[2] * eyeKm[2];
	f.a0 = std::floor( a / snap + 0.5 ) * snap;
	f.b0 = std::floor( c / snap + 0.5 ) * snap;
	return f;
}

/*! column-major 4x4: world (game units) -> (u, v, height km, 1); unitKm = km per game unit. Points on one sun line share
 *  (u, v): u, v are the coordinates across s. */
inline void lightFromWorld( const Basis & b, const Fit & f, double unitKm, double out[16] )
{
	const double k = unitKm / f.extentKm;
	for ( int c = 0; c < 3; c++ ) {
		out[c * 4 + 0] = b.e1[c] * k;
		out[c * 4 + 1] = b.e2[c] * k;
		out[c * 4 + 2] = c == 2 ? unitKm : 0.0;
		out[c * 4 + 3] = 0.0;
	}
	out[12] = 0.5 - f.a0 / f.extentKm;
	out[13] = 0.5 - f.b0 / f.extentKm;
	out[14] = 0.0;
	out[15] = 1.0;
}

//! out = a x b, column-major
inline void mul( const double a[16], const double b[16], double out[16] )
{
	for ( int c = 0; c < 4; c++ )
		for ( int r = 0; r < 4; r++ ) {
			double v = 0.0;
			for ( int k = 0; k < 4; k++ )
				v += a[k * 4 + r] * b[c * 4 + k];
			out[c * 4 + r] = v;
		}
}

//! the angle between two unit directions, degrees (the rebuild test)
inline double angleDeg( const double a[3], const double b[3] )
{
	const double x = a[1] * b[2] - a[2] * b[1], y = a[2] * b[0] - a[0] * b[2], z = a[0] * b[1] - a[1] * b[0];
	return std::atan2( std::sqrt( x * x + y * y + z * z ), a[0] * b[0] + a[1] * b[1] + a[2] * b[2] ) * 180.0 / 3.14159265358979323846;
}

/*! the receiver's transmittance from one texel (x = height km where the cloud starts from the sun's side, y = mean
 *  extinction per km of height, z = the line's whole optical depth); zKm = the receiver's height. The shader's
 *  ussCloudT is this same expression. */
inline double receiverT( const float texel[3], double zKm )
{
	const double od = std::min( double( texel[2] ), double( texel[1] ) * std::max( double( texel[0] ) - zKm, 0.0 ) );
	return std::exp( -od );
}

//! the far value: the bands' statistical transmittance (coverage c, thickness km, relative density) at sun height sz
inline double bandFarT( const float cov[2], const float thickKm[2], const float dens[2], double sz, double sigma )
{
	double T = 1.0;
	for ( int i = 0; i < 2; i++ ) {
		const double tau = sigma * double( dens[i] ) * 0.4 * double( thickKm[i] ) / std::max( sz, 0.05 );
		T *= 1.0 - double( std::clamp( cov[i], 0.0f, 1.0f ) ) * ( 1.0 - std::exp( -tau ) );
	}
	return T;
}

} // namespace WwCloudShadowMath

#endif
