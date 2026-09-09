/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef LODGEN_H
#define LODGEN_H

#include <QString>
#include <QStringList>

class NifModel;
class EsmWorld;

/* ---------------------------------------------------------------------------
 * THE RESOURCE STACK (bungo, 2026-09-06: "Instead of top wins, we use MO2's
 * standard, the last one in the order overrides the previous ones")
 *
 * An ordered list of entries, each a FOLDER (its loose files, and the archives
 * sitting in it) or an ARCHIVE FILE (.ba2/.bsa). The LAST entry overrides the
 * earlier ones, which is Mod Organizer's priority and the opposite of the game
 * manager's Settings > Resources list.
 *
 * It is indexed as ONE BA2File, because BA2File's map is FIRST-wins
 * (addPackedFile returns null for a name it already holds), in two passes, each
 * walking the stack from the LAST entry to the first:
 *
 *   pass 1  every folder's LOOSE files - handed to BA2File one data
 *           sub-directory at a time (meshes\, textures\, materials\, ...), which
 *           is what keeps archives out of this pass;
 *   pass 2  the archives - the entry itself when it names one, otherwise the
 *           .ba2/.bsa files sitting directly in the folder, mod archives before
 *           DLC before the game's own, as the engine loads them.
 *
 * So a loose file beats an archive wherever the archive sits in the list, which
 * is the engine's rule, and among archives the later entry wins.
 *
 * When the stack is empty nothing is built and every read behaves exactly as it
 * did before it existed: loose --data-root first, then the mesh index over the
 * game manager's folders / GameManager::get_file. When it is set it is
 * consulted FIRST, for meshes, textures, materials and .lodm alike.
 * --------------------------------------------------------------------------- */

//! Install the stack (empty clears it). Process-wide; the index is rebuilt lazily.
void lodgenSetResources( const QStringList & entries );
//! The stack as it was set, in LAST-WINS order.
QStringList lodgenResources();
/*! The stack flattened into the order an index wants it, FIRST-WINS: pass one's
 *  loose data sub-directories, then pass two's archives. It is what lodgen
 *  feeds its own BA2File and what the panel hands the game manager, so the
 *  viewport and the generator see one worldspace. */
QStringList lodgenResourceSearchPaths();
/*! Where a relative asset path actually resolves from, for `lodgen --probe` and
 *  its harness: `entry` is the stack entry that supplied it (empty when it came
 *  from dataRoot or the game manager), `kind` is "loose" or "archive", `path` is
 *  the file on disk for a loose hit. False when nothing has it. */
bool lodgenProbeAsset( const QString & dataRoot, const QString & relPath,
	QString * entry, QString * kind, QString * path, QByteArray * bytes );
/*! Up to `limit` of the paths the stack's index holds, sorted. For a harness
 *  that has to name a file inside an archive without guessing one. */
QStringList lodgenListResourceFiles( int limit );

/* --- Mod Organizer 2 --------------------------------------------------------
 * Launched from MO2's executable list a process sees the VIRTUAL Data folder -
 * every enabled mod's loose files and archives layered by priority - and the
 * profile's plugins.txt, so the generator does not parse MO2's own files
 * (ModOrganizer.ini, modlist.txt are never read). */

//! True when MO2's hook DLL is in this process (usvfs_x64.dll). Always false off Windows.
bool lodgenUnderMo2();
//! The profile's plugins.txt MO2 hands the process: %LOCALAPPDATA%\Fallout4\plugins.txt.
QString lodgenPluginsTxtPath();
/*! The ENABLED plugins of a plugins.txt, in load order: a line is enabled when
 *  it starts with '*'. '#' comments, blank lines and unstarred (disabled) lines
 *  are dropped. Names only - resolve them against a Data folder. */
QStringList lodgenReadPluginsTxt( const QString & path, QString * error );
/*! The archives a plugin brings: <name> - Main.ba2 and <name> - Textures.ba2 for
 *  Name.esp/.esm/.esl, whichever exist in dataDir. */
