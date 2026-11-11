#ifndef WW_ATMOMATH_H
#define WW_ATMOMATH_H

/* lane SKYA: the physical sky's pure CPU math (no GL, no Qt), shared by src/features/sky/atmosphere.cpp and the offline unit
 * test tests/features/sky/atmomath (which compares it with tests/features/sky/atmo_model.py). The medium is Hillaire 2020's Earth
 * (also Bruneton 2017's demo constants); km units, sun irradiance 1 at the top of the atmosphere. */

#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>

namespace WwAtmoMath
{

constexpr float kRb = 6360.0f, kRt = 6460.0f;
constexpr float kUnitKm = 0.0142875f / 1000.0f;	// one game unit (70 = 1 m)
constexpr float kRay[3] = { 5.802e-3f, 13.558e-3f, 33.1e-3f };
constexpr float kMieS = 3.996e-3f, kMieE = 4.40e-3f, kMieG = 0.8f;
constexpr float kOzone[3] = { 0.650e-3f, 1.881e-3f, 0.085e-3f };
/* lane WEATHERRECX1: the haze layer's optics (the weather .recx hazeType at its humidity, SPEC.md 1.3), set by
 * atmosphere.cpp before each frame's sky: albedo at 680 / 550 / 440 nm, spec = the extinction's spectral factor
 * (lambda / 550)^-angstrom (green 1: visibility is defined at 550 nm), g = the Mie phase. The defaults are the grey
 * haze before 1.3 exactly (albedo kMieS / kMieE, no slope, kMieG) */
struct HazeOptics
{
	float albedo[3] = { kMieS / kMieE, kMieS / kMieE, kMieS / kMieE };
	float spec[3] = { 1.0f, 1.0f, 1.0f };
	float g = kMieG;
	bool greyMs = false;	//!< NIGHTHAZE1 red WW_ATMO_RED=greyms: the CPU twin's haze scatters green's share in all three
	/* NIGHTHAZE1 (UE's volumetric fog model, one source for the sky, the near air and the clouds): glow = the haze's
	 * emissive in units of kNightFloorLum (0 = none; the glowing hazeType's airglow excess as the 557.7 nm line);
	 * cloudAlbedo = the cloud medium's albedo (0.99 grey = the march as before); inscatter / dir = the last-resort
	 * overrides (per unit sun irradiance at the top, as the LUTs; 0 = off), dirExp, dirStart in metres */
	float glow[3] = { 0.0f, 0.0f, 0.0f };
	bool glowSet = false;	//!< the file's hazeGlow (replaces the airglow excess); false = the airglow law as before
	float cloudAlbedo[3] = { 0.99f, 0.99f, 0.99f };
	float inscatter[3] = { 0.0f, 0.0f, 0.0f }, dir[3] = { 0.0f, 0.0f, 0.0f };
	float dirExp = 4.0f, dirStart = 0.0f;
	bool rgb680 = false;	//!< SKYLIGHT1 red WW_ATMO_RED=rgb680: the medium at the three wavelengths 680 / 550 / 440 nm
};
inline HazeOptics & hazeOptics()
{
	static HazeOptics h;
	return h;
}
/*! NIGHTHAZE1: the near air's albedo per colour from the same haze (one source: cellvolair.cpp airAlbedoD). The
 * hazeType's albedo over its luminance: the air's level stays the records' fit, the colour is the haze's. A grey
 * albedo gives exactly 1. */
inline void hazeAirAlbedo( float out[3] )
{
	const HazeOptics & o = hazeOptics();
	const float l = 0.2126f * o.albedo[0] + 0.7152f * o.albedo[1] + 0.0722f * o.albedo[2];
	const bool grey = o.albedo[0] == o.albedo[1] && o.albedo[1] == o.albedo[2];
	for ( int c = 0; c < 3; c++ )
		out[c] = grey || !( l > 1e-6f ) ? 1.0f : o.albedo[c] / l;
}
//! NIGHTHAZE1: one number per optics state, for the CPU tables' and memos' keys (a new hazeType rebuilds them)
inline float hazeOpticsStamp()
{
	const HazeOptics & o = hazeOptics();
	return o.spec[0] + 3.0f * o.spec[2] + 7.0f * o.albedo[0] + 11.0f * o.albedo[1] + 13.0f * o.albedo[2] + 17.0f * o.g
		+ ( o.greyMs ? 19.0f : 0.0f ) + 23.0f * o.glow[0] + 29.0f * o.glow[1] + 31.0f * o.glow[2] + ( o.glowSet ? 37.0f : 0.0f )
		+ ( o.rgb680 ? 41.0f : 0.0f );
}
/* SKYLIGHT1 (night air brown): the medium per sRGB channel, not at one wavelength each. A channel is a band; its
 * extinction is the one that gives the band-integrated transmittance -ln T / airmass, fitted over airmass 1..4 for a
 * 5778 K sun through the linear sRGB matrix (IEC 61966-2-1) of the CIE 1931 CMF (Wyman, Sloan & Shirley 2013).
 * Rayleigh: Hansen & Travis 1974 eq. 2.29 over an 8 km scale height; ozone: the Chappuis band cross-sections of
 * Bruneton 2017's demo (Serdyuchenko et al. 2014; its 680 / 550 / 440 nm entries are exactly kOzone) over the
 * 15 km tent, the blue fit (-4e-5) clamped to 0. The single wavelengths made a 33-degree sun R/B 1.63 where the
 * spectrum gives 1.45 (aerosol optical depth 0.1); the band values give 1.450. Tool: notes/skylight1/tools/spectral_rgb.py.
 * The haze's Angstrom slope reads the band's aerosol wavelength 640 / 550 / 440 nm (the fit at alpha 1.3; green stays
 * 550: visibility is defined there). Red WW_ATMO_RED=rgb680: kRay, kOzone and 680 nm again. */
constexpr float kRayBand[3] = { 6.3605e-3f, 12.530e-3f, 29.964e-3f };
constexpr float kOzoneBand[3] = { 2.4655e-3f, 1.7854e-3f, 0.0f };
constexpr float kHazeLambdaBand[3] = { 640.0f, 550.0f, 440.0f }, kHazeLambdaRgb[3] = { 680.0f, 550.0f, 440.0f };
inline const float * rayK()
{
	return hazeOptics().rgb680 ? kRay : kRayBand;
}
inline const float * ozoneK()
{
	return hazeOptics().rgb680 ? kOzone : kOzoneBand;
}
inline const float * hazeLambda()
{
	return hazeOptics().rgb680 ? kHazeLambdaRgb : kHazeLambdaBand;
}
constexpr float kPi = 3.14159265f;
/* lane FIX1101A (atmo): the moonless night sky's own radiance (airglow, zodiacal light, unresolved stars) at the zenith
 * above the air, per unit sun irradiance at the top (the LUTs' unit): 4e-4 cd/m2 / 128 klx / T(zenith, clear) 0.88,
 * colour B - V ~ 0.9 (redder than the sun) as linear sRGB with luminance 1. Leinert et al. 1998 (A&AS 127): the natural
 * dark sky reads 21-22 V mag / arcsec2 = 1.7e-4 .. 4.3e-4 cd/m2 at the zenith. atmo_sky.frag atFloor; red nofloor = 0. */
constexpr float kNightFloorLum = 4.0e-4f / 128000.0f / 0.88f;
constexpr float kNightFloor[3] = { kNightFloorLum * 1.0964f, kNightFloorLum * 0.9967f, kNightFloorLum * 0.7475f };
/* lane MODULES1 (radstorm green, 11-09): the airglow a glowing haze adds above the natural floor (airglow > 1) is the
 * oxygen 557.7 nm line of excited air. CIE 1931 CMF at 557.7 nm (555/560 interpolated): X 0.5566, Y 0.9973, Z 0.0048
 * -> linear sRGB 0.2687, 1.3354, -0.1678; B clipped to 0 (outside the gamut), luminance 1. Red WW_ATMO_RED=glowgrey:
 * the excess keeps the floor's colour (before). */
constexpr float kGreenLine5577[3] = { 0.2655f, 1.3193f, 0.0f };

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

inline void odAt( float h, const float haze[3], bool noOzone, float e[3], float ozone = 1.0f )
{
	h = std::max( h, 0.0f );
	const float dr = std::exp( -h / 8.0f ), dm = std::exp( -h / 1.2f );
	const float dz = std::max( 0.0f, 1.0f - std::fabs( h - 25.0f ) / 15.0f );
	const float dh = haze[0] * std::exp( -std::max( h - haze[2], 0.0f ) / std::max( haze[1], 0.02f ) );
	const float * ray = rayK(), * oz = ozoneK();
	for ( int c = 0; c < 3; c++ )
		e[c] = ray[c] * dr + kMieE * dm + dh * hazeOptics().spec[c] + ( noOzone ? 0.0f : oz[c] * ozone ) * dz;	// WX1: ozone x 1 = exact
}

//! the transmittance LUT's math: from height hKm along cos zenith mu to the top; 0 when the ray hits the ground
inline void cpuTrans( float hKm, float mu, const float haze[3], bool noOzone, float out[3], float ozone = 1.0f )
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
		odAt( rr - kRb, haze, noOzone, e, ozone );
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
 * (tests/features/sky/atmomath_check.py, gate "sky irradiance"). 8 x 16 view rays, 16 steps, 24-step light rays: ~1 ms. */
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
			const float g = hazeOptics().g, pm = ( 1.0f - g * g ) / ( 4.0f * kPi * std::pow( std::max( 1e-4f, 1.0f + g * g - 2.0f * g * nu ), 1.5f ) );
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
					const double sc = double( rayK()[c] ) * dr * pr + double( kMieS * dm + dh * hazeOptics().spec[c] * hazeOptics().albedo[c] ) * pm;
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

/* FIX1101A (LATE ITEM 15d, bungo 11-01 "does our GI not take sky into consideration?"): the physical sky's radiance seen
 * from the ground along unit view v (v.z > 0) with the sun toward unit s (world frame, z up), sun irradiance 1: the
 * single-scattering sum skyIrradiance integrates, for any sun azimuth (its sun sits in the x-z plane). */
inline void skyRadianceSingle( const float v[3], const float s[3], const float haze[3], float L[3] )
{
	constexpr int NS = 16;
	const float nu = v[0] * s[0] + v[1] * s[1] + v[2] * s[2];
	const float pr = 3.0f / ( 16.0f * kPi ) * ( 1.0f + nu * nu );
	const float g = hazeOptics().g, pm = ( 1.0f - g * g ) / ( 4.0f * kPi * std::pow( std::max( 1e-4f, 1.0f + g * g - 2.0f * g * nu ), 1.5f ) );
	const float r0 = kRb, vz = v[2];
	const float d = -r0 * vz + std::sqrt( r0 * r0 * ( vz * vz - 1.0f ) + kRt * kRt );
	double od[3] = { 0, 0, 0 }, acc[3] = { 0, 0, 0 };
	for ( int k = 0; k < NS; k++ ) {
		const float a0 = float( k ) / NS, a1 = float( k + 1 ) / NS, am = 0.5f * ( a0 + a1 );
		const float t = d * am * am, dt = d * ( a1 * a1 - a0 * a0 );
		const float px = v[0] * t, py = v[1] * t, pz = r0 + vz * t;
		const float r = std::sqrt( px * px + py * py + pz * pz );
		const float h = std::max( r - kRb, 0.0f );
		float e[3];
		odAt( h, haze, false, e );
		const float muS = ( px * s[0] + py * s[1] + pz * s[2] ) / r;
		float Ts[3];
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
			const double sc = double( rayK()[c] ) * dr * pr + double( kMieS * dm + dh * hazeOptics().spec[c] * hazeOptics().albedo[c] ) * pm;
			acc[c] += tv * sc * Ts[c] * dt;
			od[c] += double( e[c] ) * dt;
		}
	}
	for ( int c = 0; c < 3; c++ )
		L[c] = float( acc[c] );
}
/* FIX1101A: the physical sky's irradiance on the six world axes (+X -X +Y -Y +Z -Z), the sky above the horizon only (the
 * ground below it is the GI's SKYBELOW rule), sun toward s, sun irradiance 1: 12 elevations x 32 azimuths of
 * skyRadianceSingle (~6 ms). +Z equals skyIrradiance at the same sun (the same model; tests/features/sky/gisky). */
