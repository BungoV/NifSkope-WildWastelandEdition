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
	/*! Decimation target, triangles per cell AT DIM 4 (vanilla dim4 chunks
	 * run ~130/cell). The real budget is the chunk total (x16): vanilla
	 * holds ~2100 tris per chunk on EVERY ring, so per-cell density falls
	 * 4x per ring (measured 128 -> 32 -> 8 -> 2). 0 = no decimation, emit
	 * the full 32x32-per-cell grid. */
	int targetTrisPerCell = 130;
	/*! CS profile: store per-vertex WORLD height deltas to the parent ring's
	 * surface in Eye Data, enabling continuous (geomorphed) LOD transitions.
	 * Widens the vertex stride -- gated on the stock-engine tolerance check. */
	bool geomorph = false;
	/*! CS terrain profile: add COLORS to the Land desc — R = LTEX material
	 * class (0 dirt, 32 grass, 64 forest floor, 96 rock, 128 road/concrete,
	 * 160 sand, 192 marsh/wet, 224 snow), G = flow-accumulation wetness,
	 * B = heightfield AO, A = 255. The Physical Weathers tie-in. */
	bool terrainIdentity = false;
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
	bool bakeAO = true;         //!< ray-cast per-placement AO into channel B
	int lodLevel = -1;          //!< MNAM slot; -1 = pick by dim (4->0, 8->1, 16->2, 32->3)
	/*! Impostor card library: a directory of <formid8hex>_front.png /
	 * _side.png / <formid8hex>.txt baked by the WW_IMPOSTOR_BAKE hook
	 * (tools/bake_impostor_cards.sh drives it). When the requested MNAM
	 * slot is EMPTY, two crossed card quads substitute instead of falling
	 * back to a nearer (heavier) slot; the card DDS (BC1 punch-through
	 * alpha) is written beside the PNGs and referenced as
	 * Data\Textures\Lodgen\Cards\<id>.DDS — ship that directory there. */
	QString impostorDir;
};

bool lodgenBuildObjectChunk( NifModel * nif, const EsmWorld & world,
	int chunkX, int chunkY, const LodgenObjectOptions & opts,
	QString * manifestOut, QString * error );

/* Rung 3: bake a chunk's terrain textures from the LAND splat — evaluate
 * the CK paint (per-quadrant LTEX palette + 17x17 opacities) with the
 * source landscape textures world-tiled, plus a model-space normal map
 * from the heightfield. Uncompressed BGRA DDS (BC1 later). */
bool lodgenBakeTerrainTextures( const EsmWorld & world, int chunkX, int chunkY,
	int dim, const QString & dataRoot, const QString & outDir, QString * error );

/* Object atlas pass (vanilla-style: one 4096x2048 sheet per worldspace).
 * Post-processes generated .bto files: shapes whose UVs sit inside [0,1]
 * move onto 256x256 atlas cells (diffuse + matching normal sheet, 2-texel
 * inset against mip bleed) and their texture sets are repointed at
 * atlasGameBase (+".DDS"/"_n.DDS"); tiling shapes keep their source
 * textures. Writes atlasFileBase(.DDS/_n.DDS) and rewrites the files. */
bool lodgenBuildAtlas( const QStringList & btoPaths, const QString & dataRoot,
	const QString & atlasFileBase, const QString & atlasGameBase,
	QString * error );

#endif // LODGEN_H
