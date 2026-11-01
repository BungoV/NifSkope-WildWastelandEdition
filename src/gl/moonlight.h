#ifndef WW_MOONLIGHT_H
#define WW_MOONLIGHT_H

/* lane FIX1101A (moon, LATE ITEMs 24b / 24c / 24d / 27f): the one key light of the cell view round the clock, the pure
 * math (no GL, no Qt; the twin is tests/moonlight1101a/moontwin.py). Under Sunlight From Sky = Full, Moon Phase and
 * Stars on and Sun Path = Real (lookdevstage.cpp computeLight):
 *   sun part  = the weather's Day key through the air toward the REAL sun (k x T(mu), 0 under the horizon);
 *   moon part = the weather's Night key (the record: the full moon at 45 degrees as the night-adapted eye sees it)
 *               x Krisciunas & Schaefer 1991 (eqs. 8-9) at our globe's phase angle x T toward our globe x nightWeight;
 *   the light = their sum, its direction the luminance-weighted slerp of the two (z held at 0.02 or above).
 * Before (the red WW_MOON_RED=handover): the key switched Day -> Night key at the engine's D (19:04:30 under
 * DefaultClimate) and, after the real sunset, the light slerped toward the ENGINE's light, still its day sun at >= 27.6
 * degrees (fSunShadowMinAngle) at the Day key: a ghost sun 17:20-19:04 at 0.65 of noon, then a x14 drop at D.
 * The physical illuminances (cited, notes/fix1101a/moon/MOON_TABLE.md): full moon 0.313 lx above the air (K&S eq. 8-9),
 * sun 125.9 klx (V -26.74, NASA Sun fact sheet), ratio 2.49e-6; CommonwealthClear's Night key is 1/13.8 of its Day key,
 * so the record bakes ~14.8 EV of night eye adaptation in (x29157): the frame's brightness at night is that exposure,
 * never a moon brighter than its record. */

#include <algorithm>
#include <cmath>

namespace wwmoon {

inline float lumOf( const float c[3] )
{
	return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];
}

//! the night light's share by the sun's elevation (degrees): 0 at the horizon, 1 by -6 (civil dusk), smoothstep (the
//! crossfade SunSync's direction already used)
inline float nightWeight( float sunElevDeg )
{
	const float w0 = std::clamp( -sunElevDeg / 6.0f, 0.0f, 1.0f );
	return w0 * w0 * ( 3.0f - 2.0f * w0 );
}

//! our globe's phase angle (degrees, 0 full) for a sun at elongation e from the moon (skyephem.h elongationDeg: 0 new,
//! 180 full; skydisc.h sunFromElongation + phaseAngleDeg)
inline double alphaFromElongation( double elongDeg )
{
	constexpr double kR = 3.14159265358979323846 / 180.0;
	return 180.0 - std::acos( std::clamp( std::cos( elongDeg * kR ), -1.0, 1.0 ) ) / kR;
}

/* the one light: rgb = sun + moon; dir = slerp(sunDir, moonDir, moon's luminance share); both dark: the moon's
 * direction once the night weight passes 0.5. z held at 0.02 or above (the light never from under the ground). */
inline void combine( const float sun[3], const float sunDir[3], const float moon[3], const float moonDir[3], float wN,
	float rgb[3], float dir[3] )
{
	const float ls = lumOf( sun ), lm = lumOf( moon );
	for ( int c = 0; c < 3; c++ )
		rgb[c] = sun[c] + moon[c];
	const double share = ls + lm > 1e-12f ? double( lm ) / double( ls + lm ) : ( wN >= 0.5f ? 1.0 : 0.0 );
	double a[3], b[3];
	double la = 0.0, lb = 0.0;
	for ( int c = 0; c < 3; c++ ) {
		a[c] = sunDir[c];
		b[c] = moonDir[c];
		la += a[c] * a[c];
		lb += b[c] * b[c];
	}
	la = std::sqrt( la );
	lb = std::sqrt( lb );
	for ( int c = 0; c < 3; c++ ) {
		a[c] = la > 1e-12 ? a[c] / la : ( c == 2 ? 1.0 : 0.0 );
		b[c] = lb > 1e-12 ? b[c] / lb : ( c == 2 ? 1.0 : 0.0 );
	}
	const double cosw = std::clamp( a[0] * b[0] + a[1] * b[1] + a[2] * b[2], -1.0, 1.0 );
	const double om = std::acos( cosw );
	double o[3];
	if ( om < 1e-4 || om > 3.14159265358979323846 - 1e-3 ) {
		for ( int c = 0; c < 3; c++ )
			o[c] = a[c] * ( 1.0 - share ) + b[c] * share;
	} else {
		const double s = std::sin( om ), wa = std::sin( ( 1.0 - share ) * om ) / s, wb = std::sin( share * om ) / s;
		for ( int c = 0; c < 3; c++ )
			o[c] = a[c] * wa + b[c] * wb;
	}
	double lo = std::sqrt( o[0] * o[0] + o[1] * o[1] + o[2] * o[2] );
	if ( !( lo > 1e-12 ) ) {
		for ( int c = 0; c < 3; c++ )
			o[c] = b[c];
		lo = 1.0;
	}
	for ( int c = 0; c < 3; c++ )
		o[c] /= lo;
	if ( o[2] < 0.02 ) {
		double h = std::sqrt( o[0] * o[0] + o[1] * o[1] );
		const double hx = h > 1e-12 ? o[0] / h : 1.0, hy = h > 1e-12 ? o[1] / h : 0.0;
		const double c2 = std::sqrt( 1.0 - 0.02 * 0.02 );
		o[0] = hx * c2;
		o[1] = hy * c2;
		o[2] = 0.02;
	}
	for ( int c = 0; c < 3; c++ )
		dir[c] = float( o[c] );
}