inline void skyIrradianceAxes( const float s[3], const float haze[3], float out[6][3] )
{
	constexpr int NE = 12, NA = 32;
	double acc[6][3] = {};
	for ( int i = 0; i < NE; i++ ) {
		const float el = ( float( i ) + 0.5f ) / NE * 0.5f * kPi;
		const float ce = std::cos( el ), se = std::sin( el );
		for ( int j = 0; j < NA; j++ ) {
			const float az = ( float( j ) + 0.5f ) / NA * 2.0f * kPi;
			const float v[3] = { ce * std::cos( az ), ce * std::sin( az ), se };
			float L[3];
			skyRadianceSingle( v, s, haze, L );
			const double dw = double( ce ) * ( 0.5 * kPi / NE ) * ( 2.0 * kPi / NA );   // dOmega on the elevation grid
			const double cosA[6] = { std::max( 0.0f, v[0] ), std::max( 0.0f, -v[0] ), std::max( 0.0f, v[1] ), std::max( 0.0f, -v[1] ),
				std::max( 0.0f, v[2] ), 0.0 };
			for ( int a = 0; a < 6; a++ )
				for ( int c = 0; c < 3; c++ )
					acc[a][c] += double( L[c] ) * cosA[a] * dw;
		}
	}
	for ( int a = 0; a < 6; a++ )
		for ( int c = 0; c < 3; c++ )
			out[a][c] = float( acc[a][c] );
}

