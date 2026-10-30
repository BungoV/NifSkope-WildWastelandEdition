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

#endif // GLASSPANERULES_H