QStringList lodgenPluginArchives( const QString & dataDir, const QString & pluginName );
/*! The stack MO2 mode builds, in LAST-WINS order: the Data folder (its loose
 *  files win outright through pass one, its unclaimed archives sink to the
 *  bottom through pass two), then the base game's archives, then each enabled
 *  plugin's archives in LOAD order, so the last plugin's win. */
QStringList lodgenMo2Stack( const QString & dataDir, const QStringList & pluginNames );

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
	/*! Adaptive water subdivision: the deepest quadtree level a wet cell
	 *  may refine to, densest where the water meets land. 0 reproduces
	 *  vanilla exactly -- ONE quad per wet cell, four corner vertices
	 *  4096 units apart, which is why vanilla LOD water can describe no
	 *  shoreline at all. 3 puts a 512-unit quad on the waterline.
	 *
	 *  Near ring only (the dim-4 segmented shape). Far rings are merged
	 *  and distant, so vertices spent there buy nothing.
	 */
	int waterSubdiv = 3;
	/*! Per-vertex water channels, in vertex COLORS on the water shape.
	 *
	 *  Only meaningful with subdivision on: at waterSubdiv 0 the mesh is
	 *  one quad per wet cell and four corner values 4096 units apart can
	 *  describe no shoreline, so the channels stay off there and the
	 *  output remains byte-identical to vanilla.
	 *
	 *  R = depth (water height minus terrain), G = distance to land.
	 *  B and A are free. The mesh is welded and T-junction-free, which is
	 *  what lets a channel cross every edge without seaming.
	 */
	bool waterChannels = true;
	/*! Drop water leaves that lie entirely under terrain.
	 *
	 *  Vanilla culls water per CELL, so a 4096-unit cell with a hill in it
	 *  still gets a full quad and the buried part is drawn, blended and
	 *  then depth-rejected. Subdivision makes the leaves small enough to
	 *  resolve that. Conservative by construction: a leaf is dropped only
	 *  when EVERY terrain sample under it stands above the water plane by
	 *  the margin, so it cannot eat water that is actually visible. */
	bool waterCullBuried = true;
	/*! Denser terrain geometry along shorelines.
	 *
	 *  meshopt_simplify minimises HEIGHT error, and a shoreline is flat -- so
	 *  collapsing coastline vertices costs it almost nothing and they go
	 *  first, while the budget is spent on inland ridges nobody looks at. The
	 *  waterline is the one silhouette in a LOD chunk the eye tracks, and it
	 *  was the cheapest thing in the mesh to delete.
	 *
	 *  This makes collapses near the waterline expensive instead. It is a
	 *  weight, not a lock: nothing is forbidden, so the simplifier still
	 *  converges -- density just buys the coast more of the budget, and the
	 *  triangle count rises with it.
	 */
	bool shoreDenser = false;
	/*! Shore density, 1..10. Scales the weight.
	 *
	 *  1 is the default because it is STRICTLY better than off, measured on the
	 *  harbour chunk: 2160 triangles against off's 2267 -- fewer -- while
	 *  waterline vertices rise 304 -> 371 (25.7% -> 33.7%) and sliver triangles
	 *  fall 10.7% -> 6.1%. Attribute-aware simplification also lands nearer the
	 *  target than the plain call does.
	 *
	 *  Above 1 the waterline SHARE plateaus around 34% while the triangle count
	 *  climbs steeply (density 3 = 3498, density 10 = 8633), so higher settings
	 *  buy overall density rather than a better-concentrated coast. Raise it
	 *  only where the coastline is the point and the budget is not.
	 */
	int shoreDensity = 1;
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
	bool terrainIdentity = true;
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
	/*! Drop object geometry the TERRAIN hides, which is what vanilla's generator
	 *  does -- measured on Sanctuary (-20,24): of our
	 *  rock vertices, the ones vanilla does NOT have are 97.7% below ground,
	 *  median 424 units down. Two rails, both required:
	 *
	 *    * a triangle goes only when ALL THREE of its vertices are below the
	 *      surface by cullMargin, so the shell that crosses the ground is never
	 *      opened and no gap can appear at the terrain line;
	 *    * a PLACEMENT that would lose every triangle keeps all of them. A flat
	 *      tree card sitting under the ground would otherwise vanish outright,
	 *      and a missing object is a worse artefact than a buried one.
	 */
	bool cullBuried = false;
	/*! DEBUG VIEW, not a shipping profile: write the baked AO into R, G and B
	 *  so the channel can be looked at on its own, and multiplied over the
	 *  textured and lit geometry the way the engine would. With the identity
	 *  index in R+G the AO is invisible under it -- the object colour dominates
	 *  and every render is a study of the index instead. The manifest is still
	 *  written, but the identity channel this produces is NOT decodable.
	 */
	bool aoGrey = false;
	/*! Cells of neighbouring terrain and objects to bake AO against, beyond the
	 *  chunk's own edge. 0 reproduces the old behaviour, where both occluders
	 *  stopped dead at the border and edge objects came out too open -- measured
	 *  on Sanctuary (-20,24): mean AO 229 within 150 units of the edge against
	 *  196 in the interior, ~17% too bright, and the neighbouring chunk is too
	 *  bright in the same way, so it reads as a seam along every boundary.
	 *
	 *  Skirt geometry occludes but is never emitted: only this chunk's own
	 *  vertices get a colour written.
	 */
	int aoSkirtCells = 1;
	/*! Per-vertex tree sway weight in the identity profile's vertex ALPHA:
	 *  0 at the trunk base, 1 at the branch tips.
	 *
	 *  Alpha, not UV2 -- UV2.x carries sky visibility. Alpha looked unusable
	 *  at first because it multiplies into the FO4 discard test and the
	 *  branch cards are the alpha-TESTED geometry, which would eat the leaves
	 *  from the inside out. It is usable because the engine only consults
	 *  vertex alpha when SLSF1_Vertex_Alpha is set, and that flag is CLEAR on
	 *  every LOD shape in vanilla and in ours (measured 0x80400001). The byte
	 *  is inert to stock FO4, and costs no stride at all.
	 *
	 *  If a generator option ever sets that flag, sway moves to UV2.y.
	 */
	bool treeSway = true;
	/*! Per-vertex sky visibility (UV2.x) and ground-contact blend (Eye Data).
	 *
	 *  Both are bake-time-only and per PLACEMENT, which is what earns them a
	 *  vertex slot: no shared tiling texture can say how open the sky is HERE,
	 *  or how far THIS vertex is above the ground it stands on.
	 *
	 *  They widen the identity descriptor from 24 to 32 bytes. That desc already
	 *  owes the stock-engine tolerance gate, so this makes the owed measurement
	 *  bigger rather than adding a new kind of risk -- but it does make it
	 *  bigger, which is why it is a switch.
	 */
	bool objectChannels = true;
	/*! World units below the surface before a vertex counts as under it.
	 *  Only selects CANDIDATES -- a candidate still has to be invisible from
	 *  every one of nine directions before it goes, so this is no longer the
	 *  thing standing between the cull and a hole.
	 */
	float cullMargin = 128.0f;
	/*! Divisor on the impostor sheets that are NOT the base colour: 1 keeps
	 *  them at the frame's size, 2 halves each side.
	 *
	 *  The base colour never divides. It carries the coverage in alpha, so it
	 *  IS the silhouette, and a soft silhouette is the one fault an impostor
	 *  cannot hide. The normal, mask and emissive sheets are lit-appearance
	 *  data at LOD distance and take the halving for 54% off a card's bytes
	 *  (3.5 bytes a sheet texel becomes 1.625).
	 */
	int cardAuxDiv = 1;
	int lodLevel = -1;          //!< MNAM slot; -1 = pick by dim (4->0, 8->1, 16->2, 32->3)
	/*! Substitute the nearest filled MNAM slot when the requested one is
	 * empty. OFF matches vanilla, where an empty slot drops the object at
	 * that ring (measured: vanilla dim16 chunks are ~3% of the
	 * always-substitute vertex count). Impostor cards are the better fix. */
	bool slotFallback = false;
	/*! Impostor card library: a directory of <formid8hex>_front.png /
	 * _side.png / <formid8hex>.txt baked by the WW_IMPOSTOR_BAKE hook
	 * (tools/bake_impostor_cards.sh drives it). When the requested MNAM
	 * slot is EMPTY, two crossed card quads substitute instead of falling
	 * back to a nearer (heavier) slot; the card DDS (BC1 punch-through
	 * alpha) is written beside the PNGs and referenced as
	 * Data\Textures\Lodgen\Cards\<id>.DDS — ship that directory there. */
	QString impostorDir;
	/*! From this MNAM level on (0 = dim 4), a placement whose base has a card
	 * stands on the card even where the ring's slot has a mesh: one quad per
	 * tree at the near rings, for a consumer that draws the octahedral
	 * sheets. -1 = cards only where a slot is empty (the default). The stock
	 * engine sees the crossed quads there, so the panel offers it under the
	 * FO4CS target only. bungo, 2026-09-06: "some of the closer ones could be
	 * replaced with a higher res LOD impostor for performance gain". */
	int impostorFromLevel = -1;
};