/* GISKYTWIN (11-02): the DRAWN sky's own math on the CPU, so the GI's sky is the sky the view shows. skyIrradianceAxes above
 * (single scattering, 16 view steps from the ground) read +Z R/B 0.87 at CommonwealthClear noon's haze (0.513, 0.214, 0)
 * where the drawn sky reads 0.53 (notes/giskytwin/RESULTS.md): three causes, each measured on the real shaders:
 *   - no multiple scattering: the drawn sky adds Hillaire 2020's MS LUT (Psi x scattering), mostly blue; single-scatter
 *     R/B 0.78 -> 0.62 with it (the haze's single scattering is near-white, its 2nd+ orders take the Rayleigh blue)
 *   - the quadrature: 16 midpoint steps over a 0.21 km haze scale height over-count the near-white haze (0.87 vs 0.78)
 *   - the height: the drawn sky stands at the camera (atmosphere.cpp hKm), this at the ground; 90 m up sees a third less
 *     haze overhead (0.62 -> 0.53)
 * So this is atmo_transmittance.frag, atmo_ms.frag and atmo_common.glsl atSkyMarch ported 1:1 (the same LUT sizes, uv
 * maps, sample points and bilinear reads), integrated on the six axes from height hKm. Red: skyIrradianceAxes (today's). */
/* NIGHTHAZE1: the haze scatters per channel as atmo_common.glsl atMedium does (dh x spec x albedo). It took green's
 * share in all three while the extinction was per channel, so red scattered more than it lost (albedo 1.08 at
 * humidity 100) and blue less: the twin's night horizon read 1 : 0.68 : 0.46 where the drawn sky reads 1 : 0.94 : 0.91. */
inline void skyMedium( float h, const float haze[3], float ozone, float rs[3], float ms[3], float e[3] )
{
	h = std::max( h, 0.0f );
	const float dr = std::exp( -h / 8.0f ), dm = std::exp( -h / 1.2f );
	const float dz = std::max( 0.0f, 1.0f - std::fabs( h - 25.0f ) / 15.0f );
	const float dh = haze[0] * std::exp( -std::max( h - haze[2], 0.0f ) / std::max( haze[1], 0.02f ) );
	const HazeOptics & ho = hazeOptics();
	for ( int c = 0; c < 3; c++ ) {
		const int k = ho.greyMs ? 1 : c;
		ms[c] = kMieS * dm + dh * ho.spec[k] * ho.albedo[k];
		rs[c] = rayK()[c] * dr;
		e[c] = rs[c] + ( kMieE * dm + dh * hazeOptics().spec[c] ) + ozoneK()[c] * ozone * dz;
	}
}
inline float skyDistTop( float r, float mu )
{
	return std::max( 0.0f, -r * mu + std::sqrt( std::max( 0.0f, r * r * ( mu * mu - 1.0f ) + kRt * kRt ) ) );
}
inline float skyDistGround( float r, float mu )
{
	return std::max( 0.0f, -r * mu - std::sqrt( std::max( 0.0f, r * r * ( mu * mu - 1.0f ) + kRb * kRb ) ) );
}
inline bool skyHitsGround( float r, float mu )
{
	return mu < 0.0f && r * r * ( mu * mu - 1.0f ) + kRb * kRb >= 0.0f;
}
inline float skyUnitToSub( float u, float n ) { return 0.5f / n + u * ( 1.0f - 1.0f / n ); }
inline float skySubToUnit( float u, float n ) { return ( u - 0.5f / n ) / ( 1.0f - 1.0f / n ); }

struct SkyTables
{
	static constexpr int kTW = 256, kTH = 64;
	int msN = 0;
	float key[6] = { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };	// haze x3, ozone, msN, hazeOpticsStamp
	std::vector<float> trans, ms;	// rgb rows, row 0 = v 0 (GL's bottom row)
};

