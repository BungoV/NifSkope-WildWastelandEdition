/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef LODGEN_H
#define LODGEN_H

#include <QString>

class NifModel;
class EsmWorld;

/* LODGEN rung 1 (docs/LODGEN_PLAN.md): terrain .btr generation from LAND
 * records. The output replicates the vanilla chunk anatomy measured on
 * Commonwealth.4.-20.24.BTR (docs/LODGEN_ESM_LAYOUTS.md):
 *
 *   BSMultiBoundNode 'chunk'
 *     BSTriShape 'Land'         scale = dim, 12-byte verts (half pos +
 *                               bitangentX + half UV), NO normals — the
 *                               per-chunk _msn texture carries them
 *     BSMultiBoundNode 'WATER'  one 16-segment BSSubIndexTriShape per
 *                               distinct water height (quad per wet cell)
 *     BSMultiBound/AABB pairs   X/Y chunk-relative, Z absolute world
 *
 * v1 emits the full regular grid (dim*32+1 per side) rather than vanilla's
 * decimated ~1k verts; decimation via the vendored meshoptimizer is the
 * planned follow-up. Textures point at vanilla's existing per-chunk bakes.
 */
struct LodgenTerrainOptions
{
	int dim = 4;                //!< chunk edge in cells (4/8/16/32)
	bool water = true;
	//! texture path template; {ws}/{dim}/{x}/{y} are substituted
	QString textureBase = QStringLiteral( "Data\\Textures\\Terrain\\%1\\%1.%2.%3.%4.DDS" );
};

//! Build one terrain chunk into a fresh FO4 document. chunkX/chunkY are the
//! SW corner cell coordinates (dim-aligned, per vanilla file naming).
bool lodgenBuildTerrainChunk( NifModel * nif, const EsmWorld & world,
	int chunkX, int chunkY, const LodgenTerrainOptions & opts, QString * error );

#endif // LODGEN_H
