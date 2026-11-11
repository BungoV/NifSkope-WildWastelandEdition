#pragma once
/* MODULES1 10b-7: the Fusion Shaders cloud shadow law, its constants in ONE place (cloudshadow.cpp hands them to
 * cloud_shadowmap.frag as csFs; nothing else may repeat them).
 * The law (the sheets rung of the Fusion Shaders cloud shadow mask): every weather cloud layer is a flat sheet; only
 * sheets above the receiver shadow it; the light kept is the product over the sheets of (1 - cover), cover = the
 * layer's texture alpha x its layer alpha; the mask is 1 - saturate((1 - kept) x opacity), then mask ^ strength.
 * There is no extinction constant and no volume in that model.
 * NifSkope's Cloud Map already holds each band's cover as the union 1 - prod(1 - cover) of its layers (cloudmath.h),
 * so the product over the bands is the product over the layers.
 * MODULES1 10c: the rest of that model is ported (cloudsheetmath.h, cloud_shadowmap.frag csSheetN > 0): one sheet
 * per LAYER at its share of the deck's altitude, read along the texel's sun line (the parallax), the sun-disc taps,
 * the hex tiling, the density exponent. Red WW_CLOUDSHADOW_RED=bandsheets: 10b's one plane per band from the Cloud
 * Map; notaps, nohex, nodensity, norecx, noscroll: one part each. Red beer: the map mode's old Beer law. */

#include <cmath>
#include <cstring>

namespace WwCloudFusionLaw
{

constexpr float kOpacity = 1.0f;	//!< how dark a full cover gets (1 = black under a closed sheet)
constexpr float kStrength = 1.0f;	//!< the mask's exponent (1 = identity)
constexpr float kLayerDensity = 1.0f;	//!< the exponent on each layer's alpha (1 = identity; applied per sheet since 10c)
constexpr float kDeckUnits = 110000.0f;	//!< the deck's absolute height, world units (the highest layer's sheet)
constexpr float kSheetThinKm = 0.01f;	//!< half the height a flat sheet takes in the map's texel (x = its top, y = od / height)

//! the mask from the light kept (the product of 1 - cover over the sheets above)
inline float mask( float kept, float opacity = kOpacity, float strength = kStrength )
{
	float lost = ( 1.0f - kept ) * opacity;
	lost = lost < 0.0f ? 0.0f : lost > 1.0f ? 1.0f : lost;
	return std::pow( 1.0f - lost, strength );
}

/* MODULES1 10c: the 2D layers' lighting of that model (its cloud scatter pass): a Henyey-Greenstein lobe toward the
 * key light, a rim term on the cover's edge and a body term on the cover, capped; scaled by the light's elevation
 * gate and the weather class's response. lookdev_clouds.frag (ldFs*) draws it as a share of the layer's own lit
 * colour (no light of its own). Red WW_LOOKDEV_RED=cloudnoscatter. */
constexpr float kScatterRim = 1.0f, kScatterBody = 0.6f, kScatterG = 0.6f, kScatterCeiling = 0.5f;
constexpr float kScatterGateLoDeg = -2.0f, kScatterGateHiDeg = 8.0f;	//!< the key light's elevation: none below, full above
constexpr float kScatterCool[3] = { 0.70f, 0.80f, 1.00f }, kScatterWarm[3] = { 1.00f, 0.85f, 0.65f };

struct ScatterRow
{
	float scale, warmth;
};

//! the weather class's response (weathergen.h's class words)
inline ScatterRow scatterRow( const char * cls )
{
	auto is = [cls]( const char * w ) { return cls && std::strcmp( cls, w ) == 0; };
	if ( is( "clear" ) || is( "clearest" ) || is( "dust" ) )
		return { 1.0f, 1.0f };
	if ( is( "overcast" ) )
		return { 1.0f, 0.9f };
	if ( is( "rain" ) || is( "drizzle" ) )
		return { 0.7f, 0.5f };
	if ( is( "storm" ) )
		return { 0.5f, 0.4f };
	if ( is( "fog" ) || is( "mist" ) )
		return { 0.3f, 0.2f };
	if ( is( "snow" ) )
		return { 0.6f, 0.1f };
	if ( is( "radstorm" ) )
		return { 0.0f, 0.5f };
	return { 0.8f, 0.85f };
}

inline float scatterHg( float cosT, float g )
{
	const float d = 1.0f + g * g - 2.0f * g * cosT;
	return ( 1.0f - g * g ) / ( 12.566371f * d * std::sqrt( d ) );
}

inline float scatterGate( float elevDeg )
{
	float t = ( elevDeg - kScatterGateLoDeg ) / ( kScatterGateHiDeg - kScatterGateLoDeg );
	t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
	return t * t * ( 3.0f - 2.0f * t );
}

struct Scatter
{
	float rim = 0.0f, body = 0.0f, gate = 0.0f, scale = 0.0f, warmth = 0.0f;
	float tint[3] = { 1.0f, 1.0f, 1.0f };
};

inline Scatter scatter( float elevDeg, ScatterRow row )
{
	Scatter o;
	o.gate = scatterGate( elevDeg );
	o.scale = row.scale;
	o.warmth = row.warmth;
	const float k = o.gate * row.scale;
	o.rim = kScatterRim * k;
	o.body = kScatterBody * k;
	for ( int c = 0; c < 3; c++ )
		o.tint[c] = kScatterCool[c] + ( kScatterWarm[c] - kScatterCool[c] ) * row.warmth;
	return o;
}

//! the share of the layer's own lit luminance added (the shader's sum, before the tint)
inline float scatterShare( const Scatter & s, float edge, float cover, float cosT )
{
	const float v = ( s.rim * edge + s.body * cover ) * scatterHg( cosT, kScatterG );
	return v < kScatterCeiling ? v : kScatterCeiling;
}

}