//! GL_LINEAR + clamp-to-edge read of an rgb table at uv
inline void skyTexel( const std::vector<float> & t, int w, int h, float u, float v, float out[3] )
{
	const float x = std::clamp( u * float( w ) - 0.5f, 0.0f, float( w - 1 ) );
	const float y = std::clamp( v * float( h ) - 0.5f, 0.0f, float( h - 1 ) );
	const int x0 = int( x ), y0 = int( y ), x1 = std::min( x0 + 1, w - 1 ), y1 = std::min( y0 + 1, h - 1 );
	const float fx = x - float( x0 ), fy = y - float( y0 );
	for ( int c = 0; c < 3; c++ ) {
		const float a = t[size_t( ( y0 * w + x0 ) * 3 + c )], b = t[size_t( ( y0 * w + x1 ) * 3 + c )];
		const float d = t[size_t( ( y1 * w + x0 ) * 3 + c )], f = t[size_t( ( y1 * w + x1 ) * 3 + c )];
		const float lo = a + ( b - a ) * fx, hi = d + ( f - d ) * fx;
		out[c] = lo + ( hi - lo ) * fy;
	}
}
//! atmo_common.glsl atTrans
inline void skyTrans( const SkyTables & T, float r, float mu, float out[3] )
{
	if ( skyHitsGround( r, mu ) ) {
		out[0] = out[1] = out[2] = 0.0f;
		return;
	}
	const float H = std::sqrt( kRt * kRt - kRb * kRb );
	const float rho = std::sqrt( std::max( 0.0f, r * r - kRb * kRb ) );
	const float d = skyDistTop( r, mu ), dmin = kRt - r, dmax = rho + H;
	skyTexel( T.trans, SkyTables::kTW, SkyTables::kTH, skyUnitToSub( ( d - dmin ) / std::max( dmax - dmin, 1e-6f ), float( SkyTables::kTW ) ),
		skyUnitToSub( rho / H, float( SkyTables::kTH ) ), out );
}
//! atmo_common.glsl atMsUv + the MS read
inline void skyMs( const SkyTables & T, float r, float mus, float out[3] )
{
	const float n = float( T.msN );
	const float hu = std::clamp( ( r - kRb ) / ( kRt - kRb ), 0.0f, 1.0f );
	skyTexel( T.ms, T.msN, T.msN, skyUnitToSub( std::clamp( mus * 0.5f + 0.5f, 0.0f, 1.0f ), n ), skyUnitToSub( std::sqrt( hu ), n ), out );
}

//! rows 0..n-1 of a table on up to 8 threads (each row writes only its own texels)
template <class F> void skyRows( int n, F && row )
{
	const int nt = int( std::max( 1u, std::min( 8u, std::thread::hardware_concurrency() ) ) );
	std::vector<std::thread> pool;
	for ( int t = 0; t < nt; t++ )
		pool.emplace_back( [&row, n, nt, t]() {
			for ( int j = t; j < n; j += nt )
				row( j );
		} );
	for ( auto & th : pool )
		th.join();
}

//! atmo_transmittance.frag + atmo_ms.frag (msN 32 = High+, 16 = Low, atmosphere.cpp); kept while haze, ozone and msN stand
inline void skyTablesBuild( const float haze[3], float ozone, int msN, SkyTables & T )
{
	const float key[6] = { haze[0], haze[1], haze[2], ozone, float( msN ), hazeOpticsStamp() };
	if ( std::equal( key, key + 6, T.key ) )
		return;
	std::copy( key, key + 6, T.key );
	T.msN = msN;
	const int W = SkyTables::kTW, Hh = SkyTables::kTH;
	T.trans.assign( size_t( W * Hh * 3 ), 0.0f );
	const float H = std::sqrt( kRt * kRt - kRb * kRb );
	skyRows( Hh, [&]( int j ) {
		for ( int i = 0; i < W; i++ ) {
			const float xm = skySubToUnit( ( float( i ) + 0.5f ) / float( W ), float( W ) );
			const float xr = skySubToUnit( ( float( j ) + 0.5f ) / float( Hh ), float( Hh ) );
			const float rho = H * xr, r = std::sqrt( rho * rho + kRb * kRb );
			const float dmin = kRt - r, dmax = rho + H, dd = dmin + xm * ( dmax - dmin );
			const float mu = dd == 0.0f ? 1.0f : std::clamp( ( H * H - rho * rho - dd * dd ) / ( 2.0f * r * dd ), -1.0f, 1.0f );
			const float d = skyDistTop( r, mu ), tm = std::clamp( -r * mu, 0.0f, d );
			constexpr int N = 40;
			double od[3] = { 0, 0, 0 };
			for ( int k = 0; k < N; k++ ) {
				const float a0 = float( k ) / N, a1 = float( k + 1 ) / N, am = ( a0 + a1 ) * 0.5f;
				const float tA = tm * ( 1.0f - ( 1.0f - am ) * ( 1.0f - am ) ), wA = tm * ( ( 1.0f - a0 ) * ( 1.0f - a0 ) - ( 1.0f - a1 ) * ( 1.0f - a1 ) );
				const float tB = tm + ( d - tm ) * am * am, wB = ( d - tm ) * ( a1 * a1 - a0 * a0 );
				float rs[3], ms[3], e[3];
				skyMedium( std::sqrt( r * r + tA * tA + 2.0f * r * mu * tA ) - kRb, haze, ozone, rs, ms, e );
				for ( int c = 0; c < 3; c++ )
					od[c] += double( e[c] ) * wA;
				skyMedium( std::sqrt( r * r + tB * tB + 2.0f * r * mu * tB ) - kRb, haze, ozone, rs, ms, e );
				for ( int c = 0; c < 3; c++ )
					od[c] += double( e[c] ) * wB;
			}
			for ( int c = 0; c < 3; c++ )
				T.trans[size_t( ( j * W + i ) * 3 + c )] = float( std::exp( -od[c] ) );
		}
	} );
	T.ms.assign( size_t( msN * msN * 3 ), 0.0f );
	const float n = float( msN );
	skyRows( msN, [&]( int j ) {
		for ( int i = 0; i < msN; i++ ) {
			const float mus = skySubToUnit( ( float( i ) + 0.5f ) / n, n ) * 2.0f - 1.0f;
			const float u = skySubToUnit( ( float( j ) + 0.5f ) / n, n );
			const float r = kRb + std::max( 0.01f, u * u * ( kRt - kRb ) );
			const float s[3] = { std::sqrt( std::max( 0.0f, 1.0f - mus * mus ) ), 0.0f, mus };
			constexpr int K = 8, N = 40;
			double L2[3] = { 0, 0, 0 }, F[3] = { 0, 0, 0 };
			for ( int a = 0; a < K; a++ )
				for ( int b = 0; b < K; b++ ) {
					const float th = 2.0f * kPi * ( float( a ) + 0.5f ) / float( K );
					const float ph = std::acos( 1.0f - 2.0f * ( float( b ) + 0.5f ) / float( K ) );
					const float v[3] = { std::sin( ph ) * std::cos( th ), std::sin( ph ) * std::sin( th ), std::cos( ph ) };
					const bool gnd = skyHitsGround( r, v[2] );
					const float d = gnd ? skyDistGround( r, v[2] ) : skyDistTop( r, v[2] );
					double thr[3] = { 1, 1, 1 }, L[3] = { 0, 0, 0 }, f[3] = { 0, 0, 0 };
					for ( int k = 0; k < N; k++ ) {
						const float a0 = float( k ) / N, a1 = float( k + 1 ) / N;
						const float dt = d * ( a1 * a1 - a0 * a0 ), t = d * 0.5f * ( a0 * a0 + a1 * a1 );
						const float p[3] = { v[0] * t, v[1] * t, r + v[2] * t };
						const float rp = std::sqrt( p[0] * p[0] + p[1] * p[1] + p[2] * p[2] );
						float rs[3], ms[3], e[3], Ts[3];
						skyMedium( rp - kRb, haze, ozone, rs, ms, e );
						skyTrans( T, rp, ( p[0] * s[0] + p[2] * s[2] ) / rp, Ts );
						for ( int c = 0; c < 3; c++ ) {
							const double sc = double( rs[c] ) + ms[c], Tst = std::exp( -e[c] * dt );
							const double ie = 1.0 / std::max( double( e[c] ), 1e-9 );
							L[c] += thr[c] * ( Ts[c] * sc / ( 4.0 * kPi ) ) * ( 1.0 - Tst ) * ie;
							f[c] += thr[c] * sc * ( 1.0 - Tst ) * ie;
							thr[c] *= Tst;
						}
					}
					if ( gnd ) {
						const float pg[3] = { v[0] * d, v[1] * d, r + v[2] * d };
						const float lg = std::sqrt( pg[0] * pg[0] + pg[1] * pg[1] + pg[2] * pg[2] );
						const float c0 = ( pg[0] * s[0] + pg[2] * s[2] ) / lg;
						float Tg[3];
						skyTrans( T, kRb + 1e-4f, c0, Tg );
						for ( int c = 0; c < 3; c++ )
							L[c] += thr[c] * Tg[c] * std::max( c0, 0.0f ) * 0.3 / kPi;
					}
					for ( int c = 0; c < 3; c++ ) {
						L2[c] += L[c];
						F[c] += f[c];
					}
				}
			for ( int c = 0; c < 3; c++ ) {
				const double l2 = L2[c] / ( K * K ), ff = F[c] / ( K * K );
				T.ms[size_t( ( j * msN + i ) * 3 + c )] = float( l2 / std::max( 1e-4, 1.0 - ff ) );
			}
		}
	} );
}

