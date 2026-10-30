#ifndef WWFXCOV_H
#define WWFXCOV_H

/* lane TREELEAF3 (FIX1030C): an alpha-tested EFFECT shape's side pass tests the alpha its colour pass tests.
 *
 * Of the side passes that alpha-test a base map, only the TAA object vectors (taainputs.cpp, the movers) ever draw a
 * BSEffectShaderProperty shape: the sun cascades, the light cubes and the reflection probes take wwCastsSunShadow()
 * shapes (`!bsesp`), and the GPU cull's Hi-Z takes only fo4_cell/fo4_default-family programs. The TAA pass tested the
 * raw base alpha (`base.a * alpha < t`, alpha = 1 for an effect shape) with a `>=` test, while the effect's colour pass
 * (fo4_effectshader.frag) tests
 *     color.a = base.a x the vertex alpha x baseColor.a^2       (the palette's alpha at (base.a, the rest) under
 *                                                                greyscaleAlpha; x falloff; x the cell's soft fade)
 * with `>` (a material: alphaFlags 4; a NIF: its function, 4 on every vanilla mover). Census (tests/treeleaf3/
 * fx_census.py, 224734 vanilla NIFs): 214 alpha-tested effect shapes can move (skinned or animated), 166 of them compose
 * an alpha that differs from base.a (bloatfly / bloodbug / stingwing wings: baseColor.a^2 0.56-0.64), all 214 strict.
 *
 * This header is the rule as plain values (no Qt, no GL), so Shape::wwFxSideAlpha (glshape.cpp) and the unit test
 * (tests/treeleaf3) run the same code; taa_objvel.frag is its GLSL mirror (fxMode). Reproduced exactly: baseColor.a^2,
 * the vertex alpha (the attribute the colour pass reads), the palette's alpha and the strict threshold. NOT reproduced
 * (they need the per-pixel normal-map normal and the frame's depth): falloff and the soft fade -- both only lower the
 * colour pass's alpha, so the side pass keeps a superset there (52 of the 166 movers). Red WW_TREELEAF_RED=fx (also
 * "1" and "side"): today's raw base alpha, `>=`. */

#include <cmath>

namespace WwFxCov
{

//! what the effect colour pass multiplies into its tested alpha (renderer.cpp setupProgramCE1, the effect branch)
struct In
{
	bool effect = false;	//!< a BSEffectShaderProperty drawn by fo4_effectshader (FO4, BS 130..139)
	float baseColorA = 1.0f;	//!< Base Color alpha (BGEM: its alpha): glowColor.a, squared by the shader
	bool vertexAlpha = false;	//!< the colour pass reads the vertex alpha (C.a; vertexColorOverride.a 0)
	bool greyscaleAlpha = false;	//!< the alpha comes from the palette at (base.a, the composed rest)
	bool strict = false;	//!< the colour pass keeps a > threshold (a material, or NIF function 4)
	float threshold = 0.0f;	//!< today's side threshold (Shape::wwLeafSideThreshold of wwDrawAlpha)
	float lightingAlpha = 1.0f;	//!< today's `alpha` uniform (a lighting shape's material alpha, else 1)
};

//! the TAA object-vector pass's uniforms: fxMode (0 today's test; 4 + 1 vertex alpha + 2 palette), alpha, threshold
struct Side
{
	int mode = 0;
	float mult = 1.0f;
	float threshold = 0.0f;
};

inline Side side( const In & s, bool red )
{
	Side o;
	o.mult = s.lightingAlpha;
	o.threshold = s.threshold;
	if ( red || !s.effect )
		return o;
	o.mult = s.baseColorA * s.baseColorA;	// the shader's alphaMult = baseColor.a * baseColor.a
	o.mode = 4 | ( s.vertexAlpha ? 1 : 0 ) | ( s.greyscaleAlpha ? 2 : 0 );
	if ( s.strict )
		o.threshold = std::nextafter( s.threshold, 2.0f );	// `discard a < t` keeps exactly `a > threshold`
	return o;
}

/*! taa_objvel.frag's tested alpha (its GLSL, line for line): `pal( u, v )` stands for the palette's .a at (u, v).
 *  base = the base map's .a, vtx = the vertex alpha. */
template <class Pal>
inline float sideAlpha( const Side & o, float base, float vtx, Pal pal )
{
	if ( o.mode == 0 )
		return base * o.mult;
	const float k = ( o.mode & 1 ) ? vtx : 1.0f;
	return ( o.mode & 2 ) ? pal( base, k * o.mult ) : base * k * o.mult;
}

//! the side pass keeps a texel when it does not `discard a < t`
inline bool sideKeeps( float a, float t )
{
	return !( a < t );
}

/*! fo4_effectshader.frag's composed alpha with no falloff and no soft fade: color = baseMap * C, color.a *= alphaMult;
 *  under greyscaleAlpha baseMap.a = 1 first and color.a = palette( texture( BaseMap ).a, color.a ).a */
template <class Pal>
inline float colourAlpha( const In & s, float base, float vtx, Pal pal )
{
	const float c = s.vertexAlpha ? vtx : 1.0f;
	const float alphaMult = s.baseColorA * s.baseColorA;
	if ( s.greyscaleAlpha )
		return pal( base, ( 1.0f * c ) * alphaMult );
	return ( base * c ) * alphaMult;
}

//! the colour pass's test (`m & ( 1 << alphaFlags )`): 4 = `>`, 6 = `>=`
inline bool colourKeeps( float a, float threshold, bool strict )
{
	return strict ? a > threshold : !( a < threshold );
}

}	// namespace WwFxCov

#endif // WWFXCOV_H
