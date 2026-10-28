#pragma once

/*! lane VOLFIT2: the room shapes fitted on a fine lattice instead of the coarse room grid.
 *
 *  The room grid's cell grows with the block (Concord 61 units, Goodneighbor 39): a cell any triangle touches is solid,
 *  so boxes fitted on it stop a cell or more short of every wall. Per room, a lattice of `h`-unit steps in the room's
 *  footprint frame is flooded through segments no triangle cuts: from the room's grid air into the cells the room owns
 *  (its own air plus the solid and void cells nearest it). What the other rooms' and the outdoor air reach is foreign;
 *  what neither reaches (inside furniture, inside walls) is free to cover. Boxes grow greedily over the room's points and
 *  never take a foreign one; each face then moves onto the triangle in front of it. */

#include "probevolumes.h"

#include <functional>
#include <vector>

namespace probebvh {
struct Bvh;
}
namespace bakert {
class Scene;
}

//! the coarse room grid as the fine fit reads it
struct ProbeFineGrid
{
	const qint32 * labels = nullptr;   //!< per cell: -1 solid / void, 0 outside, k = room k
	const quint8 * state = nullptr;    //!< per cell: nonzero = a triangle or glass touches it
	int dims[3] = { 0, 0, 0 };
	float origin[3] = { 0, 0, 0 };
	float cell = 16.0f;
	const std::vector<ProbeVolumeShape> * cuts = nullptr;   //!< threshold boxes: no flood crosses one
	const float * anchors = nullptr;   //!< 3 per room: the room's flood starts at its own air nearest the anchor
	bakert::Scene * scene = nullptr;   //!< the per-point rays on the GPU (null: the CPU tree; the answers are the same)
	std::function<bool( int, const double * )> backFace;   //!< ray direction d met triangle tri's back (solid rule)
	std::function<double( int, const double * )> facing;  //!< cosine of triangle tri's normal and direction d
	const std::vector<std::vector<ProbeVolumeShape>> * grid = nullptr;   //!< per room: the refined grid fit (VOLFIT3 starts there)
	const std::vector<ProbeVolumeShape> * openings = nullptr;   //!< HALLINDOOR1: room-to-outside thresholds (null: the old indoor rule)
	const std::vector<int> * windowRooms = nullptr;   //!< HALLINDOOR1: labels of the rooms behind those windows
};

struct ProbeFineSpec
{
	double h = 16.0;                //!< the lattice step
	qint64 maxPoints = 6000000;     //!< per room; the step grows past it
	int maxBoxes = 64;              //!< per room
	double minGain = 65536.0;       //!< u3 a box must add to the room's cover (a 40-unit cube)
	double poke = 3.0;              //!< a face may stand this far past the triangle in front of it
	int ownerReach = 3;             //!< coarse cells a room's air owns into the solid and void around it
	double grow = 4.0;              //!< coarse cells past the owned ones the room may grow into enclosed air
	bool exactEdges = false;        //!< cast every edge (no coarse-air shortcut)
	bool freeD = true;              //!< boxes may take points no flood reaches (inside furniture, walls)
	double margin = 0.85;           //!< a point this near volcheck's solid limit stops the floods (1 = exact)
	double marginOpen = 1.0;        //!< the same for the outdoor limit
	double ownedOut = 2.0;          //!< the room's own grid points stop the flood this far past the outdoor limit (0 never)
	double snapSteps = 2.0;         //!< a face reaches a wall (or threshold) this many steps away when all its points meet one
	int checkPasses = 2;            //!< refits after volcheck's flood over the boxes (room points it misses go foreign)
	int sphereK = 32;               //!< the indoor rule (volcheck's): K rays, at most skyMax of the upper ones
	double skyMax = 0.25, openMax = 0.25, openReach = 1536.0;   //!< miss the world, at most openMax miss or pass reach
	// lane VOLFIT3
	bool legacy = false;            //!< VOLFIT2's fit (red "volfit2"): the lattice boxes as they come, curved rooms on the grid
	bool jitter = true;             //!< rooms square to the world sit on volcheck's own jittered lattice
	double skin = 8.0;              //!< a room point this near a triangle is no gap (volcheck's tolSkin)
	double badW = 2.0;              //!< a foreign point in a shape costs this many uncovered room points
	double badWin = 4.0;            //!< HALLINDOOR1: badW in a room behind a window (its hall air now reads indoor)
	int extras = 8;                 //!< lattice boxes tried per room on top of the moved grid boxes
	double budget = 1.45;           //!< all rooms' shapes at most this times the grid fit's (the bar: x1.5 VOLRAY1's)
	int rounds = 2;                 //!< face-move passes over the grid boxes
	// lane VOLFIT4
	double badO = 16.0;             //!< a non-room point inside another room's grid shape costs this many (a cross-wall overlap)
	// lane VOLFIT5 (neither changes a byte of the result; 0 = VOLFIT4's path)
	bool edgeGpu = true;            //!< the edge rays go to g.scene in batches (any-hit on the RT tracer)
	bool bigPar = true;             //!< the big rooms are fitted side by side, not one at a time
};

struct ProbeFineStats
{
	qint64 points = 0, rays = 0, freeEdges = 0, snapRays = 0;
	qint64 roomPts = 0, coveredPts = 0, foreignPts = 0;
	qint64 encTests = 0, encGrown = 0;   //!< indoor-rule tests, points grown past the owned cells
	qint64 checkLost = 0;                //!< room points the check flood missed (made foreign before a refit)
	qint64 pruned = 0;                   //!< snapped boxes the others covered within minGain (dropped)
	int pickGrid = 0, pickMoved = 0, extrasUsed = 0, facesMoved = 0, merged = 0;   //!< VOLFIT3's per-room picks
	int gridShapes = 0;
	int rooms = 0, boxes = 0;
	double msOwner = 0, msRays = 0, msCover = 0;
	double msClass = 0, msGreedy = 0, msMoves = 0;   //!< VOLFIT4's profile (thread-summed, as msRays/msCover)
	double msBig = 0, msWall = 0;                    //!< wall clock: the big rooms, the whole fit
	qint64 edgeCast = 0;                             //!< VOLFIT5: edge rays sent to the scene
	double msSmall = 0;                              //!< VOLFIT6: wall clock of the small rooms
	QString prof;                                    //!< VOLFIT6: the census tail of the edge stage's profile
};

/*! Per room (index r = room r + 1): its boxes, empty where the room has no air on the lattice (keep the grid fit).
 *  VOLFIT3: with `g.grid` set, each room's refined grid shapes have their box faces moved on the lattice (room points in,
 *  foreign points out), lattice boxes are added where they cut the most uncovered air, and every room keeps whichever
 *  set scores better (uncovered room points + badW x foreign points) within the shape budget over all rooms.
 *  `yaw` per room in radians (the footprint frame), `bvhO` the x y the tree's triangles were moved by. */
std::vector<std::vector<ProbeVolumeShape>> probeVolumesFineFit( const probebvh::Bvh & bvh, const double bvhO[2],
	const ProbeFineGrid & g, int rooms, const std::vector<double> & yaw, const ProbeFineSpec & spec, ProbeFineStats * stats );
