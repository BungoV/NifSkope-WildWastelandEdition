#ifndef GLASSPANERULES_H
#define GLASSPANERULES_H

/* lane GLASSPANE1 (FIX1030B, 2026-10-30): the effect shader's env mask channel, pure C++ (tests/glasspane1 compiles
 * this header alone and judges it on a census of the game's own effect materials and env mask textures).
 *
 * An effect (BGEM) shape's env cube is scaled by its env mask's R channel (the specular mask): every one of the game's
 * effect pixel shaders with the env technique bit reads the mask's R (202 of 202), x the env mask scale x the normal
 * map's alpha (lane FXCHECK1 found it from FO4CS's capture; GLASSPANE1 counted it over the game's shader package).
 * NifSkope read G (gloss: 1.0 on a few glass masks, 0.47 mean against R's 0.39), so too much cube was added and the
 * glass came out milky. FXCHECK1 shipped the R read OFF behind WW_FX_ENVMASK / WW/FxEnvMaskR; it is the default now
 * and the old read is only a red (bungo's rule: no fix-only toggle).
 * Reds (WW_FX_ENVMASK_RED): gloss = the old G read, noenv = no cube at all.
 *
 * GLASSPANE_RED (the unit test's red build) swaps in the base's rule: G unless WW_FX_ENVMASK=1 (default off). */

#include <cstring>

enum class WwFxEnvMask
{
	R = 0,		// the game's: the env mask's R (specular)
	G = 1,		// red gloss: the old read (G, gloss)
	None = 2	// red noenv: no cube
};

//! The effect shader's env mask channel for the value of WW_FX_ENVMASK_RED (null or "" = unset)
inline WwFxEnvMask wwFxEnvMaskRule( const char * red )
{
	const char * r = red ? red : "";
	if ( std::strcmp( r, "noenv" ) == 0 )
		return WwFxEnvMask::None;
	if ( std::strcmp( r, "gloss" ) == 0 )
		return WwFxEnvMask::G;
#ifdef GLASSPANE_RED
	return WwFxEnvMask::G;	// the base's default
#else
	return WwFxEnvMask::R;
#endif
}

/* lane GLASSPANE2 (FXNORMALA1, 10-30): the game's env effect pixel shaders also scale the cube by the NORMAL map's alpha
 * (the env mask's R x env scale x normal.a; every env effect PS samples the normal map's w into the cube's factor).
 * NifSkope left it out. Vanilla effect normals have alpha 1.000 (181 of 181 decoded, GLASSPANE1), so vanilla does not
 * move; a mod normal with alpha < 1 now dims its cube as in game.
 * Red WW_FX_NORMALA_RED=1 (any value but "" / "0"): the old cube without the factor. GLASSPANE2_RED (the unit's red
 * build) = the base's rule (no factor). The shader's uniform is the RED sense, so a program that never sets it keeps
 * the game's rule. */
inline bool wwFxEnvNormalAlphaRule( const char * red )
{
	const char * r = red ? red : "";
	if ( r[0] != '\0' && std::strcmp( r, "0" ) != 0 )
		return false;
#ifdef GLASSPANE2_RED
	return false;	// the base: the cube took no normal alpha
#else
	return true;
#endif
}

//! The env cube's factor from the normal map's alpha a (0..1), by the rule: a when it applies, else 1
inline float wwFxEnvNormalAlphaFactor( bool rule, float a )
{
	return rule ? ( a < 0.0f ? 0.0f : ( a > 1.0f ? 1.0f : a ) ) : 1.0f;
}

#endif // GLASSPANERULES_H
