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

/* NIGHT1: a star's or the moon disc's light through the air toward cos zenith mu, against the zenith's (the weather's
 * star and moon colours are the zenith look): T(mu) / T(1), so the zenith keeps today's colour and the horizon fades
 * and reddens (Bruneton & Neyret 2008 sec. 4: a light outside the air reaches the eye times T). mu held at 0 or
 * above. Red "noext": 1 everywhere (today's flat stars), which must fail its gate. */
inline void starExtinction( float mu, const float haze[3], bool redNoExt, float out[3] )
{
	out[0] = out[1] = out[2] = 1.0f;
	if ( redNoExt )
		return;
	float tz[3], t[3];
	cpuTrans( 0.0f, 1.0f, haze, false, tz );
	cpuTrans( 0.0f, std::max( mu, 0.0f ), haze, false, t );
	for ( int c = 0; c < 3; c++ )
		out[c] = tz[c] > 1e-6f ? std::min( 1.0f, t[c] / tz[c] ) : 0.0f;
}

/* ATMOSUN1: the key light's colour from the transmittance toward the light (Hillaire 2020 sec. 4: the sun's
 * illuminance at a point = E_sun x T(mu)). k = the weather's Day key luminance / the 45-degree transmittance's
 * luminance, the same k the sky uses (atmosphere.cpp), so a 45-degree sun keeps the weather's Day brightness.
 *   mode 1 (Tint): the weather's own key luminance, the transmittance's colour (mu held at 0.02 or above)
 *   mode 2 (Full): k x T(mu), the colour and the brightness (0 once the light is under the horizon) */
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

/* ATMOSUN1: the sky's irradiance on a level ground (cosine-weighted upper hemisphere) with the sun at cos zenith
 * mus, single scattering (Rayleigh + Mie, Hillaire 2020 eq. 3 without the MS term), sun irradiance 1. Only its
 * COLOUR is used (the ambient's tint), so the missing multiple scattering is checked against the model's full sky
 * (tests/spells/atmomath_check.py, gate "sky irradiance"). 8 x 16 view rays, 16 steps, 24-step light rays: ~1 ms. */
