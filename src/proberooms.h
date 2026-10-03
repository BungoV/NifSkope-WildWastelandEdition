#ifndef PROBEROOMS_H
#define PROBEROOMS_H

/*! ROOM LABELS PER VOXEL (lane ROOMCLAMP1, 2026-10-03; docs/PRTP_PLAN.md 2am).
 *
 *  The bounce grid's voxels are bigger than a wall is thick: a voxel on one side of a two-unit wall blended into
 *  a fragment on the other side is a light leak. Every point of the air gets a room, rebuilt from the bake's soup
 *  when the bake is relit (no file format change), and the grid's blend keeps only the voxels of the sample's own
 *  room (probegi.h step 3, res/shaders/cell_lights.glsl cellGiSample):
 *    1. a fine grid (16 units, raised until it fits) over the soup; a cell any triangle touches is solid (the
 *       separating-axis test: a thin wall always closes), a cell blended glass touches is glass
 *    2. the air's distance to the nearest solid or glass cell (exact Euclidean); the air deeper than `pinch`
 *       (and outside every door box) is a room's core: openings narrower than 2 x pinch, doors and cracks in a
 *       shell split rooms. The cores flood 6-connected; a core that reaches the grid's border is outdoors (0)
 *    3. the rest of the air takes the room of the core it is reached from first, deepest first (a watershed);
 *       air no core reaches floods into rooms of its own (a closet narrower than the pinch)
 *    4. an air cell beside a cell of another room is an opening and names both; a glass cell names the rooms on
 *       its two sides (one value per side: the window between a room and the outdoors)
 *  A surface reads its room at P + N x 0.75 cell, or P + N x 1.75 cell when that is still solid (the surface's
 *  own cell). Refuters (WW_CELL_ROOMS_RED): "conn26" the floods 26-connected, "boxes" each room its bounding box
 *  (the old room boxes), "glasswall" glass names no room. */

#include "probeplace.h"

#include <QString>

#include <vector>

struct ProbeRoomSpec
{
	float cell = 16.0f;         //!< the fine grid's cell, raised until it fits maxCells
	int maxCells = 8000000;
	float pinch = 48.0f;        //!< a room's core is the air deeper than this
	QString red;
};

struct ProbeRooms
{
	bool ok = false;
	QString error;
	float origin[3] = { 0, 0, 0 };  //!< the grid's min corner (world)
	float cell = 16.0f;
	int dims[3] = { 0, 0, 0 };
	std::vector<qint16> a, b;       //!< per cell (x fastest): the room, the room across an opening; -1 none
	int rooms = 0;                  //!< rooms 1 .. rooms (0 = outdoors)
	qint64 cellsAir = 0, cellsSolid = 0, cellsGlass = 0, cellsDoor = 0, cellsCore = 0, cellsLeft = 0;
	qint64 cellsOpening = 0, glassBoth = 0;
	int cores = 0, coresOutdoors = 0, pockets = 0, folded = 0;
	double ms = 0;

	size_t index( int x, int y, int z ) const
	{
		return ( size_t( z ) * size_t( dims[1] ) + size_t( y ) ) * size_t( dims[0] ) + size_t( x );
	}
	//! the cell at world `p`: its two names (-1 none); false outside the grid
	bool at( const double p[3], int out[2] ) const;
	//! a surface's room: at P + N x 0.75 cell, else P + N x 1.75 cell, else of the 8 cells round either read in
	//! the surface's plane the air cell nearest its read, the 0.75 read first on a tie (the shader's rule, which also
	//! skips a cell whose blend has no weight); false: none
	bool surface( const double p[3], const double n[3], int out[2] ) const;
};

bool probeRoomsBuild( const ProbeSoup & soup, const ProbeRoomSpec & spec, ProbeRooms * out );
//! One census line.
QString probeRoomsCensusText( const ProbeRooms & r );
//! A cell's two names as the shader's one float: (a + 1) x 4096 + (b + 1), a and b in -1 .. 4094
inline float probeRoomsPack( int a, int b )
{
	return float( ( a + 1 ) * 4096 + ( b + 1 ) );
}
//! gi_rooms.bin: float32 origin[3], cell; int32 dims[3], rooms; then int16 a, int16 b per cell
bool probeRoomsDump( const ProbeRooms & r, const QString & path, QString * err );

#endif // PROBEROOMS_H
