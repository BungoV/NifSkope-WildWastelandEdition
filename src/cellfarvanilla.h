#ifndef CELLFARVANILLA_H
#define CELLFARVANILLA_H

/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENSE BLOCK *****/

/*
 * The game's own far LOD (.btr terrain, .bto objects incl. tree LOD) in the cell view, for the chunks our
 * LOD files do not cover. Terrain is clipped exactly to each ring's region, clip-line vertices snapped to the LAND.
 */

#include <QModelIndex>
#include <QString>
#include <QStringList>

#include <functional>
#include <vector>

class NifModel;

struct WwFarVanRing
{
	int level = 4;                                //!< the authored level (cells a chunk edge)
	int x0 = 0, y0 = 0, x1 = -1, y1 = -1;         //!< the ring's footprint, inclusive cells
	int cx0 = 0, cy0 = 0, cx1 = -1, cy1 = -1;     //!< the footprint inside (cut), inclusive cells
	int ux0 = 0, uy0 = 0, ux1 = -1, uy1 = -1;     //!< our coverage of this ring (none when ux1 < ux0)
	bool noOursCut = false;                       //!< debug "vanboth": draw over our coverage too
	bool noCut = false;                           //!< debug "nocut" (band only): draw over the cut too
	QString tag;                                  //!< "r0".. for the shape names

	// ---- out
	int chunksOurs = 0, chunksVanilla = 0, chunksBoth = 0, chunksNone = 0;
	qint64 terrainTris = 0, objectTris = 0, clipTris = 0, droppedTris = 0, shapes = 0;
	qint64 snapped = 0, landRetyped = 0, skirtTris = 0;          //!< landRetyped: .btr LOD-landscape shaders drawn as the default type
	qint64 btoSwayShapes = -1, btoSwayTrees = 0, btoSwayShared = 0;  //!< lane FAR5: Game LOD trees sway (-1: off)
	double snapMax = 0.0;
	std::vector<int> waterBlocks;                 //!< document blocks of water shapes (BSWaterShaderProperty)
	bool stopped = false;                         //!< the budget stopped it
	QStringList notes;
};

/*! Appends each ring's vanilla chunks under `parent` (the caller holds updates and runs updateModel).
 *  `shift` is subtracted from every chunk root's translation. `landZ(wx, wy, &z)` gives the LAND's height
 *  (false = none); `overBudget()` is asked before each chunk. */
bool wwFarVanAppend( NifModel * nif, const QModelIndex & parent, const QString & ws, const float shift[3],
	std::vector<WwFarVanRing> & rings, const std::function<bool( float, float, float * )> & landZ,
	const std::function<bool()> & overBudget, QString * error );

//! The game's .bto object triangles at `level` whose centroid is inside cells x0..x1, y0..y1 and
//! outside hx0..hx1, hy0..hy1 -- counted, nothing appended. `chunksRead` = the chunk files found.
qint64 wwFarVanObjectCensus( const QString & ws, int level, int x0, int y0, int x1, int y1,
	int hx0, int hy0, int hx1, int hy1, int * chunksRead );

#endif // CELLFARVANILLA_H
