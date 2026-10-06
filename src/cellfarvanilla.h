#ifndef CELLFARVANILLA_H
#define CELLFARVANILLA_H

/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENSE BLOCK *****/

/*
 * Lane FARLOD1 (2026-10-05): the game's own far LOD in the cell view -- the chunks our FO4CSLOD files do
 * not cover, or every chunk when the Far LOD type is Vanilla.
 *
 *   terrain  meshes/terrain/<ws>/<ws>.<lvl>.<x>.<y>.btr
 *   objects  meshes/terrain/<ws>/objects/<ws>.<lvl>.<x>.<y>.bto  (Fallout 4's tree LOD is inside the .bto:
 *            the game's Data holds no .btt -- 0 of 3,525 terrain files, measured on his unpacked Data)
 *
 * Read through the resource manager (archives, then loose folders and WW_LODGEN_RESOURCES), the blocks moved
 * whole into the cell document (their shader, textures and alpha untouched), so they take the cell's lights,
 * fog and weather like any other document shape.
 *
 * One ring at the game's authored level for its distance (4, 4, 8, 16, 32 for the band and rings 0-3). The
 * ring's region is its footprint minus the footprint inside it (the loaded block for the band) minus OUR
 * coverage of the ring: terrain triangles are CLIPPED to that region exactly (no overlap, no gap with our
 * ring or the next), the vertices on a clip line taken to the LAND's height there (no step at the seam);
 * object triangles are kept by their centre. A chunk is "ours" when our coverage holds every cell the ring
 * needs of it, "vanilla" when it holds none and the file exists, "both" when it holds some, "none" when the
 * file is missing.
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
	bool noOursCut = false;                       //!< red "vanboth": draw over our coverage too
	bool noCut = false;                           //!< red "nocut" (band only): draw over the cut too
	QString tag;                                  //!< "r0".. for the shape names

	// ---- out
	int chunksOurs = 0, chunksVanilla = 0, chunksBoth = 0, chunksNone = 0;
	qint64 terrainTris = 0, objectTris = 0, clipTris = 0, droppedTris = 0, shapes = 0;
	qint64 snapped = 0, landRetyped = 0, skirtTris = 0;          //!< landRetyped: .btr LOD-landscape shaders drawn as the default type
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

#endif // CELLFARVANILLA_H