//! The chunk builder's tree test on a model path: under a `trees` folder or a
//! file named `tree...`. FO4 has no class field; TREE records count too.
bool lodgenIsTreeModel( const QString & model );

/*! A model's world extent, for the impostor baker's size ladder.
 *
 *  `halfW` is the largest radius about the vertical axis through the model's
 *  centroid - what the bake's own widest silhouette converges to, since it
 *  photographs from every azimuth - and `halfH` is half the Z span. The card
 *  baker compares `max(halfW, halfH)` against the run's largest base to pick
 *  this base's rung: half the size, half the frame.
 *
 *  Reads through the resource stack, so a mesh inside a .ba2 measures too.
 *  False when the model does not load or holds no vertices. */
bool lodgenModelExtent( const QString & dataRoot, const QString & meshPath,
	float * halfW, float * halfH );

bool lodgenBuildObjectChunk( NifModel * nif, const EsmWorld & world,
	int chunkX, int chunkY, const LodgenObjectOptions & opts,
	QString * manifestOut, QString * error );

/* Rung 3: bake a chunk's terrain textures from the LAND splat — evaluate
 * the CK paint (per-quadrant LTEX palette + 17x17 opacities) with the
 * source landscape textures world-tiled, plus a model-space normal map
 * from the heightfield. Uncompressed BGRA DDS (BC1 later). */