/* LATE ITEM 24d(2): the drawn sun disc (and our moon globe) through the Volumetric Air. The sky pass draws the disc
 * through the physical sky's own air (T_sky toward it); the air's composite then keeps clamp(T_air / T_sky) of it
 * (cell_volsky.frag wwSkyBandT), T_air = the bands x the ground layer along the view. The light (30dfaadd) is dimmed by
 * T_sky's twin and the ground layer only: the bands ARE the records' fog, already in the weather's colours. So the disc
 * was dimmed by the bands on top (Foggy noon at c3: the seen disc x0.27 of the light's own dimming). The gain the disc
 * takes before the composite so that it ends at T_sky x T_layer like its light: max(T_layer, T_sky / T_band).
 * med = (sigma0 per unit, band base zb, band 1/H, top), layer = (near band weight, zn, 1/Hn, ground strength s),
 * ground = (zg, 1/Hg); zc = the camera's height in the fog frame (lookdev_fog.glsl wwAirColLaw's terms). */
inline float airColOne( float lo, float hi, float z0, float invH )	// lookdev_fog.glsl wwAirColOne
{
	if ( hi <= lo )
		return 0.0f;
	if ( invH <= 0.0f )
		return hi - lo;
	const float below = std::max( std::min( hi, z0 ) - lo, 0.0f );
	const float a = std::max( lo, z0 );
	if ( hi <= a )
		return below;
	const float w = hi - a, u = w * invH;
	const float e1 = u > 1e-3f ? ( 1.0f - std::exp( -u ) ) / u : 1.0f - u * ( 0.5f - u / 6.0f );
	return below + std::exp( -( a - z0 ) * invH ) * w * e1;
}

//! the bands' transmittance (no ground layer) from height zc to the air's top along cos zenith mu
inline float airBandT( float zc, float mu, const float med[4], const float layer[4] )
{
	const float top = med[3];
	float c = airColOne( zc, top, med[1], med[2] );
	if ( layer[0] > 0.0f )
		c = ( 1.0f - layer[0] ) * c + layer[0] * airColOne( zc, top, layer[1], layer[2] );
	return std::exp( -med[0] * c / std::max( mu, 0.02f ) );
}

//! the ground layer's transmittance from zc along mu (cell_airmed.glsl airTupSun's law)
inline float airLayerT( float zc, float mu, const float med[4], const float layer[4], const float ground[2] )
{
	if ( zc >= med[3] || layer[3] <= 0.0f )
		return 1.0f;
	return std::exp( -med[0] * layer[3] * airColOne( zc, med[3], ground[0], ground[1] ) / std::max( mu, 0.02f ) );
}

//! the disc's gain (>= T_layer; 1 with no air, or the body under the horizon)
inline float discAirGain( float zc, float mu, float tSky, const float med[4], const float layer[4], const float ground[2] )
{
	if ( !( med[0] > 0.0f ) || mu <= 0.0f )
		return 1.0f;
	const float tb = airBandT( zc, mu, med, layer ), tl = airLayerT( zc, mu, med, layer, ground );
	return std::clamp( std::max( tl, tSky / std::max( tb, 1e-6f ) ), 0.0f, 1e4f );
}

}	// namespace wwmoon

#endif
