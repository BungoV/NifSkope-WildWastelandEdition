#ifndef WW_CELLFGCUT_H
#define WW_CELLFGCUT_H

/* lane VKFAR14 (row G3 leftover): which ring-0 cells the live far ground draws, and whether an open prepares it.
 * A plain open cuts the loaded block (it draws its own ground). A streamed open's ring 0 is one shape a cell, uncut
 * (cellfarlod.cpp, CELLSTREAM1): crossings only hide and show those shapes (Stream::showFar), so the live ground is
 * prepared over every ring-0 cell and the open block only centres the cached pages. Qt-free for the offline gate
 * (tests/features/far-ground/fgstream). Red WW_CELL_FARGROUND_RED=nostream: a streamed open prepares nothing (the old rule). */

#include <cstring>

namespace CellFarGround
{
struct Cut
{
	int x0 = 0, y0 = 0, x1 = -1, y1 = -1;	// the open block in cells
	bool off = false;						// streamed: every cell drawn, the block only centres
	bool has( int wx, int wy ) const { return !off && wx >= x0 && wx <= x1 && wy >= y0 && wy <= y1; }
	float centreX( float cell ) const { return float( x0 + x1 + 1 ) * 0.5f * cell; }
	float centreY( float cell ) const { return float( y0 + y1 + 1 ) * 0.5f * cell; }
};

//! Whether ring 0's append prepares the live ground. red = WW_CELL_FARGROUND_RED's value (may be null).
inline bool streamPrepares( bool stream, bool built, bool noTerrainRed, const char * red )
{
	if ( !built || noTerrainRed )
		return false;
	return !stream || !( red && std::strcmp( red, "nostream" ) == 0 );
}
}

#endif