//! Bake the worldspace height map FO4CS casts terrain shadows from.
//! Writes <outDir>/Textures/Terrain/<EDID>/<EDID>.HeightMap.S.W.N.E.B.T.dds,
//! R16_UNORM, no mips, pixel = height/8 + 32767.
bool lodgenBakeHeightmap( const EsmWorld & world, const QString & outDir,
	int resolution, QString * outPath, QString * error );


/*! Ground cover and the grass tint — docs/LODGEN_TERRAIN_VT.md §2 and §3.
 *
 *  OFF by default, and off means byte-identical: with `cover` false the GRAS
 *  chain is never walked, the tint lerp is never entered and the terrain data
 *  sheet is written BC1 with alpha 0xFF, exactly as before. That is a gated
 *  claim (tests/spells/lodgen_ground_cover.sh C1/C2), not an assertion.
 *
 *  `coverFull` is the fixed normalisation constant the cover byte is measured
 *  against: 96 is the largest Density an artist authored in the shipped corpus.
 *  It is fixed, never derived from the run, because a run-derived constant
 *  makes two chunks baked in different runs incomparable — which is the whole
 *  point of a streaming format. */
struct LodgenCoverOptions
{
	bool cover = false;
	float tintStrength = 0.35f;     //!< 0 keeps the albedo byte-identical
	float coverFull = 96.0f;
	QString dumpCoverPath;          //!< raw 512^2 u8 plane, north-up, headerless
};

