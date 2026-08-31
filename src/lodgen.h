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
	/*! Decimation target, triangles per cell (vanilla dim4 chunks run
	 * ~130). 0 = no decimation, emit the full 32x32-per-cell grid. The
	 * simplifier locks the border ring so the skirt stays exact. */
	int targetTrisPerCell = 130;
	//! texture path template; {ws}/{dim}/{x}/{y} are substituted
	QString textureBase = QStringLiteral( "Data\\Textures\\Terrain\\%1\\%1.%2.%3.%4.DDS" );
};

//! Build one terrain chunk into a fresh FO4 document. chunkX/chunkY are the
//! SW corner cell coordinates (dim-aligned, per vanilla file naming).
bool lodgenBuildTerrainChunk( NifModel * nif, const EsmWorld & world,
	int chunkX, int chunkY, const LodgenTerrainOptions & opts, QString * error );

/* Rung 2: object .bto stitching. REFRs with LOD models are gathered per
 * chunk, their per-object _LOD.nif meshes loaded from the data root,
 * transformed into miniature chunk space and welded into per-material
 * shapes — per-cell segments at dim4, one segment otherwise. No atlas:
 * shapes reference the source LOD textures directly (xLODGen-legal).
 *
 * With identity on, the output vertex format gains COLORS: R+G = 16-bit
 * per-chunk object index, B = 255 (AO bake slot), A = the source mesh's
 * own alpha (tree sway weight) — the FO4CS extra-data channel contract
 * from docs/TO_BE_IMPLEMENTED.md. A manifest text file (one line per
 * index: formID, base type, position, scale) is written beside the chunk.
 */
struct LodgenObjectOptions
{
	int dim = 4;
	QString dataRoot;           //!< Data folder holding meshes\\lod\\... sources
	bool identity = true;       //!< vertex-colour identity + manifest (CS profile)
	int lodLevel = -1;          //!< MNAM slot; -1 = pick by dim (4->0, 8->1, 16->2, 32->3)
};

bool lodgenBuildObjectChunk( NifModel * nif, const EsmWorld & world,
	int chunkX, int chunkY, const LodgenObjectOptions & opts,
	QString * manifestOut, QString * error );

#endif // LODGEN_H
