#ifndef PROBEROOMS_H
#define PROBEROOMS_H

//! ROOM LABELS PER VOXEL:
//! The bounce grid's voxels are bigger than a wall is thick: a voxel on one side of a two-unit wall blended
//! into a fragment on the other side is a light leak.

#include "probeplace.h"

#include <QString>

#include <vector>

struct ProbeRoomSpec
{
	float cell = 16.0f;         //!< the fine grid's cell, raised until it fits maxCells
	int maxCells = 8000000;
	float pinch = 48.0f;        //!< a room's core is the air deeper than this
	QString red;
	//! the box the grid may span (world; lo > hi on an axis: that axis is not clipped).
	float clipLo[3] = { 1.0f, 1.0f, 1.0f };
	float clipHi[3] = { 0.0f, 0.0f, 0.0f };
	//! keep the working grids (ProbeRooms::state, labels, dist2, facing) for the Space stage (probevolumes.h)
	bool keep = false;
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
	std::vector<qint32> openings;   //!< the opening cells (step 4, air only), the threshold stand-in's input
	qint64 cellsMaskOpen = 0, cellsMaskSolid = 0;   //!< cells masked triangles touch, left air / solid
	int cores = 0, coresOutdoors = 0, pockets = 0, folded = 0;
	int clippedFaces = 0;           //!< the soup box's faces the clip cut (bits -x +x -y +y -z +z)
	double ms = 0;
	/*! with ProbeRoomSpec::keep, per cell: 0 air / 1 solid / 2 glass; the unfolded label (-1 solid or
	 *  glass, 0 outdoors); the squared distance to solid or glass (cells); the one-sided faces' summed normal x 32
	 *  (saturated; a wall's front side reads along it). Empty otherwise. */
	std::vector<quint8> state;
	std::vector<qint32> labels;
	std::vector<float> dist2;
	std::vector<qint8> facing;  //!< 3 per cell

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

//! THE GI VOLUMES' INTERFACE:
//! The outdoors is one volume, room 0, and has no shapes: a pixel no interior shape holds reads the outdoor GI.
struct GiVolumeShape
{
	enum Type { Box = 0, Cylinder = 1, Wedge = 2, Capsule = 3, Sphere = 4 };
	int type = Box;
	float center[3] = { 0, 0, 0 };      //!< world
	float size[3] = { 0, 0, 0 };        //!< box: edges; cylinder: x, y diameters, height; wedge: edges (full height at
	                                    //!< -y, none at +y); capsule: diameter, diameter, end to end on z; sphere: diameters
	float rotation[3] = { 0, 0, 0 };    //!< degrees about x, y, z (shape -> world = Rz Ry Rx)
	int room = 1;
};

struct GiThreshold
{
	GiVolumeShape shape;                //!< the opening's extent (its room unused)
	int rooms[2] = { 0, 0 };            //!< the two volumes it joins
	float blendWidth = 0.0f;            //!< game units each side; 0: 1.5 GI voxels
};

struct GiVolumeSet
{
	std::vector<GiVolumeShape> shapes;  //!< interior rooms only
	std::vector<GiThreshold> thresholds;
	QString source;                     //!< who made it ("rooms grid stand-in", the fitter's name)
	QString census;                     //!< one line: rooms, shapes, coverage
};

/*! The stand-in until the fitter lands: each interior room's air as boxes (greedy, grown from a seed while the next
 *  slab holds the room's air and nothing of another room's; at most maxPerRoom a room), each connected run of
 *  opening cells between two rooms a threshold (its bounding box). The census counts the room air the boxes hold. */
GiVolumeSet giVolumesFromRooms( const ProbeRooms & r, int maxPerRoom = 64 );

#endif // PROBEROOMS_H
