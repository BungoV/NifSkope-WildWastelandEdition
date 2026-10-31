#ifndef WW_CLOUDLIGHTNING_H
#define WW_CLOUDLIGHTNING_H

/* lane CLOUD6f (CLOUDBAND1 leftover, RDR2 gap f): lightning lit inside the volume clouds for storm weathers. The CPU
 * side, no GL and no Qt (tests/cloudlightning builds it with g++); res/shaders/cloud_composite.frag holds the GLSL twin
 * of fieldAt(). Sources and what is ours:
 * - RDR2 (Bauer 2019, SIGGRAPH Advances, slide 40 + note 40, notes/fogheight1/rdr2/deck.txt lines 485-496): each strike
 *   is a point light inside the cloud, "exponential falloff + HG phase function", "simplified local extinction", added
 *   to the scattering term. RDR2 loops the lights per march sample; here the march is not touched (a lit sample already
 *   reads 10-16 densities, CLOUD6c): the flash is added where the volume is composited, at the cloud's
 *   transmittance-weighted mean depth (the history's depth target), full screen resolution, every frame.
 * - The field: the steady diffusion Green's function of a point source in a scattering medium (exp(-r / delta) / r,
 *   delta = 1 / sqrt(3 sigma_a sigma_t'), D = 1 / (3 sigma_t')) with image sources across the Low band's extrapolated
 *   boundaries (z_b = 2 D; Farrell, Patterson & Wilson 1992 dipole), plus the first-order radiance term toward the eye
 *   (L = (phi + 3 g F.s) / 4 pi, F = -D grad phi). Gated against a Monte Carlo (tests/spells/cloudlightning_check.py).
 * - Which weathers storm: a WTHR SNAM sound of type 3 (thunder). The DATA thunder byte is 133..255 in every vanilla
 *   weather (CLOUD6d, MEASURED) and tells nothing apart; the thunder sound is in 15 of 107 (census_storm.py, MEASURED).
 * - The timing (ours, not game data: the game's frequency law is not known here): a deterministic clock (the clouds'
 *   pinned clock, so rows repeat): slots of kSlotS seconds, a strike in a slot with chance kStrikeP, 1..4 return strokes
 *   kStrokeTauS long each; a flash ends inside its slot, and between flashes the level is exactly 0. */

