/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef CELLSSRWATER_H
#define CELLSSRWATER_H

/*! Lane FIX1101A (bungo 10-31, RULED: "we need ssr for water"): the water's screen-space reflections are their own
 *  part of the SSR pass, ON by default outdoors and indoors, whatever the SSR row (WW_CELL_SSR / WW/CellSsr) and the
 *  SSR outdoors row (WW_CELL_SSR_EXT) say; those two keep deciding the opaque surfaces' reflections exactly as they
 *  ship. The water part: WW_CELL_SSR_WATER (pin, 0 = off) or WW/CellSsrWater (default on).
 *  Red WW_CELL_SSR_WATER_RED=1: the old coupling (the water's rays run only while the SSR row is on, so a frame with
 *  WW_CELL_SSR=0, as every shot runner pins, draws no water reflection of the scene).
 *  Pure logic, no GL: cellssr.cpp asks these, tests/waterssr/verdict_test.cpp tests them. */

//! the pass's verdict, as cellssr.cpp prints it: 0 no cell, 1 off (row), 2 off (exterior, SSR outdoors row off),
//! 3 off (no obscurance pass), 4 on, 5 water only (exterior, SSR outdoors row off), 6 water only (SSR row off)
struct WwSsrVerdictIn
{
	bool cell = false;			// a cell's lighting is known
	bool rowOn = false;			// the SSR row (opaque surfaces)
	bool ext = false;			// the SSR outdoors row
	bool interior = false;
	bool waterWanted = false;	// a drawn water body reflects (cellwater.cpp wwCellWaterSsrWanted)
	bool waterRow = true;		// the water part (WW_CELL_SSR_WATER)
	bool red = false;			// WW_CELL_SSR_WATER_RED=1
	bool aoTargets = false;		// the obscurance pass's targets exist this frame
};

//! the opaque surfaces' reflections run (the SSR row, and outdoors the SSR outdoors row)
inline bool wwSsrFullPart( const WwSsrVerdictIn & v )
{
	return v.cell && v.rowOn && ( v.interior || v.ext );
}

//! the water's rays run; the red ties them to the SSR row as before
inline bool wwSsrWaterPart( const WwSsrVerdictIn & v )
{
	return v.cell && v.waterWanted && ( v.red ? v.rowOn : v.waterRow );
}

//! does this frame's pass need the obscurance pass's targets (cellssr.cpp wwCellSsrWantsTargets, minus the bounce)
inline bool wwSsrWantsTargets( const WwSsrVerdictIn & v )
{
	return wwSsrFullPart( v ) || wwSsrWaterPart( v );
}

inline int wwSsrVerdict( const WwSsrVerdictIn & v )
{
	if ( !v.cell )
		return 0;
	const bool full = wwSsrFullPart( v ), water = wwSsrWaterPart( v );
	if ( !full && !water )
		return v.rowOn ? 2 : 1;
	if ( !v.aoTargets )
		return 3;
	if ( full )
		return 4;
	return ( v.rowOn && !v.interior && !v.ext ) ? 5 : 6;
}

//! the pass draws the water's rays alone (no opaque surface reflects): verdicts 5 and 6
inline bool wwSsrWaterOnly( const WwSsrVerdictIn & v )
{
	return !wwSsrFullPart( v );
}

#endif
