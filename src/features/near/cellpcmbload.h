#ifndef CELLPCMBLOAD_H
#define CELLPCMBLOAD_H

/* Lane PCMB1 (2026-11-06): the Qt side of the precombined reader (src/features/near/cellpcmb.h). Finds the
 * "<plugin> - Geometry.csg" a packed object names by its file hash in the data folder, reads the zlib chunks it needs
 * (kept in a capped cache shared by every loader thread) and expands one precombined shape to world-space LOD0. */

#include "features/near/cellpcmb.h"

#include <QByteArray>
#include <QList>
#include <QString>

namespace WwPcmb
{

/*! The geometry of one precombined shape: `nif` the file's bytes, `extraBlocks` the shape's Extra Data List links.
 *  Every BSPackedCombinedSharedGeomDataExtra among them is expanded into `g` (world space, LOD0). False, with `why`,
 *  when the NIF does not walk, a block does not end exactly, the CSG is not found, or an object disagrees with the
 *  CSG's directory. */
bool loadShape( const QString & dataRoot, const QByteArray & nif, const QList<qint32> & extraBlocks, Geometry & g,
	QString * why = nullptr );

//! WW_CELL_PCMB_RED=refs: no precombined mesh, every ref drawn as itself (the code before PCMB1).
bool redRefs();

//! Counters for the cell view's `cell pcmb:` line: shapes filled, shapes refused, CSG chunks read, chunk cache hits.
struct LoadStats
{
	qint64 shapes = 0, refused = 0, chunksRead = 0, chunkHits = 0, vertices = 0, triangles = 0;
};
LoadStats loadStats();

} // namespace WwPcmb

#endif // CELLPCMBLOAD_H
