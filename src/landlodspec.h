#ifndef LANDLODSPEC_H
#define LANDLODSPEC_H

/* lane LANDSPEC1: the game's LOD land specular and its land-LOD blend (no Qt; tests/landspec). The far ground takes
 * fLODLandSpecPower / fLODLandSpecScale as its gloss / spec mask; the near ground lerps its own toward them by the
 * blend factor of the distance (xy) to the centre of the camera's cell. fo4_default.frag (landLod) runs the same law. */

#include <algorithm>
#include <cmath>

namespace LandLodSpec
{
constexpr float kPower = 0.22f;	// fLODLandSpecPower:TerrainManager (the far ground's gloss)
constexpr float kScale = 0.06f;	// fLODLandSpecScale:TerrainManager (its spec mask)
constexpr float kBlendEnd = 9625.6f;	// the blend is whole from 2.35 cells off the centre
constexpr float kBlendInv = 0.000375600968f;	// 1 / 2662.4: it starts 0.65 cells nearer (1.7 cells)
constexpr float kCell = 4096.0f;

enum Mode { Off = 0, Near = 1, Far = 2 };

inline float blend( float d )
{
	return 1.0f - std::clamp( ( kBlendEnd - d ) * kBlendInv, 0.0f, 1.0f );
}

//! the middle of the cell holding p, in a cell scene whose origin is a cell's middle
inline float cellCentre( float p )
{
	return std::floor( ( p + 0.5f * kCell ) / kCell ) * kCell;
}

struct GlossSpec
{
	float gloss = 1.0f, spec = 1.0f;
};

inline float mixf( float a, float b, float t )
{
	return a * ( 1.0f - t ) + b * t;
}

//! a ground point's gloss / spec mask (the material's x its _s map) as the game draws it
inline GlossSpec land( GlossSpec own, Mode m, float dx, float dy, float scale = kScale )
{
	const float v = m == Far ? 1.0f : ( m == Near ? blend( std::sqrt( dx * dx + dy * dy ) ) : 0.0f );
	return { mixf( own.gloss, kPower, v ), mixf( own.spec, scale, v ) };
}

/* Wetness: the land pre-pass law and the game's wetness clock. Nothing calls these yet: the viewer has no
 * precipitation (LEFTOVER in notes/landspec1). Defaults = fDefaultLandscape_WetnessControl_*. */
struct Wet
{
	float specScale = 0.5f, specPowerScale = 2.75f, specMin = 0.3f, metalness = 0.35f, envMapScale = 1.5f;
};

inline GlossSpec wetLand( GlossSpec gs, float wet, const Wet & w = Wet() )
{
	const float m = std::clamp( w.specMin, 0.0f, 1.0f );
	const float spec = gs.spec * ( 1.0f - wet * m ) + wet * m;
	return { mixf( 1.0f, w.specPowerScale, wet ) * gs.gloss, mixf( 1.0f, w.specScale, wet ) * spec };
}

inline float wetAlbedoK( float wet, const Wet & w = Wet() )
{
	return std::clamp( 1.0f - wet * w.metalness, 0.0f, 1.0f );
}

//! one step: rain (and ripples on) wets at fWetSpeed 0.03 a second, else dries at fDrySpeed 0.02
inline float wetStep( float wet, bool raining, float dt )
{
	return raining ? std::min( wet + 0.03f * dt, 1.0f ) : std::max( wet - 0.02f * dt, 0.0f );
}
} // namespace LandLodSpec

#endif
