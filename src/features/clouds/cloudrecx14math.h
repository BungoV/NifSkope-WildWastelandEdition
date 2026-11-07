#ifndef WW_CLOUDRECX14MATH_H
#define WW_CLOUDRECX14MATH_H

/* lane CLOUDRECX1 (bungo 11-07, SPEC.md 1.4 "clouds"): the weather .recx clouds block's physical keys, no GL and no Qt,
 * shared by src/features/weather/weatherrecx.cpp (the reader), src/features/clouds/cloudmap.cpp (the bands),
 * src/features/clouds/cloudvol.cpp (the march's uniforms) and the offline gate tests/features/clouds/cloudrecx14.
 * The shader twins: res/shaders/features/clouds/ww_droplet_phase.glsl wwIcePhase, cloud_march_step.glsl the shafts.
 * Sources (notes/cloudrecx1/RESEARCH.md has the table with every citation):
 * - extinction of a cloud of droplets much larger than the wavelength (extinction efficiency ~2):
 *   sigma = 3 LWC / (2 rho_w r_e) (Stephens 1978 J. Atmos. Sci. 35; Hansen & Travis 1974); LWC g/m^3, r_e um ->
 *   sigma /m = 1.5 LWC / r_e, /km = 1500 LWC / r_e. Ice: rho_i 0.917 g/cm^3 in place of rho_w (Fu 1996).
 * - the drawn extinction keeps today's picture for the type's default: the march's density is multiplied by
 *   sigma(file) / sigma(type default) (1 exactly when the file sets neither key). */

#include <algorithm>
#include <cmath>

