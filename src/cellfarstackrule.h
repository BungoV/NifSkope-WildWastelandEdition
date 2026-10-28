#pragma once
/* lane CELLUNI5 (OPENREDS11's leftover): whether the near cells refuse the far LOD bake's plugin stack when the two
 * differ, and why. cellFarLodStackFor (cellfarlod.cpp) asks it before FARLOD3's adoption. Pure: no Qt, no GL, so
 * tests/farstackrule runs it alone.
 *   WW_CELL_FARLOD_RED=ownstack  -> refused (FARLOD3's red: the near cells keep their own load order)
 *   WW_CELL_FARLOD_NEARONLY=1    -> refused: the near gate's arm draws the far planes and nothing else, so the near
 *                                   cells must load what the off path loads, or a picture against it shows the
 *                                   stack, not the planes
 *   WW_CELL_FARLOD_RED=nearadopt -> the red of that rule: NEARONLY adopts the bake's stack as before this lane */
#include <cstring>

enum class CellFarStackRefusal { Adopt, RedOwnStack, NearOnly };

inline CellFarStackRefusal cellFarStackRefusal( const char * red, int nearOnly )
{
	const bool redOwn = red && std::strcmp( red, "ownstack" ) == 0;
	const bool redNearAdopt = red && std::strcmp( red, "nearadopt" ) == 0;
	if ( redOwn )
		return CellFarStackRefusal::RedOwnStack;
	if ( nearOnly != 0 && !redNearAdopt )
		return CellFarStackRefusal::NearOnly;
	return CellFarStackRefusal::Adopt;
}
