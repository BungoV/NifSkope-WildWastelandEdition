#ifndef WW_ATMOMATH_H
#define WW_ATMOMATH_H

/* lane SKYA: the physical sky's pure CPU math (no GL, no Qt), shared by src/gl/atmosphere.cpp and the offline unit
 * test tests/atmomath (which compares it with tests/spells/atmo_model.py). The medium is Hillaire 2020's Earth
 * (also Bruneton 2017's demo constants); km units, sun irradiance 1 at the top of the atmosphere. */

#include <algorithm>
#include <cmath>

namespace WwAtmoMath
{

constexpr float kRb = 6360.0f, kRt = 6460.0f;
constexpr float kUnitKm = 0.0142875f / 1000.0f;	// one game unit (70 = 1 m)
constexpr float kRay[3] = { 5.802e-3f, 13.558e-3f, 33.1e-3f };
constexpr float kMieS = 3.996e-3f, kMieE = 4.40e-3f, kMieG = 0.8f;
constexpr float kOzone[3] = { 0.650e-3f, 1.881e-3f, 0.085e-3f };
constexpr float kPi = 3.14159265f;

inline float lum( const float c[3] )
{
	return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];
}

/* the haze rung (ATMO1, notes/atmores1/RESEARCH.md sec. 7): the fog reaches Max at Far; Koschmieder's visibility in
 * its general form, sigma = -ln(1 - min(Max, 0.98)) / Far, less the clear air's ground extinction, is a haze layer
 * flat up to the fog's Far Height Mid and e-folding over its Range (atmo_model.py haze_from_fog) */
inline void hazeFromFog( float fogFar, float fogMax, float fogMid, float fogRange, bool noHaze, float out[3] )
{
	out[0] = 0.0f;
	out[1] = 1.0f;
	out[2] = 0.0f;
	if ( fogFar <= 0.0f || fogMax <= 0.0f || noHaze )
		return;
	const float vKm = fogFar * kUnitKm;
	const float sigma = -std::log( 1.0f - std::min( fogMax, 0.98f ) ) / vKm;
	out[0] = std::clamp( sigma - kRay[1] - kMieE, 0.0f, 50.0f );
	out[1] = std::max( 0.02f, fogRange * kUnitKm );
	out[2] = std::max( 0.0f, fogMid * kUnitKm );
}

inline void odAt( float h, const float haze[3], bool noOzone, float e[3] )
{
	h = std::max( h, 0.0f );
	const float dr = std::exp( -h / 8.0f ), dm = std::exp( -h / 1.2f );
	const float dz = std::max( 0.0f, 1.0f - std::fabs( h - 25.0f ) / 15.0f );
	const float dh = haze[0] * std::exp( -std::max( h - haze[2], 0.0f ) / std::max( haze[1], 0.02f ) );
	for ( int c = 0; c < 3; c++ )
		e[c] = kRay[c] * dr + kMieE * dm + dh + ( noOzone ? 0.0f : kOzone[c] ) * dz;
}

//! the transmittance LUT's math: from height hKm along cos zenith mu to the top; 0 when the ray hits the ground
inline void cpuTrans( float hKm, float mu, const float haze[3], bool noOzone, float out[3] )
{
	const float r = kRb + std::max( hKm, 0.0f );
	if ( mu < 0.0f && r * r * ( mu * mu - 1.0f ) + kRb * kRb >= 0.0f ) {
		out[0] = out[1] = out[2] = 0.0f;
		return;
	}
	const float d = std::max( 0.0f, -r * mu + std::sqrt( std::max( 0.0f, r * r * ( mu * mu - 1.0f ) + kRt * kRt ) ) );
	constexpr int N = 256;
	double od[3] = { 0, 0, 0 };
	for ( int i = 0; i < N; i++ ) {
		// packed toward the start: the haze layer sits at the bottom
		const float a0 = float( i ) / N, a1 = float( i + 1 ) / N, am = 0.5f * ( a0 + a1 );
		const float t = d * am * am, w = d * ( a1 * a1 - a0 * a0 );
		const float rr = std::sqrt( r * r + t * t + 2.0f * r * mu * t );
		float e[3];
		odAt( rr - kRb, haze, noOzone, e );
		for ( int c = 0; c < 3; c++ )
			od[c] += double( e[c] ) * w;
	}
	for ( int c = 0; c < 3; c++ )
		out[c] = float( std::exp( -od[c] ) );
}

