#pragma once
/* MODULES1 10b-7: the Fusion Shaders cloud shadow law, its constants in ONE place (cloudshadow.cpp hands them to
 * cloud_shadowmap.frag as csFs; nothing else may repeat them).
 * The law (the sheets rung of the Fusion Shaders cloud shadow mask): every weather cloud layer is a flat sheet; only
 * sheets above the receiver shadow it; the light kept is the product over the sheets of (1 - cover), cover = the
 * layer's texture alpha x its layer alpha; the mask is 1 - saturate((1 - kept) x opacity), then mask ^ strength.
 * There is no extinction constant and no volume in that model.
 * NifSkope's Cloud Map already holds each band's cover as the union 1 - prod(1 - cover) of its layers (cloudmath.h),
 * so the product over the bands is the product over the layers.
 * Not ported here (cloudshadow.cpp says so in its line): one sheet per LAYER at the deck's altitude (the map has one
 * plane per band, drawn at the band's own height: the shadow stays under the cloud drawn), the sun-disc taps, the
 * hex tiling. Red WW_CLOUDSHADOW_RED=beer: the map mode's old Beer law. */

#include <cmath>

namespace WwCloudFusionLaw
{

constexpr float kOpacity = 1.0f;	//!< how dark a full cover gets (1 = black under a closed sheet)
constexpr float kStrength = 1.0f;	//!< the mask's exponent (1 = identity)
constexpr float kLayerDensity = 1.0f;	//!< the exponent on each layer's alpha (1 = identity; not applied: the map's cover)
constexpr float kDeckUnits = 110000.0f;	//!< the deck's absolute height, world units (the highest layer's sheet)
constexpr float kSheetThinKm = 0.01f;	//!< half the height a flat sheet takes in the map's texel (x = its top, y = od / height)

//! the mask from the light kept (the product of 1 - cover over the sheets above)
inline float mask( float kept, float opacity = kOpacity, float strength = kStrength )
{
	float lost = ( 1.0f - kept ) * opacity;
	lost = lost < 0.0f ? 0.0f : lost > 1.0f ? 1.0f : lost;
	return std::pow( 1.0f - lost, strength );
}

}
