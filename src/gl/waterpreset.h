/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef WATERPRESET_H
#define WATERPRESET_H

/*! Lane FIX1101A (bungo 10-31 RULED: "we need water that looks good and is not expensive to render"): the water quality
 *  preset, four steps, so the whole water look (near + far, the SSR included) fits a GPU budget per step at 1080p
 *  (Low 0.4, Medium 0.8, High 1.2, Ultra 1.8 ms). WW_CELL_WATER_PRESET = low|medium|high|ultra|0..3 pins it, else
 *  WW/CellWaterPreset; the default is Ultra = every water look as it ships today (no change by default). A row's own
 *  pin (WW_CELL_WATER_OCEAN=1 ...) wins over the preset. The water's SSR runs in every step (bungo: "we need ssr for
 *  water"). Cost per term: tests/watercost/watercost.py (offline, the real programs through the driver).
 *    Low     no water look: the game's water (fo4_water.prog) + the water SSR
 *    Medium  + clear colour, physical reflections, soft edge, river foam
 *    High    + the wind's wave spectrum (Ocean waves), shore foam
 *    Ultra   + sun glare (facets), contact foam, and the rows that ship OFF when the user turns them on
 *  Pure logic: celllooks.cpp asks wwWaterPresetLets for every water look row. */

namespace WwWaterPreset
{
enum Step { Low = 0, Medium = 1, High = 2, Ultra = 3 };
//! the water looks the preset steps through (celllooks.h WwLook water rows, by their meaning)
enum Term { Clear, Refl, SoftEdge, ShoreFoam, RiverFoam, Ocean, Glint, Contact, OffRow, NotWater };

//! the lowest step a term draws at
inline int minStep( Term t )
{
	switch ( t ) {
	case Clear: case Refl: case SoftEdge: case RiverFoam:
		return Medium;
	case Ocean: case ShoreFoam:
		return High;
	case Glint: case Contact: case OffRow:
		return Ultra;
	case NotWater:
		return Low;
	}
	return Ultra;
}

inline bool lets( int step, Term t )
{
	return step >= minStep( t );
}

//! "low", "Medium", "2" ... -> the step; anything else -> -1 (not set)
inline int parse( const char * s )
{
	if ( !s || !*s )
		return -1;
	auto eq = []( const char * a, const char * b ) {
		for ( ; *a && *b; a++, b++ ) {
			char c = *a >= 'A' && *a <= 'Z' ? char( *a - 'A' + 'a' ) : *a;
			if ( c != *b )
				return false;
		}
		return *a == 0 && *b == 0;
	};
	if ( eq( s, "low" ) || eq( s, "0" ) )
		return Low;
	if ( eq( s, "medium" ) || eq( s, "1" ) )
		return Medium;
	if ( eq( s, "high" ) || eq( s, "2" ) )
		return High;
	if ( eq( s, "ultra" ) || eq( s, "3" ) )
		return Ultra;
	return -1;
}
}	// namespace WwWaterPreset

#endif
