#ifndef FARTILE_H
#define FARTILE_H

/* lane SHADOWSHIMMER5 (FARTILESPLIT, 2026-11-02): how many cells a side an instanced far ring's cull tile spans.
 *
 * src/features/far-lod/lodinative.cpp groups an instanced far bucket's rows into tiles of N x N cells (key = the placement cell >> log2 N),
 * one indirect command a run of equal (tile, sub-mesh); gl/cellfarinst.cpp culls a whole tile by its sphere (the camera,
 * every sun cascade's box, SHADOWSHIMMER4's view-slice test). 4 x 4 cells made spheres of ~10.3 k units median radius
 * on Concord's rings 0-2, so a cascade or a view slice that touched a tile's edge drew all of it.
 * WW_CELL_FARINST_TILE=1|2|4|8 picks N (default 2); red WW_CELL_FARINST_TILE_RED=1 = 4, the old tiles. Read while the
 * far ring is built (a launch / reopen key). The cull stays conservative (a tile is drawn whole or not at all, its
 * sphere bounds every row sphere), so the drawn pixels do not change; only the number of rows the culls let through.
 * Gate: tests/fartilesplit. */

#include <cstdint>

namespace WwFarTile
{

constexpr int kDefaultCells = 2;
constexpr int kRedCells = 4;

//! log2 of an allowed tile side (1, 2, 4, 8 cells); anything else (0 = unset) gives the default's
inline int shiftOf( int cells, bool red )
{
	if ( red )
		return 2;
	switch ( cells ) {
	case 1: return 0;
	case 2: return 1;
	case 4: return 2;
	case 8: return 3;
	default: return 1;	// kDefaultCells
	}
}

//! the tile of a placement cell: x tile in the high 16 bits, y tile in the low (arithmetic shift: floor for negatives)
inline uint32_t key( int cellX, int cellY, int shift )
{
	return ( uint32_t( ( cellX >> shift ) + 32768 ) << 16 ) | uint32_t( ( cellY >> shift ) + 32768 );
}

} // namespace WwFarTile

#endif