/*! Caches shared by every bake unit of one pass: the landscape texture LRU,
 *  the LTEX/GRAS/tint answers and the per-chunk terrain channels. Opaque —
 *  the texture type is internal to lodgen.cpp. A null pointer is legal
 *  everywhere and means "own a cache for this call", which is what the
 *  per-chunk path did before the pyramid existed. */
struct LodgenBakeCaches;

//! textureBudgetBytes bounds the texture LRU; 512 MiB is ~24 landscape
//! diffuses at 2048^2 with mips, and the pyramid's 9,216 tiles would
//! otherwise hold every one of ~60 distinct diffuses at once (1.25 GiB).
LodgenBakeCaches * lodgenCreateBakeCaches( qint64 textureBudgetBytes = qint64( 512 ) << 20 );
void lodgenDestroyBakeCaches( LodgenBakeCaches * caches );
//! Census counters the caches accumulate, for the bake's self-accusing line.
void lodgenBakeCacheCounts( const LodgenBakeCaches * caches, int * nifReads, int * texLoads );

/*! The terrain virtual texture (docs/LODGEN_TERRAIN_VT.md).
 *
 *  A pyramid of bordered tiles, one container per level under Data/Terrain,
 *  indexed by a `terrainVT` .lodm. Off by default and off changes nothing: the
 *  pass is not entered and no file is written.
 *
 *  FOUR sheets, not three: colour, model-space normal, data (AO, wetness,
 *  shore, cover) and HEIGHT. The height sheet is R16 with the shadow
 *  heightmap's own encoding, on the same tile grid with the same border and
 *  built the same way, so a consumer that wants nested grids has the geometry
 *  side of the pyramid too and does not have to go back to the whole-worldspace
 *  heightmap for it. Nothing camera-relative, toroidal or morph-banded is baked
 *  -- those are runtime concerns and would make the files useless to the
 *  per-chunk consumer that comes first. */
struct LodgenVtOptions
{
	int finestDim = 2;              //!< cells per tile at the finest level, 1 or 2
	int content = 256;
	int border = 8;
	int mips = 2;
	int compression = 0;            //!< 0 raw, 1 zlib
	/*! The fourth sheet, R16 height, one per tile. OFF by default: it is
	 *  uncompressed where the other three are BC1, so it is +133% on a tile,
	 *  and for a Fallout 4 source it is interpolation rather than measurement -
	 *  the finest default level is 32 world units a texel against LAND's own
	 *  128. The worldspace heightmap already carries every real height at
	 *  source resolution in 75.5 MB and a geometry clipmap can mip that at
	 *  load. Worth turning on for a source with finer terrain than Fallout 4's,
	 *  which is what a Fallout 76 port brings (128 samples a cell, not 32). */
	bool height = false;
	LodgenCoverOptions cover;
	//! When set, the .btr chunk sheets for `btrDims` are ASSEMBLED from the
	//! pyramid's own staging as it is built, rather than baked again.
	QString btrTexDir;
	QVector<int> btrDims;
	/*! A cell rectangle to bake instead of the whole worldspace, inclusive,
	 *  x0 y0 x1 y1. The pyramid is per WORLDSPACE by construction, so a region
	 *  bake writes the region as the container's world rectangle and the index
	 *  says `partial: true`. It exists for the harness and for looking at one
	 *  valley; a consumer must not treat a partial set as a worldspace's. */
	bool haveRegion = false;
	int region[4] = { 0, 0, 0, 0 };
	/*! Called once per finest-level tile row; return false to cancel. A
	 *  whole-worldspace pass is minutes long, so a GUI caller repaints and
	 *  honours its own Cancel here rather than freezing its window. */
	bool ( *progress )( void * user, int rowsDone, int rowsTotal ) = nullptr;
	void * progressUser = nullptr;
};