namespace WwCloudRecx14
{

constexpr float kPi = 3.14159265f;
constexpr float kLwcMax = 5.0f;	// clouds.liquidWater, g/m^3 (the wettest cumulonimbus cores ~4)
constexpr float kRadiusMin = 2.0f, kRadiusMax = 100.0f;	// clouds.dropletRadius, um (cirrus ice r_e reaches ~100)
constexpr float kEvolutionMin = 1.0f, kEvolutionMax = 1440.0f;	// clouds.evolution, minutes
constexpr float kAnvilMax = 2.0f;	// clouds.layers.<n>.anvil: 1 = a cumulonimbus' spread, 0 = none, 2 = a full deck
constexpr float kPrecipBaseMax = 20000.0f;	// clouds.precipitationBase, metres above sea level
constexpr float kRhoIce = 0.917f;	// ice density / water density

//! sigma /km of water content w (g/m^3) in drops / crystals of effective radius r (um), ice share f
inline float sigmaPerKm( float w, float rUm, float f = 0.0f )
{
	if ( w <= 0.0f || rUm <= 0.0f )
		return 0.0f;
	f = std::clamp( f, 0.0f, 1.0f );
	return 1500.0f * w / rUm * ( ( 1.0f - f ) + f / kRhoIce );
}

struct Micro
{
	float lwc = 0.0f, rUm = 0.0f;	// g/m^3, um
};

/* the type defaults, Low band (type 0 stratus, 1/3 stratocumulus, 2/3 cumulus, 1 cumulonimbus; between anchors the
 * two blend). RESEARCH.md table T: stratus 0.28 g/m^3 / 7.33 um (OPAC "stratus continental", Hess et al. 1998),
 * stratocumulus 0.45 / 8 (LWC: Thompson 2007; r ours, between OPAC's stratus and cumulus), cumulus 0.6 / 9 (bungo's
 * ruling 0.5-1; OPAC's cumulus models 0.26-0.44 at 5.8-12.7 um), cumulonimbus 1.5 / 14 (Thompson 2007 1-3 g/m^3;
 * 14 um = the raining-cloud top r_e of Rosenfeld & Gutman 1994) */
constexpr Micro kLowAnchor[4] = { { 0.28f, 7.33f }, { 0.45f, 8.0f }, { 0.60f, 9.0f }, { 1.50f, 14.0f } };
constexpr Micro kMidDefault = { 0.20f, 8.0f };	// altocumulus / altostratus: ours (no primary table found, RESEARCH.md T2)
constexpr Micro kHighDefault = { 0.026f, 91.7f };	// cirrus: OPAC CIR1 (-25 C) ice water content and r_eff

inline Micro lowDefault( float type )
{
	const float u = std::clamp( type, 0.0f, 1.0f ) * 3.0f;
	const int k = std::min( int( u ), 2 );
	const float t = u - float( k );
	return { kLowAnchor[k].lwc + ( kLowAnchor[k + 1].lwc - kLowAnchor[k].lwc ) * t,
		kLowAnchor[k].rUm + ( kLowAnchor[k + 1].rUm - kLowAnchor[k].rUm ) * t };
}

//! the band's default (0 Low at the type, 1 Mid, 2 High)
inline Micro bandDefault( int band, float type )
{
	return band == 0 ? lowDefault( type ) : band == 1 ? kMidDefault : kHighDefault;
}

/*! the drawn density's factor for a file's liquidWater / dropletRadius (< 0 = not set: the default's) against the
 *  default d: sigma(file) / sigma(default). 1 exactly when neither is set (today's picture). */
inline float densityRatio( float lwc, float rUm, const Micro & d )
{
	if ( lwc < 0.0f && rUm < 0.0f )
		return 1.0f;
	const float w = lwc >= 0.0f ? lwc : d.lwc, r = rUm >= 0.0f ? rUm : d.rUm;
	const float s0 = sigmaPerKm( d.lwc, d.rUm );
	return s0 > 0.0f ? sigmaPerKm( w, r ) / s0 : 1.0f;
}

/* the per-layer keys onto the three bands (cloudmap.cpp): each drawn layer's band, its cover (the weight the band's
 * density already takes, cloudmath.h bandsOf) and its keys (< 0 = not set). A band's ratio is the cover-weighted mean
 * of its layers' sigma ratios against the band's default (Low at the layer's type, else the weather's); its ice the
 * weighted mean of the layers' iceFraction, else the weather's; the anvil the largest over the Low band's layers. A band
 * none of whose layers sets a key keeps ratio 1 and ice 0 exactly (today). */
struct LayerIn
{
	int band = 0;
	float cover = 0.0f;
	float lwc = -1.0f, rUm = -1.0f, ice = -1.0f, anvil = -1.0f, type = -1.0f;
};
struct BandsOut
{
	float ratio[3] = { 1.0f, 1.0f, 1.0f };
	float ice[3] = { 0.0f, 0.0f, 0.0f };
	float anvil = -1.0f;
};

inline BandsOut layersOntoBands( const LayerIn * L, int n, float weatherIce, float lowType )
{
	BandsOut o;
	for ( int b = 0; b < 3; b++ ) {
		double w = 0.0, wr = 0.0, wi = 0.0;
		bool anyR = false, anyI = false;
		for ( int i = 0; i < n; i++ ) {
			if ( L[i].band != b || !( L[i].cover > 0.0f ) )
				continue;
			const double c = L[i].cover;
			const bool hasR = L[i].lwc >= 0.0f || L[i].rUm >= 0.0f;
			anyR = anyR || hasR;
			anyI = anyI || L[i].ice >= 0.0f;
			w += c;
			wr += c * ( hasR ? densityRatio( L[i].lwc, L[i].rUm, bandDefault( b, L[i].type >= 0.0f ? L[i].type : lowType ) ) : 1.0 );
			wi += c * ( L[i].ice >= 0.0f ? L[i].ice : std::max( weatherIce, 0.0f ) );
			if ( b == 0 && L[i].anvil >= 0.0f )
				o.anvil = std::max( o.anvil, L[i].anvil );
		}
		if ( anyR && w > 0.0 )
			o.ratio[b] = float( wr / w );
		if ( anyI && w > 0.0 )
			o.ice[b] = float( wi / w );
		else if ( weatherIce >= 0.0f )
			o.ice[b] = weatherIce;
	}
	return o;
}

/* the droplet phase's diameter (cloudvolmath.h dropletParams, Jendersie & d'Eon 2023: 5..50 um): today's 20 um unless
 * the file sets dropletRadius (the type's default radius does NOT move the phase: today's picture) */
inline float phaseDiameterUm( float rUm, float todayUm )
{
	return rUm >= 0.0f ? std::clamp( 2.0f * rUm, 5.0f, 50.0f ) : todayUm;
}

/* the ice phase (iceFraction blends it in for the droplet phase): a smooth crystal's forward diffraction peak, the
 * roughened body (HG g 0.75: Yang et al. 2013, Baum et al. 2005) and the 22 deg halo (Greenler 1980; Tape 1994),
 * a Gaussian ring 1 deg wide. k scales the lobes like the droplet octaves (wwDropletPhase's k); the halo's share
 * fades to isotropic with k so the sum stays normalised. Twin: ww_droplet_phase.glsl wwIcePhase. */
constexpr float kIceG = 0.75f, kIceFwdG = 0.98f, kIceFwdW = 0.10f, kIceHaloW = 0.03f;
constexpr float kHaloRad = 0.383972f, kHaloWidthRad = 0.0174533f;	// 22 deg, 1 deg
//! the ring's normalisation: 2 pi sin(22 deg) w sqrt(pi) (a narrow Gaussian in angle on the sphere)
constexpr float kHaloNorm = 2.0f * kPi * 0.374607f * kHaloWidthRad * 1.7724539f;

inline float hgPhase( float g, float c )
{
	return ( 1.0f - g * g ) / ( 4.0f * kPi * std::pow( std::max( 1e-4f, 1.0f + g * g - 2.0f * g * c ), 1.5f ) );
}

inline float icePhase( float c, float k )
{
	const float a = std::acos( std::clamp( c, -1.0f, 1.0f ) ) - kHaloRad;
	const float ring = std::exp( -( a * a ) / ( kHaloWidthRad * kHaloWidthRad ) ) / kHaloNorm;
	return ( 1.0f - kIceFwdW - kIceHaloW ) * hgPhase( kIceG * k, c ) + kIceFwdW * hgPhase( kIceFwdG * k, c )
		+ kIceHaloW * ( k * ring + ( 1.0f - k ) / ( 4.0f * kPi ) );
}

/* clouds.evolution: the shapes renew in this many minutes, apart from the wind's drift: the noise is read
 * kEvolveKm higher per evolution period (the height profile stays: the cloud stays where it is and changes shape;
 * Schneider & Vos 2015 animate the noise the same way). 0 = not set: no change (today). Wrapped at 24 km (the shape
 * 6 km, detail 0.6 km and curl 24 km periods all divide it: no seam). */
constexpr float kEvolveKm = 1.0f, kEvolveWrapKm = 24.0f;
inline float evolveKm( double secs, float minutes )
{
	if ( !( minutes > 0.0f ) )
		return 0.0f;
	return float( std::fmod( secs / ( 60.0 * double( minutes ) ) * double( kEvolveKm ), double( kEvolveWrapKm ) ) );
}

/* clouds.precipitationBase: shafts under the Low band from the precipitation block's rates. Extinction of rain
 * /km = 0.21 R^0.74 (R mm/h), of snow /km = 1.3 R^0.7 (R mm/h water equivalent): RESEARCH.md table P (Atlas 1953;
 * Rasmussen et al. 1999's snow visibility, both through Koschmieder 3.912 / V). 0 = no shafts. */
inline float shaftSigmaPerKm( float rainMmH, float snowMmH )
{
	float s = 0.0f;
	if ( rainMmH > 0.0f )
		s += 0.21f * std::pow( rainMmH, 0.74f );
	if ( snowMmH > 0.0f )
		s += 1.3f * std::pow( snowMmH, 0.7f );
	return s;
}

/* the shaft's fall-off down the column: hb 0 at the cloud base, 1 at the precipitationBase. Above the ground the
 * shaft evaporates (virga): it thins over its last 40 %; a base at or below the ground keeps it full to the ground.
 * Twin: cloud_march_step.glsl. */
inline float shaftProfile( float hb, bool virga )
{
	if ( hb <= 0.0f || hb >= 1.0f )
		return 0.0f;
	if ( !virga )
		return 1.0f;
	const float t = std::clamp( ( hb - 0.6f ) / 0.4f, 0.0f, 1.0f );
	return 1.0f - t * t * ( 3.0f - 2.0f * t );
}

//! the shaft's cell at a point: the Low band's coverage c (calibrated) against the shape read s, as the band's base
inline float shaftCell( float s, float c )
{
	if ( c <= 0.0f )
		return 0.0f;
	return std::clamp( ( s - ( 1.0f - c ) ) / std::max( c, 1e-3f ), 0.0f, 1.0f );
}

}	// namespace WwCloudRecx14

#endif