/* NIGHT1: the moon's brightness against the full moon at phase angle alpha (degrees, 0 full, 180 new):
 * Krisciunas & Schaefer 1991 (PASP 103, 1033), eq. 20: m = -12.73 + 0.026 |alpha| + 4e-9 alpha^4, so the ratio is
 * 10^(-0.4 (0.026 |alpha| + 4e-9 alpha^4)). The engine's eight phases (esmweather wwMoonPhase: 0 full .. 4 new .. 7)
 * sit 45 degrees apart. Red "nophase": 1 at every phase (the vanilla moon). */
inline float moonPhaseAlpha( int phase )
{
	if ( phase < 0 )
		return 0.0f;
	const int p = phase % 8;
	return 45.0f * float( p <= 4 ? p : 8 - p );
}

inline float moonPhaseFactor( int phase, bool redNoPhase )
{
	if ( redNoPhase || phase < 0 )
		return 1.0f;
	const float a = moonPhaseAlpha( phase );
	return std::pow( 10.0f, -0.4f * ( 0.026f * a + 4e-9f * a * a * a * a ) );
}

/* ATMOSUN1: the key light's colour from the transmittance toward the light (Hillaire 2020 sec. 4: the sun's
 * illuminance at a point = E_sun x T(mu)). k = the weather's Day key luminance / the 45-degree transmittance's
 * luminance, the same k the sky uses (atmosphere.cpp), so a 45-degree sun keeps the weather's Day brightness.
 *   mode 1 (Tint): the weather's own key luminance, the transmittance's colour (mu held at 0.02 or above)
 *   mode 2 (Full): k x T(mu), the colour and the brightness (0 once the light is under the horizon)
 * The ambient (both modes): the weather's ambient luminance, its colour turned by T(mu) against T(45 deg), so a
 * 45-degree sun keeps the weather's ambient colour exactly and a low sun warms it. */
inline void sunLight( int mode, float keyLum45, const float T[3], const float T45[3], const float vanilla[3],
	float out[3] )
{
	if ( mode == 2 ) {
		const float k = keyLum45 / std::max( 1e-4f, lum( T45 ) );
		for ( int c = 0; c < 3; c++ )
			out[c] = k * T[c];
		return;
	}
	const float lt = lum( T ), lv = lum( vanilla );
	for ( int c = 0; c < 3; c++ )
		out[c] = lt > 1e-6f ? lv * T[c] / lt : vanilla[c];
}

inline void ambientTint( const float T[3], const float T45[3], float rgb[3] )
{
	const float lt = lum( T ), l45 = lum( T45 ), lv = lum( rgb );
	if ( lt <= 1e-6f || l45 <= 1e-6f || lv <= 0.0f )
		return;
	float o[3];
	for ( int c = 0; c < 3; c++ ) {
		const float r45 = T45[c] / l45;
		o[c] = rgb[c] * ( r45 > 1e-6f ? ( T[c] / lt ) / r45 : 1.0f );
	}
	const float lo = lum( o );
	for ( int c = 0; c < 3; c++ )
		rgb[c] = lo > 1e-9f ? o[c] * lv / lo : rgb[c];
}

/* EXPO1: the frame's mean luminance with the sky in it. The imagespace measure (celllights.h THE IMAGESPACE) averages
 * the cell-lit pixels only; the sky's pixels (the rest of the frame, share 1 - cover) are taken at the sky-view
 * table's mean luminance over the upper hemisphere (sr-weighted), in the same scene units as the surfaces.
 * Red "skyzero": the sky counted as nothing (the frame mean over every pixel), which must fail the gate. */
inline float adaptedWithSky( float sceneMean, float cover, float skyMean, bool redSkyZero )
{
	cover = std::clamp( cover, 0.0f, 1.0f );
	if ( redSkyZero )
		return sceneMean * cover;
	return sceneMean * cover + std::max( skyMean, 0.0f ) * ( 1.0f - cover );
}

}	// namespace WwAtmoMath

#endif