struct LodgenVtEstimateOut
{
	int levels = 0;
	int coarsestDim = 0;
	bool shortened = false;         //!< the worldspace is not aligned to dim 32
	qint64 tiles = 0;
	qint64 pyramidBytes = 0;
	qint64 btrBytes = 0;
	qint64 deliveredBytes = 0;
	int levelDims[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
	qint64 levelTiles[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
};

//! What a bake would cost, without baking: tiles, delivered bytes and the
//! ladder. One estimator, shared by the panel's summary line, by
//! --vt-estimate, and by --vt itself before it does any work.
bool lodgenVtEstimate( const EsmWorld & world, const LodgenVtOptions & opts,
	bool alsoBtr, LodgenVtEstimateOut * out );

//! The same estimator from a cell rectangle alone, for a caller that has the
//! worldspace's bounds but no loaded plugin - the panel, which must answer on
//! every toggle and cannot spend a 24-second parse doing it.
bool lodgenVtEstimateBounds( int worldWest, int worldSouth, int worldEast, int worldNorth,
	const LodgenVtOptions & opts, bool alsoBtr, LodgenVtEstimateOut * out );

bool lodgenBakeTerrainVt( const EsmWorld & world, const QString & dataRoot,
	const QString & outDir, const LodgenVtOptions & opts, LodgenBakeCaches * caches,
	QString * report, QString * error );

bool lodgenBakeTerrainTextures( const EsmWorld & world, int chunkX, int chunkY,
	int dim, const QString & dataRoot, const QString & outDir,
	const LodgenCoverOptions & coverOpts, LodgenBakeCaches * caches, QString * error );

/* Object atlas pass (vanilla-style: one 4096x2048 sheet per worldspace).
 * Post-processes generated .bto files: shapes whose UVs sit inside [0,1]
 * move onto 256x256 atlas cells (diffuse + matching normal sheet, 2-texel
 * inset against mip bleed) and their texture sets are repointed at
 * atlasGameBase (+".DDS"/"_n.DDS"); tiling shapes keep their source
 * textures. Writes atlasFileBase(.DDS/_n.DDS) and rewrites the files.
 *
 * REQUIRED for stock installs: the source LOD textures are CK-only
 * resources, absent from every shipped BA2 (docs/LODGEN_PARITY.md). The
 * textures tiling shapes keep referencing are therefore COPIED loose from
 * dataRoot into looseRoot (a Data folder) under their game-relative paths,
 * so the output is self-contained; pass an empty looseRoot to skip.
 *
 * bc1 writes the DIFFUSE sheet as BC1 (DXT1) with one-bit alpha for the
 * cut-outs instead of BC3, which is what vanilla's own sheet is: measured,
 * `Commonwealth.Objects.DDS` is 4096x2048 DXT1, 13 mips, 5,592,552 bytes.
 * Half the memory for a sheet whose alpha is only ever a cut-out mask, so it
 * is the stock target's default; FO4CS keeps BC3 for its eight-bit alpha.
 * The normal sheet stays BC3 and `_s` stays BC5. */
bool lodgenBuildAtlas( const QStringList & btoPaths, const QString & dataRoot,
	const QString & atlasFileBase, const QString & atlasGameBase,
	const QString & looseRoot, bool bc1, QString * error );

/* Texture arrays for FO4CS: the three textures of docs/LODGEN_IMPOSTOR_SPEC.md
 * as DX10 BC3 arrays, one set per texture size class and FAMILY (legacy,
 * vanilla-sourced: `<base>.<WxH>_d/_n/_gsaos`; pbr, from a source .lodm:
 * `<base>PBR.<WxH>_bc/_n/_rmaos`), a `.lodm` beside every set, the layer in
 * UV2.y of every vertex, an `A <shape block> <layer> <lodm>` line per shape in
 * each chunk's manifest (the chunk's `M` lines name the source materials), and
 * a sidecar arrayFileBase.txt listing every layer. Run BEFORE the atlas, which
 * repoints diffuse paths; the stock engine reads none of it. */
bool lodgenBuildTextureArrays( const QStringList & btoPaths, const QString & dataRoot,
	const QString & arrayFileBase, const QString & arrayGameBase, QString * report, QString * error );

/* Merge each chunk's shapes down to one per material the engine can tell
 * apart (name, the ten texture slots, alpha, shader flags and constants,
 * vertex descriptor, and the array set its `A` line names), AFTER the arrays
 * and the atlas: vanilla chunks hold three shapes, ours held ten before this.
 * Vertices and triangles concatenate per segment, bounds take the union, the
 * merged-away branches go, and the manifest's `A` (layer -1 = per vertex in
 * UV2.y) and `M` lines are rewritten for the surviving blocks. */
bool lodgenMergeChunkShapes( const QStringList & btoPaths, QString * report, QString * error );

/*! Far-ring proxy simplification: how much of a chunk's geometry survives at
 *  each ring.  Every engine since 2017 replaces a far cluster with one
 *  simplified mesh; ours simplifies the MERGED shape in place, which is the
 *  same thing once the merge has already made one shape per material.
 *
 *  A ratio of 1 leaves the ring untouched.  Ring 0 (dim 4) is what the player
 *  walks up to and is never simplified at all, whatever is set here -- the
 *  byte-identity gate for the near chunk depends on that.
 */
struct LodgenSimplifyOptions
{
	bool enabled = true;
	float ratio8 = 1.0f;        //!< ring 1 (dim 8): off by default
	float ratio16 = 0.35f;      //!< ring 2 (dim 16)
	float ratio32 = 0.20f;      //!< ring 3 (dim 32)
	/*! Tolerated deviation in WORLD units at ring 0, scaled by the ring's dim
	 *  (ring 2 tolerates 4x it, ring 3 8x): the simplifier stops early rather
	 *  than exceed it, so the ratio is a target and this is the rail.  Note
	 *  that a chunk shape's vertices are miniatures divided by the ring's dim,
	 *  so a bound that grows with the ring is a CONSTANT in the file's own
	 *  units -- which is the point: the same on-screen error at every ring. */
	float errorWorld = 128.0f;   //!< 128 is the measured knee: 32 left the rail binding
	                             //!< before topology did (ring 2: 0.912 achieved against
	                             //!< 0.848 at 128), and 512 and 2048 buy 0.003 more.
	//! A group of triangles this small keeps every one of them.
	int minTris = 8;
};

//! The ratio for one ring; 1 (untouched) for ring 0 and anything unknown.
float lodgenSimplifyRatio( const LodgenSimplifyOptions & opts, int dim );

/*! Simplify each far chunk's merged shapes, AFTER the merge.
 *
 *  Per shape, triangles are grouped by (object identity index, texture-array
 *  layer) and each group is simplified on its own, so a collapse can never
 *  weld two objects together, never interpolates the identity index in the
 *  vertex colours (the surviving vertices are a SUBSET of the originals --
 *  meshoptimizer creates no new ones), and never crosses an array layer.
 *  Every other channel of docs/LODGEN_VERTEX_PACKING.md rides along as a
 *  weighted attribute so the metric keeps it meaningful: normal, UV, sky
 *  visibility (UV2.x), baked AO (colour B), sway (colour A) and the
 *  ground-contact blend (Eye Data).
 *
 *  Shapes with an alpha property are left ALONE -- a cut-out card's four
 *  vertices cannot lose one -- and so is any group whose object index appears
 *  on a `C` (impostor card) manifest line.  Segments are regrouped after the
 *  cut by the cell of each triangle's CENTROID, bounds and multi-bounds are
 *  recomputed, and the manifest's rows are untouched. */
bool lodgenSimplifyFarRings( const QStringList & btoPaths, const LodgenSimplifyOptions & opts,
	QString * report, QString * error );

/* Card sheet arrays: every octahedral card set the chunks' C lines stand on,
 * packed by family and sheet size into DX10 BC3 arrays (`<base>.<family>.<WxH>
 * _d/_n/_gsaos` or `_bc/_n/_rmaos`) with a `cardArray` .lodm beside them
 * carrying the grid, the frame and every layer's card geometry; each C line
 * gains `<array lodm> <layer>`. The per-card sets stay beside the cards. */
bool lodgenBuildCardArrays( const QStringList & btoPaths, const QString & cardDir,
	const QString & arrayFileBase, const QString & arrayGameBase, int auxDiv,
	QString * report, QString * error );

#endif // LODGEN_H