//! atmo_common.glsl atSkyMarch (single scattering + the MS term, 30 quadratic steps) for a view ray above the horizon
inline void skyMarch( const SkyTables & T, const float haze[3], float ozone, float r, const float v[3], const float s[3], double L[3] )
{
	const bool gnd = skyHitsGround( r, v[2] );
	const float d = gnd ? skyDistGround( r, v[2] ) : skyDistTop( r, v[2] );
	const float cvs = v[0] * s[0] + v[1] * s[1] + v[2] * s[2];
	const float pr = 3.0f / ( 16.0f * kPi ) * ( 1.0f + cvs * cvs );
	const float g = hazeOptics().g, pm = ( 1.0f - g * g ) / ( 4.0f * kPi * std::pow( std::max( 1e-4f, 1.0f + g * g - 2.0f * g * cvs ), 1.5f ) );
	constexpr int N = 30;
	double thr[3] = { 1, 1, 1 };
	L[0] = L[1] = L[2] = 0.0;
	float t0 = 0.0f;
	for ( int i = 0; i < N; i++ ) {
		const float a = float( i + 1 ) / float( N ), t1 = d * a * a, dt = t1 - t0, t = t0 + dt * 0.5f;
		t0 = t1;
		const float p[3] = { v[0] * t, v[1] * t, r + v[2] * t };
		const float rp = std::sqrt( p[0] * p[0] + p[1] * p[1] + p[2] * p[2] );
		const float mus = ( p[0] * s[0] + p[1] * s[1] + p[2] * s[2] ) / rp;
		float rs[3], ms[3], e[3], Ts[3], M[3];
		skyMedium( rp - kRb, haze, ozone, rs, ms, e );
		skyTrans( T, rp, mus, Ts );
		skyMs( T, rp, mus, M );
		for ( int c = 0; c < 3; c++ ) {
			const double S = double( Ts[c] ) * ( rs[c] * pr + ms[c] * pm ) + double( M[c] ) * ( rs[c] + ms[c] );
			const double Tst = std::exp( -double( e[c] ) * dt );
			L[c] += thr[c] * S * ( 1.0 - Tst ) / std::max( double( e[c] ), 1e-9 );
			thr[c] *= Tst;
		}
	}
}

/* the drawn sky's irradiance on the six world axes (+X -X +Y -Y +Z -Z) from height hKm, the sky above the horizon only (as
 * skyIrradianceAxes: the ground below is the GI's SKYBELOW rule; -Z = 0), sun toward s, sun irradiance 1. Elevations packed
 * toward the horizon (el = 90 deg u^2: the haze's bright band is a few degrees tall), NE x NA rays of skyMarch. */
