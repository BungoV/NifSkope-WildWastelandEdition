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

#include <vector>

namespace probebvh {
struct Bvh;
}

//! the coarse room grid as the fine fit reads it
struct ProbeFineGrid
{
	const qint32 * labels = nullptr;   //!< per cell: -1 solid / void, 0 outside, k = room k
	const quint8 * state = nullptr;    //!< per cell: nonzero = a triangle or glass touches it
	int dims[3] = { 0, 0, 0 };
	float origin[3] = { 0, 0, 0 };
	float cell = 16.0f;
};

struct ProbeFineSpec
{
	double h = 16.0;                //!< the lattice step
	qint64 maxPoints = 6000000;     //!< per room; the step grows past it
	int maxBoxes = 64;              //!< per room
	double minGain = 12288.0;       //!< u3 a box must add to the room's cover
	double poke = 3.0;              //!< a face may stand this far past the triangle in front of it
	int ownerReach = 3;             //!< coarse cells a room's air owns into the solid and void around it
};

struct ProbeFineStats
{
	qint64 points = 0, rays = 0, freeEdges = 0, snapRays = 0;
	qint64 roomPts = 0, coveredPts = 0, foreignPts = 0;
	int rooms = 0, boxes = 0;
	double msOwner = 0, msRays = 0, msCover = 0;
};

/*! Per room (index r = room r + 1): its boxes, empty where the room has no air on the lattice (keep the grid fit).
 *  `yaw` per room in radians (the footprint frame), `bvhO` the x y the tree's triangles were moved by. */
std::vector<std::vector<ProbeVolumeShape>> probeVolumesFineFit( const probebvh::Bvh & bvh, const double bvhO[2],
	const ProbeFineGrid & g, int rooms, const std::vector<double> & yaw, const ProbeFineSpec & spec, ProbeFineStats * stats );