#include "cloudvolmath.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace WwCloudLightning
{

constexpr double kSlotS = 8.0;	// one strike chance per slot
constexpr float kStrikeP = 0.75f;	// a strike in a slot (mean interval kSlotS / kStrikeP = 10.7 s)
constexpr double kLeadS = 0.5;	// a strike starts kLeadS .. kSlotS - 1.5 s into its slot
constexpr int kMaxStrokes = 4;
constexpr double kStrokeTauS = 0.04;	// each return stroke's decay
constexpr double kStrokeLenS = 0.3;	// a stroke's light is cut to exactly 0 this long after it starts
constexpr float kRingMinKm = 2.0f, kRingKm = 10.0f;	// strike distance from the camera: kRingMinKm .. + kRingKm
constexpr float kSourceHf = 0.35f;	// the strike's height in the Low band (0 = base, 1 = top)
constexpr float kFlashUnder = 0.5f;	// look: the base right under a strike peaks at this x a white sunlit surface (sunK / pi)
constexpr float kRb = 6360.0f;	// km, the planet's radius (atmo_common.glsl kRb)

//! the switch WW_CLOUD_LIGHTNING: unset / 1 / on = on (the default); 0 / off / false / no = off (cloudvolmath.h)
inline bool switchOn( const char * v )
{
	return WwCloudVolMath::defaultOnSwitch( v );
}

inline std::uint32_t hash( std::uint32_t x )
{
	// a 32-bit integer mix (Wellons' lowbias32): the same draws on every machine
	x ^= x >> 16;
	x *= 0x7feb352dU;
	x ^= x >> 15;
	x *= 0x846ca68bU;
	x ^= x >> 16;
	return x;
}

//! the i-th uniform draw [0, 1) of slot k under seed
inline float draw( std::int64_t k, std::uint32_t seed, std::uint32_t i )
{
	const std::uint32_t lo = std::uint32_t( std::uint64_t( k ) & 0xffffffffu ), hi = std::uint32_t( std::uint64_t( k ) >> 32 );
	const std::uint32_t h = hash( hash( hash( lo ^ 0x9e3779b9u ) ^ hi ) ^ hash( seed + 0x85ebca6bu * ( i + 1u ) ) );
	return float( h >> 8 ) * ( 1.0f / 16777216.0f );
}

//! one slot's strike (none when !on)
struct Strike
{
	bool on = false;
	double startS = 0.0;	// seconds on the clock
	int strokes = 0;
	double strokeS[kMaxStrokes] = {};
	float amp[kMaxStrokes] = {};
	float offKm[2] = {};	// from the camera, world x / y
};

/* lane LIGHTNING2 (LIGHTNINGFREQ1): the schedule as data. The old one (CLOUD6f, ours: 8 s slots, a strike in 75 % of
 * them, the storm share on the brightness) is oldSchedule; the DATA law is freqSchedule:
 * - MEASURED (notes/fix1031a/lightning2/census, the 107 vanilla WTHR): DATA byte 10 "Thunder/Lightning - Frequency"
 *   (the FO4Edit field name) reads 133 in the 6 radstorms, 137 in the 2 Glowing Sea fogs, 250 in the 7 rains of the 15
 *   thunder-SNAM weathers; the quiet ones 137 (65, the editor's default) or 255 (25). Bytes 8 / 9 "- Begin Fade In" /
 *   "- End Fade Out": rain 128 / 255, radstorm 216 / 231, Glowing Sea fog 92 / 216.
 * - SOURCED (UESP, Oblivion's GetWeatherLightningFrequency): the byte runs 0..255, 255 the lowest frequency, 0 the
 *   highest. The GECK wiki: Begin Fade In = the point of a change INTO the weather after which thunder/lightning is
 *   seen; End Fade Out = the point of a change OUT of it after which it is no longer seen.
 * - REASONED (ours, no game law read): the strike rate is linear in (255 - byte) / 255 = a strike in a kFreqSlotS slot
 *   with that chance (byte 0 = one every 2 s, 133 = every 4.2 s, 250 = every 102 s, 255 = never); the fade share (the
 *   bytes 8 / 9 ramps, stormShare) scales the CHANCE of a strike, each strike at full brightness ("you will begin
 *   seeing" = how many flashes, not how bright); strokes, their decay and the brightness stay CLOUD6f's. */
struct Schedule
{
	double slotS = kSlotS;	// one strike chance per slot
	float chance = kStrikeP;	// a strike in a slot
	double leadS = kLeadS, spanS = kSlotS - 1.5 - kLeadS;	// a strike starts leadS .. leadS + spanS into its slot
	float gain = 1.0f;	// the brightness factor (the old schedule: the storm share)
};

constexpr double kFreqSlotS = 2.0, kFreqLeadS = 0.2, kFreqSpanS = 0.8;	// a flash (<= 0.66 s) ends inside its slot
constexpr double kSearchS = 1200.0;	// nextStrikeS looks this far ahead (the rarest vanilla storm: one per 102 s)

//! CLOUD6f's own schedule (the red WW_LIGHTNING_FREQ_RED): the share dims each flash
inline Schedule oldSchedule( float share )
{
	Schedule s;
	s.gain = share;
	return s;
}

//! the strike chance per kFreqSlotS slot of a DATA frequency byte (255 - byte) / 255
inline float freqChance( unsigned byte )
{
	return float( 255u - std::min( byte, 255u ) ) / 255.0f;
}

//! the DATA law: the share x freqChance(byte) per 2 s slot, every strike at full brightness
inline Schedule freqSchedule( float share, unsigned byte )
{
	Schedule s;
	s.slotS = kFreqSlotS;
	s.chance = std::clamp( share, 0.0f, 1.0f ) * freqChance( byte );
	s.leadS = kFreqLeadS;
	s.spanS = kFreqSpanS;
	s.gain = share > 0.0f ? 1.0f : 0.0f;
	return s;
}

//! the mean time between strikes of a schedule (seconds; -1 = never)
inline double meanIntervalS( const Schedule & s )
{
	return s.chance > 0.0f ? s.slotS / double( s.chance ) : -1.0;
}

inline Strike strikeOf( std::int64_t k, std::uint32_t seed, const Schedule & sc )
{
	Strike s;
	if ( draw( k, seed, 0 ) >= sc.chance )
		return s;
	s.on = true;
	s.startS = double( k ) * sc.slotS + sc.leadS + double( draw( k, seed, 1 ) ) * sc.spanS;
	s.strokes = 1 + std::min( kMaxStrokes - 1, int( draw( k, seed, 2 ) * float( kMaxStrokes ) ) );
	double t = s.startS;
	for ( int j = 0; j < s.strokes; j++ ) {
		s.strokeS[j] = t;
		s.amp[j] = j == 0 ? 1.0f : 0.35f + 0.5f * draw( k, seed, 3 + j );
		t += 0.05 + 0.07 * double( draw( k, seed, 8 + j ) );
	}
	const float a = 6.2831853f * draw( k, seed, 12 ), rho = kRingMinKm + kRingKm * std::sqrt( draw( k, seed, 13 ) );
	s.offKm[0] = rho * std::cos( a );
	s.offKm[1] = rho * std::sin( a );
	return s;
}

//! CLOUD6f's schedule (oldSchedule; the gain is not part of a strike)
inline Strike strikeOf( std::int64_t k, std::uint32_t seed )
{
	return strikeOf( k, seed, Schedule() );
}

inline std::int64_t slotOf( double secs, double slotS = kSlotS )
{
	return std::int64_t( std::floor( secs / slotS ) );
}

//! the flash level at secs under a schedule (0 between flashes, exactly; the gain not applied); the strike in *out
inline float levelAt( double secs, std::uint32_t seed, const Schedule & sc, Strike * out )
{
	const Strike s = strikeOf( slotOf( secs, sc.slotS ), seed, sc );
	if ( out )
		*out = s;
	if ( !s.on )
		return 0.0f;
	float l = 0.0f;
	for ( int j = 0; j < s.strokes; j++ ) {
		const double dt = secs - s.strokeS[j];
		if ( dt >= 0.0 && dt < kStrokeLenS )
			l += s.amp[j] * float( std::exp( -dt / kStrokeTauS ) );
	}
	return l;
}

//! CLOUD6f's: the old schedule's level
inline float levelAt( double secs, std::uint32_t seed, Strike * out = nullptr )
{
	return levelAt( secs, seed, Schedule(), out );
}

//! the next strike's start at or after secs under a schedule (old: 64 slots, else kSearchS ahead; -1 = none)
inline double nextStrikeS( double secs, std::uint32_t seed, const Schedule & sc )
{
	const std::int64_t n = sc.slotS == kSlotS ? 64 : std::int64_t( kSearchS / sc.slotS );
	for ( std::int64_t k = slotOf( secs, sc.slotS ), e = k + n; k < e; k++ ) {
		const Strike s = strikeOf( k, seed, sc );
		if ( s.on && s.startS + kStrokeLenS + 0.4 >= secs )
			return s.startS;
	}
	return -1.0;
}

inline double nextStrikeS( double secs, std::uint32_t seed )
{
	return nextStrikeS( secs, seed, Schedule() );
}

/* the storm share during a weather change (REASONED from the WTHR field names "Thunder/Lightning - Begin Fade In" and
 * "- End Fade Out", DATA bytes 8 and 9, /255 of the change): the new weather's lightning ramps in from pct = fadeIn to
 * 1, the old weather's ramps out from pct 0 to fadeOut. No change: the current weather's storm flag. */
inline float stormShare( bool stormNow, float fadeInNow, bool changing, bool stormLast, float fadeOutLast, float pct )
{
	if ( !changing )
		return stormNow ? 1.0f : 0.0f;
	float w = 0.0f;
	if ( stormNow )
		w = std::max( w, fadeInNow >= 1.0f ? ( pct >= 1.0f ? 1.0f : 0.0f )
							: std::clamp( ( pct - fadeInNow ) / ( 1.0f - fadeInNow ), 0.0f, 1.0f ) );
	if ( stormLast )
		w = std::max( w, fadeOutLast <= 0.0f ? 0.0f : std::clamp( 1.0f - pct / fadeOutLast, 0.0f, 1.0f ) );
	return w;
}

//! the field's constants for a uniform medium of extinction sigma (/km), albedo w, mean cosine g
struct Field
{
	float src[3] = {};	// km: world x, y, height
	float invDelta = 0.0f;	// 1 / km
	float r0sq = 0.0f;	// km^2: the source softened to one transport mean free path
	float mirLo = 0.0f, mirHi = 0.0f;	// the image planes' doubled heights: 2 (base - z_b), 2 (top + z_b)
	float flux = 0.0f;	// 3 g D, km (0 = the isotropic candidate)
	float norm = 0.0f;	// radiance per unit source power per unit field: w / (4 pi) / (4 pi D)
};

inline Field fieldOf( float sigma, float w, float g, float baseKm, float topKm, const float src[3], bool fluxOn = true )
{
	Field f;
	const float st = sigma * ( 1.0f - w * g ), sa = sigma * ( 1.0f - w );
	const float D = 1.0f / ( 3.0f * st ), zb = 2.0f * D;
	f.src[0] = src[0];
	f.src[1] = src[1];
	f.src[2] = src[2];
	f.invDelta = std::sqrt( 3.0f * sa * st );
	f.r0sq = 1.0f / ( st * st );
	f.mirLo = 2.0f * ( baseKm - zb );
	f.mirHi = 2.0f * ( topKm + zb );
	f.flux = fluxOn ? 3.0f * g * D : 0.0f;
	const float pi = WwCloudVolMath::kPi;
	f.norm = w / ( 4.0f * pi ) / ( 4.0f * pi * D );
	return f;
}

//! G = e^(-r / delta) / r of one source at a (p - source) and its gradient; r softened by r0
inline float green( const Field & f, const float a[3], float grad[3] )
{
	const float r = std::sqrt( a[0] * a[0] + a[1] * a[1] + a[2] * a[2] + f.r0sq );
	const float e = std::exp( -r * f.invDelta ) / r;
	const float k = -e * ( f.invDelta + 1.0f / r ) / r;
	for ( int i = 0; i < 3; i++ )
		grad[i] = k * a[i];
	return e;
}

/* the radiance toward the eye at p (world km x, y, height) per unit source power, seen along the view ray d (from the
 * eye): norm x max(phi + 3 g D grad(phi).d, 0), phi = G(source) - G(image below the base) - G(image above the top).
 * Twin: cloud_composite.frag cvLtField. */
inline float fieldAt( const Field & f, const float p[3], const float d[3] )
{
	const float zs[3] = { f.src[2], f.mirLo - f.src[2], f.mirHi - f.src[2] };
	float phi = 0.0f, gd = 0.0f;
	for ( int i = 0; i < 3; i++ ) {
		const float a[3] = { p[0] - f.src[0], p[1] - f.src[1], p[2] - zs[i] };
		float gr[3];
		const float s = i == 0 ? 1.0f : -1.0f;
		phi += s * green( f, a, gr );
		gd += s * ( gr[0] * d[0] + gr[1] * d[1] + gr[2] * d[2] );
	}
	return f.norm * std::max( phi + f.flux * gd, 0.0f );
}

/* the source power for one flash of level 1: the base right under the strike (one extinction length inside, seen
 * straight up) at kFlashUnder x a white surface under the light sunK (radiance sunK / pi) */
inline float powerFor( const Field & f, float baseKm, float sigma, float sunK )
{
	const float p[3] = { f.src[0], f.src[1], baseKm + 1.0f / sigma };
	const float d[3] = { 0.0f, 0.0f, 1.0f };
	const float under = fieldAt( f, p, d );
	return under > 0.0f ? kFlashUnder * sunK / WwCloudVolMath::kPi / under : 0.0f;
}

/* lane LIGHTNING2 (LIGHTNINGSKY1): the same flash on the rest of the frame.
 * - The 2D cloud layers (Volume Clouds Off / Low): a layer texel seen along d is lit as the Low band's base at that
 *   direction (the deck one extinction length in, deckKm), the same field and power as the volume's composite.
 * - The terrain, the objects and the air (the exterior cell programs and cell_volair take the weather's DALC): the
 *   lit base above is an emitter of radiance fieldAt x power over the ground; groundDalc integrates it over a grid
 *   around the strike into the six DALC axes (the cell's convention: index = the light's TRAVEL axis, 0 X+, 1 X-,
 *   2 Y+, 3 Y-, 4 Z+, 5 Z-; Z- lights an up-facing normal), relative to the Day sun at normal incidence (sunK), so a
 *   DALC value of 1 = a white surface as bright as under that sun. sunK cancels (powerFor is kFlashUnder x sunK / pi).
 * - REASONED: the flash's light reaches the ground only from the band's base (no light through gaps, no aerial
 *   perspective); one irradiance for the whole frame, at the camera; a camera at or above the deck gets none.
 * - With no cloud map (Volume Clouds Off and Cloud Map Off), the band is kDefBaseKm .. kDefTopKm at density 1 (CLOUD6f's
 *   gate L slab). */
constexpr float kDefBaseKm = 1.5f, kDefTopKm = 3.0f;
constexpr int kGroundN = 48;	// the ground integral's grid per side

//! the deck a 2D layer is lit at: one extinction length above the base (powerFor's point)
inline float deckKm( float baseKm, float sigma )
{
	return baseKm + 1.0f / sigma;
}

inline void groundDalc( const Field & f, float baseKm, float sigma, const float g[3], float out[6] )
{
	for ( int a = 0; a < 6; a++ )
		out[a] = 0.0f;
	const float zb = deckKm( baseKm, sigma );
	const float up[3] = { f.src[0], f.src[1], zb };
	const float dUp[3] = { 0.0f, 0.0f, 1.0f };
	const float under = fieldAt( f, up, dUp );
	if ( !( under > 0.0f ) || g[2] >= zb )
		return;
	const float half = std::clamp( 8.0f / std::max( f.invDelta, 1e-3f ), 1.0f, 6.0f );
	const float step = 2.0f * half / float( kGroundN ), dA = step * step;
	// the six normals a travel axis lights: n = -travel
	static const float nrm[6][3] = { { -1, 0, 0 }, { 1, 0, 0 }, { 0, -1, 0 }, { 0, 1, 0 }, { 0, 0, -1 }, { 0, 0, 1 } };
	double acc[6] = { 0, 0, 0, 0, 0, 0 };
	for ( int j = 0; j < kGroundN; j++ )
		for ( int i = 0; i < kGroundN; i++ ) {
			const float p[3] = { f.src[0] - half + ( float( i ) + 0.5f ) * step, f.src[1] - half + ( float( j ) + 0.5f ) * step, zb };
			const float v[3] = { p[0] - g[0], p[1] - g[1], p[2] - g[2] };
			const float r2 = v[0] * v[0] + v[1] * v[1] + v[2] * v[2], r = std::sqrt( r2 );
			const float w[3] = { v[0] / r, v[1] / r, v[2] / r };
			const float L = fieldAt( f, p, w ) / under;	// seen from g along w, relative to the base under the strike
			const float k = L * ( v[2] / r ) * dA / r2;	// x the emitter's cosine, the solid angle
			for ( int a = 0; a < 6; a++ ) {
				const float c = nrm[a][0] * w[0] + nrm[a][1] * w[1] + nrm[a][2] * w[2];
				if ( c > 0.0f )
					acc[a] += double( k * c );
			}
		}
	for ( int a = 0; a < 6; a++ )
		out[a] = float( acc[a] ) * kFlashUnder / WwCloudVolMath::kPi;
}

} // namespace WwCloudLightning

#endif