inline void skyIrradianceAxesDrawn( const float s[3], const float haze[3], float hKm, float ozone, SkyTables & T, float out[6][3],
	int msN = 32, int NE = 24, int NA = 48 )
{
	skyTablesBuild( haze, ozone, msN, T );
	const float r = kRb + std::clamp( hKm, 0.002f, 80.0f );
	double acc[6][3] = {};
	for ( int i = 0; i < NE; i++ ) {
		const double u0 = double( i ) / NE, u1 = double( i + 1 ) / NE, um = 0.5 * ( u0 + u1 );
		const double el = 0.5 * kPi * um * um, del = 0.5 * kPi * ( u1 * u1 - u0 * u0 );
		const float ce = float( std::cos( el ) ), se = float( std::sin( el ) );
		for ( int j = 0; j < NA; j++ ) {
			const float az = ( float( j ) + 0.5f ) / NA * 2.0f * kPi;
			const float v[3] = { ce * std::cos( az ), ce * std::sin( az ), se };
			double L[3];
			skyMarch( T, haze, ozone, r, v, s, L );
			const double dw = double( ce ) * del * ( 2.0 * kPi / NA );
			const double cosA[5] = { std::max( 0.0f, v[0] ), std::max( 0.0f, -v[0] ), std::max( 0.0f, v[1] ), std::max( 0.0f, -v[1] ), v[2] };
			for ( int a = 0; a < 5; a++ )
				for ( int c = 0; c < 3; c++ )
					acc[a][c] += L[c] * cosA[a] * dw;
		}
	}
	for ( int a = 0; a < 6; a++ )
		for ( int c = 0; c < 3; c++ )
			out[a][c] = float( acc[a][c] );
}
/* FIX1101A: the GI's sky from the physical sky: each axis takes the physical irradiance's colour and its share against
 * +Z, scaled so +Z keeps the weather's own luminance there (the exposure the weather was tuned with); -Z keeps the
 * weather's (the ground's light, not the sky's). Red (the caller's): the weather's DALC as it was. */
inline void giSkyFromPhysical( const float phys[6][3], const float weather[6][3], float out[6][3] )
{
	const float lp = lum( phys[4] ), lw = lum( weather[4] );
	const float k = lp > 1e-9f ? lw / lp : 0.0f;
	for ( int a = 0; a < 6; a++ )
		for ( int c = 0; c < 3; c++ )
			out[a][c] = ( a == 5 || k <= 0.0f ) ? weather[a][c] : phys[a][c] * k;
}
/* GITRUTH1: the GI's sky from the physical sky at its ABSOLUTE level against the sun: phys is per unit top-of-atmosphere
 * sun irradiance, the frame's sun is that sun's transmittance T(mu) x a scale (Sunlight From Sky: x the imagespace
 * Sunlight Scale), so the sky takes the same scale k = lum(sun) / lum(T). -Z keeps the weather's (the ground's light).
 * giSkyFromPhysical anchors +Z to the weather's luminance instead: direct / diffuse horizontal 7.5 at a 33.5 deg sun,
 * where the IES clear sky gives 3.9 and this model 3.8 (notes/gitruth1/RESULTS.md section 4). Returns k (0 = unusable,
 * out = the weather's). */
inline float giSkyAbsolute( const float phys[6][3], const float T[3], const float sun[3], const float weather[6][3], float out[6][3] )
{
	const float lt = lum( T ), ls = lum( sun );
	const float k = ( lt > 1e-9f && ls > 0.0f ) ? ls / lt : 0.0f;
	for ( int a = 0; a < 6; a++ )
		for ( int c = 0; c < 3; c++ )
			out[a][c] = ( a == 5 || k <= 0.0f ) ? weather[a][c] : phys[a][c] * k;
	return k;
}
/* MODULES1: the ground's light on -Z, physically: a Lambertian ground of albedo rho under the horizontal irradiance
 * (direct sun x mu + the sky's +Z) reflects L = rho E / pi, and a down-facing axis gathers pi L = rho E. */
inline void giSkyGround( const float sun[3], float sunZ, float rho, float out[6][3] )
{
	for ( int c = 0; c < 3; c++ )
		out[5][c] = rho * ( sun[c] * std::max( sunZ, 0.0f ) + out[4][c] );
}
/* GITRUTH1: only the LEVEL from the physical sky: the weather's own sky (its colour and its axis shares) times one factor
 * f so +Z takes the physical sky's absolute luminance (giSkyAbsolute's +Z). The level is the input both sky models agree
 * is wrong (direct / diffuse 7.5 against 3.8); the colour is not settled (the CPU single-scatter twin reads +Z R/B 0.87 at
 * noon's haze where the drawn sky reads 0.53), so this keeps the weather's. -Z the weather's. Returns f (0 = unusable).
 * GISKYTWIN: the live twin is now skyIrradianceAxesDrawn (R/B 0.527 there, within 0.6 % of the GPU sky); the old one is
 * the red WW_CELL_RTDDGI_SKYTWIN_RED=1. Which colour the GI takes stays bungo's call (absolute vs level). */