inline void skyIrradiance( float mus, const float haze[3], float out[3] )
{
	constexpr int NE = 8, NA = 16, NS = 16;
	const float sx = std::sqrt( std::max( 0.0f, 1.0f - mus * mus ) ), sz = mus;
	double acc[3] = { 0, 0, 0 }, wsum = 0;
	for ( int i = 0; i < NE; i++ ) {
		const float el = ( float( i ) + 0.5f ) / NE * 0.5f * kPi;
		const float ce = std::cos( el ), se = std::sin( el );
		for ( int j = 0; j < NA; j++ ) {
			const float az = ( float( j ) + 0.5f ) / NA * 2.0f * kPi;
			const float vx = ce * std::cos( az ), vy = ce * std::sin( az ), vz = se;
			const float w = ce * se;	// cos(theta) dOmega on the elevation grid
			const float nu = vx * sx + vz * sz;
			const float pr = 3.0f / ( 16.0f * kPi ) * ( 1.0f + nu * nu );
			const float g = kMieG, pm = ( 1.0f - g * g ) / ( 4.0f * kPi * std::pow( std::max( 1e-4f, 1.0f + g * g - 2.0f * g * nu ), 1.5f ) );
			// the view ray to the top of the air, steps packed toward the ground (the haze sits there)
			const float r0 = kRb;
			const float d = -r0 * vz + std::sqrt( r0 * r0 * ( vz * vz - 1.0f ) + kRt * kRt );
			double od[3] = { 0, 0, 0 }, L[3] = { 0, 0, 0 };
			for ( int k = 0; k < NS; k++ ) {
				const float a0 = float( k ) / NS, a1 = float( k + 1 ) / NS, am = 0.5f * ( a0 + a1 );
				const float t = d * am * am, dt = d * ( a1 * a1 - a0 * a0 );
				const float px = vx * t, py = vy * t, pz = r0 + vz * t;
				const float r = std::sqrt( px * px + py * py + pz * pz );
				const float h = std::max( r - kRb, 0.0f );
				float e[3];
				odAt( h, haze, false, e );
				const float muS = ( px * sx + pz * sz ) / r;
				float Ts[3];
				// the light ray (cpuTrans's math, fewer steps)
				if ( muS < 0.0f && r * r * ( muS * muS - 1.0f ) + kRb * kRb >= 0.0f ) {
					Ts[0] = Ts[1] = Ts[2] = 0.0f;
				} else {
					const float dl = std::max( 0.0f, -r * muS + std::sqrt( std::max( 0.0f, r * r * ( muS * muS - 1.0f ) + kRt * kRt ) ) );
					constexpr int NL = 24;
					double ol[3] = { 0, 0, 0 };
					for ( int q = 0; q < NL; q++ ) {
						const float b0 = float( q ) / NL, b1 = float( q + 1 ) / NL, bm = 0.5f * ( b0 + b1 );
						const float tt = dl * bm * bm, ww = dl * ( b1 * b1 - b0 * b0 );
						const float rr = std::sqrt( r * r + tt * tt + 2.0f * r * muS * tt );
						float el2[3];
						odAt( rr - kRb, haze, false, el2 );
						for ( int c = 0; c < 3; c++ )
							ol[c] += double( el2[c] ) * ww;
					}
					for ( int c = 0; c < 3; c++ )
						Ts[c] = float( std::exp( -ol[c] ) );
				}
				const float dr = std::exp( -h / 8.0f ), dm = std::exp( -h / 1.2f );
				const float dh = haze[0] * std::exp( -std::max( h - haze[2], 0.0f ) / std::max( haze[1], 0.02f ) );
				for ( int c = 0; c < 3; c++ ) {
					const double tv = std::exp( -( od[c] + 0.5 * double( e[c] ) * dt ) );
					const double sc = double( kRay[c] ) * dr * pr + double( kMieS * dm + dh * ( kMieS / kMieE ) ) * pm;
					L[c] += tv * sc * Ts[c] * dt;
					od[c] += double( e[c] ) * dt;
				}
			}
			for ( int c = 0; c < 3; c++ )
				acc[c] += L[c] * w;
			wsum += w;
		}
	}
	for ( int c = 0; c < 3; c++ )
		out[c] = float( acc[c] / wsum * kPi );	// E = pi x the cosine-weighted mean radiance
}

/* the ambient's tint: the weather's ambient luminance kept, its colour turned by the sky irradiance's colour now
 * against the same at 45 degrees (E, E45 from skyIrradiance), so a 45-degree sun keeps the weather's colour exactly */
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

/* ATMOSUN1: the whole Sunlight From Sky policy, as atmosphere.cpp ships it (the unit test calls this same function).
 * mu = cos zenith of the light (the sun by day, the moon by night: moonlight crosses the same air); keyLum45 = the
 * weather's Day (or Night) key luminance. The key: sunLight above (Tint holds mu at 0.02 or above, Full uses it as is).
 * The ambient rows (amb, each DALC side): ambientTint by the sky irradiance's colour (skyIrradiance), not by T: the
 * sky's light turns far less than the sun's (the model: R/B x2.4 from 45 to 3 degrees where T's is x17), mu held at
 * 0.02 or above in both modes.
 * Red "flat" (WW_ATMO_SUN_RED=flat): T := T(45 deg) at every height, the low sun no warmer, which must fail its gate. */
inline void skyKeyLight( int mode, float mu, const float haze[3], float keyLum45, bool redFlat, float sun[3],
	float ( *amb )[3], int nAmb )
{
	if ( mode <= 0 )
		return;
	float T45[3], T[3], E45[3], E[3];
	cpuTrans( 0.0f, 0.70710678f, haze, false, T45 );
	skyIrradiance( 0.70710678f, haze, E45 );
	if ( redFlat ) {
		std::copy( T45, T45 + 3, T );
		std::copy( E45, E45 + 3, E );
	} else {
		cpuTrans( 0.0f, mode == 2 ? mu : std::max( mu, 0.02f ), haze, false, T );
		skyIrradiance( std::max( mu, 0.02f ), haze, E );
	}
	const float v[3] = { sun[0], sun[1], sun[2] };
	sunLight( mode, keyLum45, T, T45, v, sun );
	for ( int i = 0; i < nAmb; i++ )
		ambientTint( E, E45, amb[i] );
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
