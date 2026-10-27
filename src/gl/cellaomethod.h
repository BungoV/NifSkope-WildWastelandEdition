#ifndef CELLAOMETHOD_H
#define CELLAOMETHOD_H

#include <cstring>

/*! lane SSAO2: the cell view's ambient occlusion methods (src/gl/celllights.cpp, res/shaders/cell_aox.frag) and the
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

/*! Whether the frame's shared opaque pass (g-buffer + depth pyramid) runs. The AO row drops only the AO term:
 *  SSR, the AO decal, contact shadows and the screen GI keep the pass. redOld (WW_CELL_AO_SSR_RED=old) puts back
 *  the coupling cellssr.h used to describe: with AO off the reflections get no pass. */
inline bool wwAoSharedPassRuns( bool aoOn, bool forSsr, bool forOthers, bool redOld )
{
	return aoOn || forOthers || ( forSsr && !redOld );
}

#endif // CELLAOMETHOD_H