inline float giSkyLevel( const float phys[6][3], const float T[3], const float sun[3], const float weather[6][3], float out[6][3] )
{
	float ab[6][3];
	const float k = giSkyAbsolute( phys, T, sun, weather, ab );
	const float lw = lum( weather[4] );
	const float f = ( k > 0.0f && lw > 1e-9f ) ? lum( ab[4] ) / lw : 0.0f;
	for ( int a = 0; a < 6; a++ )
		for ( int c = 0; c < 3; c++ )
			out[a][c] = ( a == 5 || f <= 0.0f ) ? weather[a][c] : weather[a][c] * f;
	return f;
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

/* EXPO1: the sky-view table's texture coordinate of a world direction (atmo_common.glsl atSkyDirToUv, 1:1):
 * Hillaire 2020 sec. 5.3, latitude squeezed toward the horizon, longitude = the angle to the light (0..pi). */
inline void skyDirToUv( float r, float cosZ, float cosAz, int w, int h, float & u, float & v )
{
	const float vh = std::sqrt( std::max( 0.0f, r * r - kRb * kRb ) );
	const float beta = std::acos( std::clamp( vh / r, -1.0f, 1.0f ) );
	const float zh = kPi - beta;
	const float za = std::acos( std::clamp( cosZ, -1.0f, 1.0f ) );
	if ( za < zh ) {
		const float c = 1.0f - za / zh;
		v = ( 1.0f - std::sqrt( std::max( 0.0f, c ) ) ) * 0.5f;
	} else {
		const float c = ( za - zh ) / std::max( beta, 1e-5f );
		v = std::sqrt( std::clamp( c, 0.0f, 1.0f ) ) * 0.5f + 0.5f;
	}
	u = std::sqrt( std::clamp( -cosAz * 0.5f + 0.5f, 0.0f, 1.0f ) );
	u = 0.5f / float( w ) + u * ( 1.0f - 1.0f / float( w ) );
	v = 0.5f / float( h ) + v * ( 1.0f - 1.0f / float( h ) );
}

/* EXPO1: the sky-view table's mean luminance over the upper hemisphere, each direction weighted by its solid angle
 * (24 zenith bands x 24 azimuths over 0..pi; the table is symmetric about the light), bilinear like the GPU. rgba =
 * the table read back (w x h RGBA floats); the measure's luminance weights (cellhdr.cpp isApply). Times k (pi k, the
 * sky's display scale) it is in the sky pass's scene units. Red "zenithw": each band weighted alike (not by its
 * solid angle), which must fail the gate. */
inline float skyViewMeanLum( const float * rgba, int w, int h, float r, bool redZenithW )
{
	constexpr int NZ = 24, NA = 24;
	double acc = 0.0, wsum = 0.0;
	for ( int i = 0; i < NZ; i++ ) {
		const float za = ( float( i ) + 0.5f ) / NZ * 0.5f * kPi;
		const float wz = redZenithW ? 1.0f : std::sin( za );
		for ( int j = 0; j < NA; j++ ) {
			const float az = ( float( j ) + 0.5f ) / NA * kPi;
			float u, v;
			skyDirToUv( r, std::cos( za ), std::cos( az ), w, h, u, v );
			const float x = std::clamp( u * float( w ) - 0.5f, 0.0f, float( w - 1 ) );
			const float y = std::clamp( v * float( h ) - 0.5f, 0.0f, float( h - 1 ) );
			const int x0 = std::min( int( x ), std::max( w - 2, 0 ) ), y0 = std::min( int( y ), std::max( h - 2, 0 ) );
			const int x1 = std::min( x0 + 1, w - 1 ), y1 = std::min( y0 + 1, h - 1 );
			const float fx = x - float( x0 ), fy = y - float( y0 );
			auto lumAt = [&]( int px, int py ) {
				const float * p = rgba + ( size_t( py ) * size_t( w ) + size_t( px ) ) * 4;
				return 0.2125f * p[0] + 0.7154f * p[1] + 0.0721f * p[2];
			};
			const float l = ( lumAt( x0, y0 ) * ( 1.0f - fx ) + lumAt( x1, y0 ) * fx ) * ( 1.0f - fy )
				+ ( lumAt( x0, y1 ) * ( 1.0f - fx ) + lumAt( x1, y1 ) * fx ) * fy;
			if ( std::isfinite( l ) ) {
				acc += double( l ) * wz;
				wsum += wz;
			}
		}
	}
	return wsum > 0.0 ? float( acc / wsum ) : 0.0f;
}

/* EXPO2: one table's luminance at (u, v), bilinear like the GPU (skyViewMeanLum's lookup). */
inline float tableLumAt( const float * rgba, int w, int h, float u, float v )
{
	const float x = std::clamp( u * float( w ) - 0.5f, 0.0f, float( w - 1 ) );
	const float y = std::clamp( v * float( h ) - 0.5f, 0.0f, float( h - 1 ) );
	const int x0 = std::min( int( x ), std::max( w - 2, 0 ) ), y0 = std::min( int( y ), std::max( h - 2, 0 ) );
	const int x1 = std::min( x0 + 1, w - 1 ), y1 = std::min( y0 + 1, h - 1 );
	const float fx = x - float( x0 ), fy = y - float( y0 );
	auto at = [&]( int px, int py ) {
		const float * p = rgba + ( size_t( py ) * size_t( w ) + size_t( px ) ) * 4;
		return 0.2125f * p[0] + 0.7154f * p[1] + 0.0721f * p[2];
	};
	return ( at( x0, y0 ) * ( 1.0f - fx ) + at( x1, y0 ) * fx ) * ( 1.0f - fy ) + ( at( x0, y1 ) * ( 1.0f - fx ) + at( x1, y1 ) * fx ) * fy;
}

/* EXPO2: the mean luminance of the sky pixels actually in view (EXPO1 took the whole upper hemisphere). share = the
 * measure's sky share per texel (qw x qh, row 0 = the bottom); a texel's world ray = f0 + fx (x + .5) / qw + fy (y + .5)
 * / qh (the frustum's corner rays, atmosphere.cpp buildAp). Blocks of up to 8 x 8 texels sum their share and look the
 * table up once at the block's centre (64 x 34 lookups at 1080p). tabs/K/light: the sun's and the moon's tables (a null
 * table is skipped), as atmo_sky.frag atLookup reads them. Returns -1 when no texel is sky. */
inline float skyMeanInView( const float * share, int qw, int qh, const float f0[3], const float fx[3], const float fy[3],
	const float * const tabs[2], const float K[2], const float light[2][3], int w, int h, float r, float * shareOut = nullptr )
{
	const int bx = std::max( 1, ( qw + 63 ) / 64 ), by = std::max( 1, ( qh + 63 ) / 64 );
	double acc = 0.0, wsum = 0.0;
	for ( int y0 = 0; y0 < qh; y0 += by )
		for ( int x0 = 0; x0 < qw; x0 += bx ) {
			const int x1 = std::min( x0 + bx, qw ), y1 = std::min( y0 + by, qh );
			double s = 0.0;
			for ( int y = y0; y < y1; y++ )
				for ( int x = x0; x < x1; x++ )
					s += std::clamp( share[size_t( y ) * size_t( qw ) + size_t( x )], 0.0f, 1.0f );
			if ( !( s > 0.0 ) )
				continue;
			const float a = 0.5f * float( x0 + x1 ) / float( qw ), b = 0.5f * float( y0 + y1 ) / float( qh );
			float d[3];
			for ( int c = 0; c < 3; c++ )
				d[c] = f0[c] + fx[c] * a + fy[c] * b;
			const float n = std::sqrt( d[0] * d[0] + d[1] * d[1] + d[2] * d[2] );
			if ( !( n > 1e-12f ) )
				continue;
			const float la = std::sqrt( d[0] * d[0] + d[1] * d[1] );
			float l = 0.0f;
			for ( int t = 0; t < 2; t++ ) {
				if ( !tabs[t] || !( K[t] > 0.0f ) )
					continue;
				const float lb = std::sqrt( light[t][0] * light[t][0] + light[t][1] * light[t][1] );
				const float cosAz = la > 1e-5f * n && lb > 1e-5f ? ( d[0] * light[t][0] + d[1] * light[t][1] ) / ( la * lb ) : 1.0f;
				float u, v;
				skyDirToUv( r, d[2] / n, cosAz, w, h, u, v );
				l += K[t] * tableLumAt( tabs[t], w, h, u, v );
			}
			if ( std::isfinite( l ) ) {
				acc += s * double( l );
				wsum += s;
			}
		}
	if ( shareOut )
		*shareOut = float( wsum / std::max( 1.0, double( qw ) * double( qh ) ) );
	return wsum > 0.0 ? float( acc / wsum ) : -1.0f;
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

/* ATMOCHECK1: the Sky Color row's Weather Tint keeps the physical sky's luminance and takes the weather dome's color
 * at the same elevation. The dome (Meshes\Sky\Atmosphere.nif) blends Horizon / Sky-Lower / Sky-Upper by its vertex
 * colors (lookdev_dome.frag); its vertices are grouped into rings by elevation once (tintRings), and each frame the
 * weather's colors make a table at kTintN elevations, 90 x (i / 16)^2 degrees (dense at the horizon). Red "flat":
 * the horizon's color at every elevation (the gradient lost), which must fail the gate. */
constexpr int kTintN = 17;

inline float tintElevation( int i )
{
	const float t = float( i ) / float( kTintN - 1 );
	return 90.0f * t * t;
}

//! ring means of the dome's (elevation deg, weights) points: rings within 0.05 deg merge; sorted ascending
inline void tintRings( const float * elDeg, const float ( *w )[3], int n, std::vector<float> & ringEl,
	std::vector<float> & ringW )
{
	ringEl.clear();
	ringW.clear();
	std::vector<int> o( size_t( std::max( n, 0 ) ) );
	for ( int i = 0; i < n; i++ )
		o[size_t( i )] = i;
	std::sort( o.begin(), o.end(), [&]( int a, int b ) { return elDeg[a] < elDeg[b]; } );
	for ( size_t k = 0; k < o.size(); ) {
		size_t e = k;
		double s = 0.0, a[3] = { 0.0, 0.0, 0.0 };
		while ( e < o.size() && elDeg[o[e]] - elDeg[o[k]] < 0.05f ) {
			s += elDeg[o[e]];
			for ( int c = 0; c < 3; c++ )
				a[c] += w[o[e]][c];
			e++;
		}
		const double m = double( e - k );
		ringEl.push_back( float( s / m ) );
		for ( int c = 0; c < 3; c++ )
			ringW.push_back( float( a[c] / m ) );
		k = e;
	}
}

//! the weather's linear sky color at each table elevation: scale x (wr H + wg L + wb U), the rings interpolated
inline void tintTable( const std::vector<float> & ringEl, const std::vector<float> & ringW, const float H[3],
	const float L[3], const float U[3], float scale, bool redFlat, float out[kTintN][3] )
{
	const int nr = int( ringEl.size() );
	for ( int i = 0; i < kTintN; i++ ) {
		const float el = redFlat ? 0.0f : tintElevation( i );
		float wt[3] = { 1.0f, 0.0f, 0.0f };
		if ( nr > 0 ) {
			int j = 0;
			while ( j + 1 < nr && ringEl[size_t( j + 1 )] < el )
				j++;
			const int k = std::min( j + 1, nr - 1 );
			const float span = ringEl[size_t( k )] - ringEl[size_t( j )];
			const float f = ( k == j || span <= 0.0f ) ? 0.0f : std::clamp( ( el - ringEl[size_t( j )] ) / span, 0.0f, 1.0f );
			for ( int c = 0; c < 3; c++ )
				wt[c] = ringW[size_t( j ) * 3 + size_t( c )] * ( 1.0f - f ) + ringW[size_t( k ) * 3 + size_t( c )] * f;
		}
		for ( int c = 0; c < 3; c++ )
			out[i][c] = scale * ( wt[0] * H[c] + wt[1] * L[c] + wt[2] * U[c] );
	}
}

//! the physical color's luminance in the table's color at elevDeg (below the horizon: the horizon's); the GLSL twin
//! is atmo_sky.frag atWeatherTint
inline void weatherTint( const float phys[3], const float table[kTintN][3], float elevDeg, float out[3] )
{
	const float t = std::sqrt( std::clamp( elevDeg, 0.0f, 90.0f ) / 90.0f ) * float( kTintN - 1 );
	const int i = std::min( int( t ), kTintN - 2 );
	const float f = t - float( i );
	float w[3];
	for ( int c = 0; c < 3; c++ )
		w[c] = table[i][c] * ( 1.0f - f ) + table[i + 1][c] * f;
	const float lw = lum( w );
	const float s = lw > 1e-8f ? lum( phys ) / lw : 0.0f;
	for ( int c = 0; c < 3; c++ )
		out[c] = lw > 1e-8f ? w[c] * s : phys[c];
}

}	// namespace WwAtmoMath

#endif
