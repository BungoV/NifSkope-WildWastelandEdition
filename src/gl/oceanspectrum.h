#ifndef OCEANSPECTRUM_H
#define OCEANSPECTRUM_H

/*! lane WATERLOOK1 (2026-10-27): the wind's wave spectrum on the CPU, std only (tested alone: notes/waterlook1/unit).
 *  The float64 mirror of notes/waterlook1/tools/ocean_model.py, line for line: two sets (0 sea, 1 fresh water) x 3
 *  cascades of 128 x 128 modes. JONSWAP (Hasselmann et al. 1973), Donelan-Hamilton-Hui (1985) spreading with Banner's
 *  (1990) beta, the sea's swell / a lake's long ripples (JONSWAP + Longuet-Higgins cos^2s), capillary-gravity
 *  dispersion, the resolved slope variance capped at Cox and Munk's (1954) mss for the wind. src/gl/cellocean.cpp
 *  uploads h0 and q; the GPU (res/shaders/ocean_*.frag) evolves and transforms them each frame. */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace WwOcean
{

constexpr double kG = 9.81;
constexpr double kKm = 370.0;
constexpr int kN = 128;
constexpr int kTiles = 6;
constexpr double kLoop = 1024.0;	// s: the surface loops exactly
constexpr double kPi = 3.14159265358979323846;

struct SetDef
{
	double L[3];	// tile sizes, m (non-commensurate)
	double cut[2];	// band edges between the cascades, rad/m
	double fetch;	// m
	double lam;	// the chop
	double patch;	// the wind patches' cell, u
	double cm[2];	// Cox-Munk mss = cm0 + cm1 U (clean sea, slick fresh water)
};

inline const SetDef & set( int s )
{
	static const SetDef k[2] = {
		{ { 1187.0, 97.1, 15.3 }, { 0.3, 2.6 }, 8495.0, 1.0, 8400.0, { 0.003, 0.00512 } },
		{ { 61.9, 9.53, 1.47 }, { 4.0, 26.0 }, 100.0, 0.6, 1400.0, { 0.003, 0.00084 } } };
	return k[s & 1];
}

// fresh: gentle long ripples (bungo's RDR2 lake reference); sea: the swell (Hs = swellM, WATERFOAMDETAIL1's)
constexpr double kRippleHs = 0.012, kRippleTp = 2.0, kRippleGamma = 3.3, kRippleS = 4.0;
constexpr double kSwellTp = 8.0, kSwellGamma = 7.0, kSwellS = 24.0;

struct SetParams
{
	double hw = 0.0, wp = 1.0, aw = 0.0;	// wind sea: Hs, peak omega, alpha
	double hs2 = 0.0, wp2 = 1.0, g2 = 3.3, s2 = 1.0, a2 = 0.0;	// swell / ripples
	double fw = 1.0;	// the Cox-Munk factor on the wind sea
	double hsSet = 0.0;
};

inline void band( int s, int c, double & lo, double & hi )
{
	const SetDef & S = set( s );
	const double edges[4] = { 0.0, S.cut[0], S.cut[1], 0.8 * kPi * kN / S.L[2] };
	lo = edges[c];
	hi = edges[c + 1];
}

inline double hsWind( double U, double F )
{
	return std::min( 0.0016 * U * std::sqrt( F / kG ), 0.21 * U * U / kG );
}

inline double wpWind( double U, double F )
{
	return std::max( 22.0 * std::cbrt( kG * kG / ( U * F ) ), 0.855 * kG / U );
}

inline double omega( double k )
{
	return std::sqrt( kG * k * ( 1.0 + ( k / kKm ) * ( k / kKm ) ) );
}

inline double domega( double k, double w )
{
	return kG * ( 1.0 + 3.0 * ( k / kKm ) * ( k / kKm ) ) / ( 2.0 * std::max( w, 1e-12 ) );
}

inline double jonswapShape( double w, double wp, double gamma )
{
	w = std::max( w, 1e-9 );
	const double sig = w <= wp ? 0.07 : 0.09;
	const double r = std::exp( -( ( w - wp ) * ( w - wp ) ) / ( 2.0 * sig * sig * wp * wp ) );
	return kG * kG * std::pow( w, -5.0 ) * std::exp( -1.25 * std::pow( wp / w, 4.0 ) ) * std::pow( gamma, r );
}

//! alpha so the integral over omega is ( hs / 4 )^2 (trapezoid on a log grid, 0.2 .. 60 omega_p, 4001 points)
inline double jonswapAlpha( double hs, double wp, double gamma )
{
	if ( hs <= 0.0 )
		return 0.0;
	const int n = 4001;
	const double a = std::log( 0.2 ), b = std::log( 60.0 );
	double integ = 0.0, wPrev = 0.0, sPrev = 0.0;
	for ( int i = 0; i < n; i++ ) {
		const double w = wp * std::exp( a + ( b - a ) * double( i ) / double( n - 1 ) );
		const double sv = jonswapShape( w, wp, gamma );
		if ( i > 0 )
			integ += 0.5 * ( sv + sPrev ) * ( w - wPrev );
		wPrev = w;
		sPrev = sv;
	}
	return ( hs / 4.0 ) * ( hs / 4.0 ) / integ;
}

inline double spreadDb( double theta, double w, double wp )
{
	const double q = std::max( w / wp, 0.56 );
	const double beta = q < 0.95 ? 2.61 * std::pow( q, 1.3 ) : q < 1.6 ? 2.28 * std::pow( q, -1.3 )
		: std::pow( 10.0, -0.4 + 0.8393 * std::exp( -0.567 * std::log( q * q ) ) );
	const double ch = std::cosh( beta * theta );
	return beta / ( 2.0 * std::tanh( beta * kPi ) ) / ( ch * ch );
}

inline double spreadLh( double theta, double s )
{
	const double norm = std::exp( std::lgamma( s + 1.0 ) - std::lgamma( s + 0.5 ) ) / ( 2.0 * std::sqrt( kPi ) );
	return norm * std::pow( std::fabs( std::cos( theta / 2.0 ) ), 2.0 * s );
}

inline std::uint32_t lowbias32( std::uint32_t x )
{
	x ^= x >> 16;
	x *= 0x7feb352du;
	x ^= x >> 15;
	x *= 0x846ca68bu;
	x ^= x >> 16;
	return x;
}

inline int freq( int m )
{
	return m < kN / 2 ? m : m - kN;
}

//! S( kx, ky ) in m^2 / ( rad/m )^2 at mode ( x, y ) of cascade c, its band only; the wind along +x
inline double spectrum( int s, int c, int x, int y, const SetParams & p )
{
	const double dk = 2.0 * kPi / set( s ).L[c];
	const double kx = freq( x ) * dk, ky = freq( y ) * dk;
	const double K = std::hypot( kx, ky );
	double lo, hi;
	band( s, c, lo, hi );
	if ( !( K >= lo && K < hi && K > 0.0 ) )
		return 0.0;
	const double w = omega( K );
	const double th = std::atan2( ky, kx );
	const double Sw = p.aw > 0.0 ? p.aw * jonswapShape( w, p.wp, 3.3 ) * spreadDb( th, w, p.wp ) : 0.0;
	const double S2 = p.a2 > 0.0 ? p.a2 * jonswapShape( w, p.wp2, p.g2 ) * spreadLh( th, p.s2 ) : 0.0;
	return ( Sw + S2 ) * domega( K, w ) / K;
}

//! the resolved height variance, slope variance (mss) and curvature variance of a set over its 3 cascades
inline void captured( int s, const SetParams & p, double & v, double & m2, double & m4 )
{
	v = m2 = m4 = 0.0;
	for ( int c = 0; c < 3; c++ ) {
		const double dk = 2.0 * kPi / set( s ).L[c];
		double sv = 0.0, s2 = 0.0, s4 = 0.0;
		for ( int y = 0; y < kN; y++ )
			for ( int x = 0; x < kN; x++ ) {
				const double S = spectrum( s, c, x, y, p );
				if ( S == 0.0 )
					continue;
				const double kx = freq( x ) * dk, ky = freq( y ) * dk, K2 = kx * kx + ky * ky;
				sv += S;
				s2 += S * K2;
				s4 += S * K2 * K2;
			}
		v += sv * dk * dk;
		m2 += s2 * dk * dk;
		m4 += s4 * dk * dk;
	}
}

//! the per-set numbers for wind U (m/s) and the sea's swell (m), with the Cox-Munk cap
inline void params( double U, double swellM, SetParams out[2] )
{
	for ( int s = 0; s < 2; s++ ) {
		SetParams & p = out[s];
		const SetDef & S = set( s );
		p.hw = hsWind( U, S.fetch );
		p.wp = wpWind( U, S.fetch );
		p.aw = jonswapAlpha( p.hw, p.wp, 3.3 );
		if ( s == 0 ) {
			p.hs2 = swellM;
			p.wp2 = 2.0 * kPi / kSwellTp;
			p.g2 = kSwellGamma;
			p.s2 = kSwellS;
		} else {
			p.hs2 = kRippleHs;
			p.wp2 = 2.0 * kPi / kRippleTp;
			p.g2 = kRippleGamma;
			p.s2 = kRippleS;
		}
		p.a2 = jonswapAlpha( p.hs2, p.wp2, p.g2 );
		p.fw = 1.0;
	}
	for ( int s = 0; s < 2; s++ ) {
		SetParams & p = out[s];
		const double cm = set( s ).cm[0] + set( s ).cm[1] * U;
		double v, m2w, m2r, m4;
		SetParams w = p;
		w.a2 = 0.0;
		captured( s, w, v, m2w, m4 );
		SetParams r = p;
		r.aw = 0.0;
		captured( s, r, v, m2r, m4 );
		if ( m2w > 0.0 && m2w + m2r > cm )
			p.fw = std::max( 0.0, cm - m2r ) / m2w;
		p.aw *= p.fw;
		p.hw *= std::sqrt( p.fw );
		p.hsSet = std::sqrt( p.hw * p.hw + p.hs2 * p.hs2 );
	}
}

//! the slope variance below the finest cascade (Cox-Munk minus what the cascades hold) and the curvature rms
inline void tail( int s, double U, const SetParams & p, double & mssTail, double & curvRms )
{
	double v, m2, m4;
	captured( s, p, v, m2, m4 );
	mssTail = std::max( 0.0, set( s ).cm[0] + set( s ).cm[1] * U - m2 );
	curvRms = std::sqrt( m4 );
}

constexpr int kCorrN = 16;	// the slope correlation table's distances: d0 2^( i / 2 ), d0 = the finest cascade's texel

/*! step 5: the correlation of the set's slope field at a separation d, E[ s( x ) . s( x + d ) ] / E[ s . s ] =
 *  sum S K^2 cos( k . d ) / sum S K^2 (Wiener-Khinchin), along the wind (out[0..15]) and across it (out[16..31]); the
 *  flow mixes two reads of the waves this far apart and renormalises by it (res/shaders/water_flowwaves.glsl) */
inline void slopeCorr( int s, const SetParams & p, double out[2 * kCorrN] )
{
	double d[kCorrN];
	for ( int i = 0; i < kCorrN; i++ ) {
		d[i] = set( s ).L[2] / kN * std::pow( 2.0, 0.5 * i );
		out[i] = out[kCorrN + i] = 0.0;
	}
	double norm = 0.0;
	for ( int c = 0; c < 3; c++ ) {
		const double dk = 2.0 * kPi / set( s ).L[c];
		for ( int y = 0; y < kN; y++ )
			for ( int x = 0; x < kN; x++ ) {
				const double S = spectrum( s, c, x, y, p );
				if ( S == 0.0 )
					continue;
				const double kx = freq( x ) * dk, ky = freq( y ) * dk, w = S * ( kx * kx + ky * ky ) * dk * dk;
				norm += w;
				for ( int i = 0; i < kCorrN; i++ ) {
					out[i] += w * std::cos( kx * d[i] );
					out[kCorrN + i] += w * std::cos( ky * d[i] );
				}
			}
	}
	for ( int i = 0; i < 2 * kCorrN; i++ )
		out[i] = norm > 0.0 ? out[i] / norm : 0.0;
}

/*! h0 for the six tiles side by side: h0 RGBA = ( h0( k ), conj( h0( -k ) ) ), q = floor( w T / 2 pi ); both
 *  ( 6 x 128 ) x 128, row-major, x fastest (the textures' layout) */
inline void h0Tiles( const SetParams P[2], std::vector<float> & h0, std::vector<float> & q )
{
	const int W = kN * kTiles;
	h0.assign( size_t( W ) * kN * 4, 0.0f );
	q.assign( size_t( W ) * kN, 0.0f );
	std::vector<double> re( size_t( kN ) * kN ), im( size_t( kN ) * kN );
	for ( int t = 0; t < kTiles; t++ ) {
		const int s = t / 3, c = t % 3;
		const double dk = 2.0 * kPi / set( s ).L[c];
		for ( int y = 0; y < kN; y++ )
			for ( int x = 0; x < kN; x++ ) {
				const double amp = std::sqrt( spectrum( s, c, x, y, P[s] ) * dk * dk / 2.0 );
				const std::uint32_t hsh = lowbias32( std::uint32_t( ( t * kN + y ) * kN + x ) + 0x9E3779B9u );
				const double phi = double( hsh ) / 4294967296.0 * 2.0 * kPi;
				re[size_t( y ) * kN + x] = amp * std::cos( phi );
				im[size_t( y ) * kN + x] = amp * std::sin( phi );
			}
		for ( int y = 0; y < kN; y++ )
			for ( int x = 0; x < kN; x++ ) {
				const size_t i = size_t( y ) * kN + x, j = size_t( ( kN - y ) % kN ) * kN + size_t( ( kN - x ) % kN );
				const size_t o = size_t( y ) * W + size_t( t ) * kN + x;
				h0[o * 4 + 0] = float( re[i] );
				h0[o * 4 + 1] = float( im[i] );
				h0[o * 4 + 2] = float( re[j] );
				h0[o * 4 + 3] = float( -im[j] );
				const double K = std::hypot( freq( x ) * dk, freq( y ) * dk );
				q[o] = float( std::floor( omega( K ) * kLoop / ( 2.0 * kPi ) ) );
			}
	}
}

}	// namespace WwOcean

#endif
