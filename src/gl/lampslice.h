#ifndef WW_LAMPSLICE_H
#define WW_LAMPSLICE_H

/* lane PERFFIX2 b (item 5d, 11-01): THE LAMP STORE'S PER-FRAME BUDGET (time slicing).
 *
 * The stored lamp shadows (celllights.cpp lampShadowPass) draw a lamp's static map once, when its slot first holds it.
 * A cell open (or a big slot change) stored all 16 at once: the 102 ms first-build frame (PERF2 T1, Memory Den). Now a
 * frame spends at most `slice` cube faces on first stores (6 an omni lamp, 5 a wall lamp: its front face + 4 halves),
 * in the dirty list's order (nearest the camera first); the first store of a frame always goes, so a budget under one
 * lamp still makes progress. A slot that waits is read unshadowed (as a light past the slot budget) until its map is
 * drawn; restores (blits) and movers are not budgeted. WW_CELL_LAMPSHADOW_SLICE=0: every store in one frame (the old). */

namespace WwLampSlice
{

struct Budget
{
	int slice = 0;	// faces a frame; 0 = no limit
	int spent = 0;
	//! may a store of `faces` faces go this frame (and spend them)
	bool spend( int faces )
	{
#ifdef LAMPSLICE_RED_NOFLOOR
		if ( slice > 0 && spent + faces > slice )	// the red: no first-store floor (a lamp over the budget never goes)
			return false;
#else
		if ( slice > 0 && spent > 0 && spent + faces > slice )
			return false;
#endif
		spent += faces;
		return true;
	}
};

}	// namespace WwLampSlice

#endif
