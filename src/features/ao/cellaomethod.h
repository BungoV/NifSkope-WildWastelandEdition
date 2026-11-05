#ifndef CELLAOMETHOD_H
#define CELLAOMETHOD_H

#include <cstring>

/*! lane SSAO2: the cell view's ambient occlusion methods (src/gl/celllights.cpp, res/shaders/features/ao/cell_aox.frag) and the
 *  shared opaque pass's keep rule. Pure (no Qt, no GL) so tests/aomethod checks them alone. */

enum WwAoMethod
{
	WwAoVanilla = 0,	// lane AO1's port of the game's own (half resolution, cell_ao.frag)
	WwAoGtao = 1,		// FO4 Community Shaders' GTAO
	WwAoXeGtao = 2,		// FO4 Community Shaders' XeGTAO (Intel, MIT)
	WwAoHbao = 3,		// FO4 Community Shaders' HBAO+
	WwAoMethodCount = 4
};

//! WW_CELL_AO_METHOD / WW/CellAoMethod value -> method; anything else (empty, a typo) -> -1
inline int wwAoMethodParse( const char * v )
{
	if ( !v )
		return -1;
	static const char * const names[WwAoMethodCount] = { "vanilla", "gtao", "xegtao", "hbao" };
	for ( int i = 0; i < WwAoMethodCount; i++ )
		if ( std::strcmp( v, names[i] ) == 0 )
			return i;
	return -1;
}

inline const char * wwAoMethodName( int m )
{
	static const char * const names[WwAoMethodCount] = { "vanilla", "gtao", "xegtao", "hbao" };
	return m >= 0 && m < WwAoMethodCount ? names[m] : "vanilla";
}

/*! The cell_aox.frag stages a method runs after the shared depth mips, in order (0 ends); the last one writes the
 *  target the cell programs read. FO4CS: GTAO/HBAO+ then two denoise passes; XeGTAO its own denoise then one. */
inline const int * wwAoMethodStages( int m )
{
	static const int gtao[] = { 3, 5, 5, 0 }, xe[] = { 6, 7, 5, 0 }, hbao[] = { 4, 5, 5, 0 }, none[] = { 0 };
	return m == WwAoGtao ? gtao : m == WwAoXeGtao ? xe : m == WwAoHbao ? hbao : none;
}

/*! The AO quality tier (WW_CELL_AO_QUALITY / WW/CellAoQuality; the built-in presets pick it
 *  by name): 0 Low, 1 Medium, 2 High, 3 Ultra (the default). The method stays the user's (presets leave methods
 *  alone); the tier scales its sample count. Vanilla (half resolution, the game's SAO): the still view's angles
 *  1 / 3 / 6 / 8 (8 = AO1's, the default picture; 1 = the game's own single frame). The ported methods (full
 *  resolution): slices x steps 2x2 / 2x3 / 3x3 / 4x3 (FO4CS's 9x3 is the red `old`, every tier). Offline cost
 *  (notes/perffix2/c/RESULTS.md, 1080p 5070 Ti): vanilla 0.08 / 0.14 / 0.24 / 0.30 ms. redOld: every tier as before. */
enum { WwAoTierLow = 0, WwAoTierMedium = 1, WwAoTierHigh = 2, WwAoTierUltra = 3, WwAoTierCount = 4 };

//! WW_CELL_AO_QUALITY value -> tier: low|medium|high|ultra or 0..3; anything else -> -1
inline int wwAoTierParse( const char * v )
{
	if ( !v || !*v )
		return -1;
	static const char * const names[WwAoTierCount] = { "low", "medium", "high", "ultra" };
	for ( int i = 0; i < WwAoTierCount; i++ )
		if ( std::strcmp( v, names[i] ) == 0 )
			return i;
	if ( v[0] >= '0' && v[0] <= '3' && v[1] == 0 )
		return v[0] - '0';
	return -1;
}

//! vanilla: the still view's angle count at a tier
inline int wwAoTierAngles( int tier, bool redOld )
{
	static const int angles[WwAoTierCount] = { 1, 3, 6, 8 };
	return redOld || tier < 0 || tier >= WwAoTierCount ? 8 : angles[tier];
}

//! a ported method's slices and steps per slice at a tier
inline void wwAoTierCounts( int tier, bool redOld, int & slices, int & steps )
{
	static const int s[WwAoTierCount][2] = { { 2, 2 }, { 2, 3 }, { 3, 3 }, { 4, 3 } };
	if ( redOld || tier < 0 || tier >= WwAoTierCount ) {
		slices = 9;
		steps = 3;
		return;
	}
	slices = s[tier][0];
	steps = s[tier][1];
}

/*! Whether the frame's shared opaque pass (g-buffer + depth pyramid) runs. The AO row drops only the AO term:
 *  SSR, the AO decal, contact shadows and the screen GI keep the pass. redOld (WW_CELL_AO_SSR_RED=old) puts back
 *  the coupling cellssr.h used to describe: with AO off the reflections get no pass. */
inline bool wwAoSharedPassRuns( bool aoOn, bool forSsr, bool forOthers, bool redOld )
{
	return aoOn || forOthers || ( forSsr && !redOld );
}

#endif // CELLAOMETHOD_H
