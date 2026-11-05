/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef LODGEN_H
#define LODGEN_H

#include "lodgenaggregate.h"
#include "lodbfile.h"       // LodbPlugin / LodbResource: the record's v2 rows

#include <functional>
#include <QHash>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>
#include <vector>

class NifModel;
class EsmWorld;

// THE RESOURCE STACK:
// An ordered list of entries, each a FOLDER (its loose files, and the archives sitting in it) or an ARCHIVE
// FILE (.ba2/.bsa).

//! Install the stack (empty clears it). Process-wide; the index is rebuilt lazily.
void lodgenSetResources( const QStringList & entries );
//! The stack as it was set, in LAST-WINS order.
QStringList lodgenResources();
//! the active plugins in load order (paths or file names).
void lodgenSetPluginOrder( const QStringList & plugins );
//! The stack flattened into the order an index wants it, FIRST-WINS: pass one's loose data sub-directories,
//! then pass two's archives.
QStringList lodgenResourceSearchPaths();
//! Where a relative asset path actually resolves from, for `lodgen --probe` and its harness: `entry` is the
//! stack entry that supplied it (empty when it came from dataRoot or the game manager), `kind` is "loose" or
//! "archive", `path` is the file on disk for a loose hit.
bool lodgenProbeAsset( const QString & dataRoot, const QString & relPath,
	QString * entry, QString * kind, QString * path, QByteArray * bytes );
/*! shapes the model loader left out as editor markers this process: add 1 / 2 counts one
 *  (named EditorMarker* / the word inside the name, e.g. VisibilityEditorMarker); -1 / -2 read the totals. */
int lodgenEditorMarkerCount( int add, const QString & model = QString() );
//! the models (data-relative, lower case, backslashes) that lost a shape to the inside-name rule
QStringList lodgenEditorMarkerInsideModels();
//! what the native loader's per-thread model caches took in (WW_LODGEN_MEMLOG).
QString lodgenNativeCacheMemLine();

/*! did this model's parse (lodgenLoadModel, this process) find a particle system? The cell view
 *  copies those systems into its document (the welded load takes triangle shapes only). */
bool lodgenModelHasParticles( const QString & model );
//! did this model's parse find a looping sequence or a controller no sequence drives (it moves alone)?
bool lodgenModelMoves( const QString & model );
//! a model's file bytes, found the way the model loader finds them (loose, then the archives)
bool lodgenReadModelBytes( const QString & dataRoot, const QString & model, QByteArray & bytes );
//! a previs file (`vis/<plugin>/<form>.uvd`), loose or from the archives
bool lodgenReadVisFile( const QString & dataRoot, const QString & relPath, QByteArray & bytes );
/*! is this BGEM (a material path as an effect property names it) lit by the placed lights in the
 *  game -- Effect Lighting set and a lighting influence above 0, the rule the welded shapes take?
 *  False when it cannot be read. */
bool lodgenEffectMaterialLit( const QString & dataRoot, const QString & matName );
/*! Up to `limit` of the paths the stack's index holds, sorted. For a harness
 *  that has to name a file inside an archive without guessing one. */
QStringList lodgenListResourceFiles( int limit );

//! Build every "once, on first use" index the generator owns, NOW, on the calling thread: the resource stack's
//! own BA2File, the mesh archive index, and the terrain ring self-test's one-shot.
void lodgenWarmSharedIndices();

// The landscape textures' world-space tiling:
// How many world units one repeat of a landscape diffuse covers.
float lodgenLandTiling();
void lodgenSetLandTiling( float unitsPerRepeat );

// HOW THE LAND TEXTURE IS SAMPLED INSIDE THAT REPEAT:
// `footprint` (the default, and byte-identical to the 2026-09-11 bake) reads ONE texel of the mip whose texel
// equals the bake texel's world footprint, so every 341.3333-unit repeat prints the same ~11-texel picture of
// the texture and the repeat itself becomes a visible pattern.
bool lodgenLandSampleAverage();
void lodgenSetLandSampleAverage( bool average );
float lodgenLandDetail();
void lodgenSetLandDetail( float strength );

// THE STOCHASTIC-PHASE LAND SAMPLE:
// `average` above kills the repeat by killing the grain -- bungo's words on the bake it produced were "it lost
// all the texture to it, now it's only solid color blobs". Neither that nor the repeat ships.
void lodgenLandWarp( float wx, float wy, float * wxOut, float * wyOut );
float lodgenLandWarpAmp();
void lodgenSetLandWarpAmp( float units );        // 0 = off = the rung's bytes
float lodgenLandWarpLattice();
void lodgenSetLandWarpLattice( float units );
int lodgenLandWarpOctaves();
void lodgenSetLandWarpOctaves( int octaves );
float lodgenLandMipBias();
void lodgenSetLandMipBias( float bias );         // 0 = off = the rung's bytes

// THE HISTOGRAM-PRESERVING HEX TILING:
// bungo on TILING3's warp, over cmp_tiling3.png: "the proposal looks pretty good, but maybe it could use some
// improvement".
float lodgenLandHexSize();
void lodgenSetLandHexSize( float units );        // 0 = off = the rung's bytes

// HEIGHT-AWARE LAND BLENDING + LARGE-SCALE VARIATION:
// bungo 2026-09-27: "terrain blending on lods / lod terrain patterning could use an improvement".
bool lodgenLandHeightBlend();
void lodgenSetLandHeightBlend( bool on );        // false = off = the rung's bytes
bool lodgenLandMacro();
void lodgenSetLandMacro( bool on );              // false = off = the rung's bytes

// TERRAIN-GUIDED LAND SAMPLING:
// bungo, after the warp sweep picture: "what is used for the land sample warp?
enum {
	LODGEN_LANDGUIDE_OFF       = 0,
	LODGEN_LANDGUIDE_DRAG      = 1,
	LODGEN_LANDGUIDE_ASPECT    = 2,
	LODGEN_LANDGUIDE_ASPECTHEX = 3,
	LODGEN_LANDGUIDE_SLOPEWARP = 4,
	LODGEN_LANDGUIDE_FLATWARP  = 5
};
int lodgenLandGuideRule();
void lodgenSetLandGuideRule( int rule );         // 0 = off = the rung's bytes
float lodgenLandGuideStrength();
void lodgenSetLandGuideStrength( float k );
float lodgenLandGuideScale();
void lodgenSetLandGuideScale( float units );     // 128..2048, ring-bounded
float lodgenLandGuideSlopeRef();
void lodgenSetLandGuideSlopeRef( float tangent );

/* --- VANILLA FAR-TERRAIN REUSE ---
 *
 *  "so now we do not use our own normal map if that is toggled, but reuse these
 *   ones for terrain chunks."   -- and, on the out-of-bounds ground, "out of
 *  bounds terrain blends are not included in the actual cells out of bounds,
 *  they never were, so we can't recover the color data anymore, because it was
 *  baked in a different tool outside of fo4."
 *
 *  `vanilla` (THE DEFAULT): a chunk with a shipped vanilla `_msn` writes
 *      VANILLA'S FILE, byte for byte, and our normal bake is skipped for it; a
 *      chunk whose cells carry no land paint at all writes vanilla's COLOUR
 *      file byte for byte too, because that colour was baked outside FO4 and is
 *      not recoverable from the ESM; every other chunk keeps our composite and
 *      gains vanilla's fine geology as the crevice term (lodgenLandShade).
 *  `vanilla-blend`: vanilla's fine detail over OUR coarse normal, up
 *      recomputed so the normal stays unit length. For reshaped terrain. Never
 *      the default.
 *  `none`: the rung's bytes, exactly -- no read, no copy, no shading.
 *
 *  The vanilla sheets are read as LOOSE FILES under lodgenVanillaLodRoot() and
 *  never through the resource stack, so the bake cannot "reuse vanilla" by
 *  copying its own previously installed output. */
enum LodgenLandDetailSource
{
	LODGEN_LANDDETAIL_NONE = 0,
	LODGEN_LANDDETAIL_VANILLA = 1,
	LODGEN_LANDDETAIL_VANILLA_BLEND = 2
};

enum LodgenLandDetailSourceErosion
{
	LODGEN_LANDDETAIL_EROSION = 3   //!< the grown pass, vanilla's copy off
};

//! What one chunk's erosion lattice did, so the log can say it rather than the reader having to decode a sheet
//! to find out.
struct LodgenErosionCensus
{
	qint64 cells = 0;         //!< lattice cells built, border included
	qint64 moved = 0;         //!< cells whose height delta is not exactly 0
	double meanAbs = 0.0;     //!< mean |delta| in world units over those cells
	double maxCut = 0.0;      //!< the deepest incision (negative), world units
	double maxFill = 0.0;     //!< the thickest deposit (positive), world units
	double step = 0.0;        //!< the lattice step in world units
	void add( const LodgenErosionCensus & o );
};

/*! THE EROSION PASS. 0 is the default, is off, and is the
 *  previous bake's bytes by construction: at 0 no lattice is built and the two
 *  `_msn` writers never add a term, so nothing is multiplied by zero and nothing
 *  rounds. The strength scales the height delta the droplets leave before its
 *  gradient reaches the sheet. Iterations are droplets PER LATTICE CELL. The
 *  seed feeds the world-position hash that nucleates the channels; changing it
 *  moves the channels and changes the bytes, which is why it is a switch and not
 *  a constant. */
float lodgenErosion();
void lodgenSetErosion( float strength );         // 0 = off = the rung's bytes
int lodgenErosionIterations();
void lodgenSetErosionIterations( int rounds );   // feedback rounds, 1..8
quint32 lodgenErosionSeed();
void lodgenSetErosionSeed( quint32 seed );

//! Pool one lattice into the process-wide erosion census.
void lodgenErosionCensusAdd( const LodgenErosionCensus & o );
//! The pooled erosion census for this run.
LodgenErosionCensus lodgenErosionCensusTotal();
//! Zero it, as the vanilla-reuse counters are zeroed.
void lodgenResetErosionCensus();

int lodgenLandDetailSource();
void lodgenSetLandDetailSource( int mode );      // NONE = off = the rung's bytes
QString lodgenVanillaLodRoot();
void lodgenSetVanillaLodRoot( const QString & root );
//! THE LANDLESS-CELL HEIGHT FILL. OFF by default.
bool lodgenLandFillVanilla();
void lodgenSetLandFillVanilla( bool on );
bool lodgenVanillaCellHeights( const QString & ws, int cx, int cy, float * h33x33 );
/*! the placed statics' far-LOD tops over cells [cx0, cx1] x [cy0, cy1] (plus the object
 *  field's two-cell margin), trees left out, for the .lodl sky plane (LodtOptions::staticTops). `cellWanted`
 *  false = that cell's refs are skipped. The object AO's own height field and loader. */
struct LodtStaticTops;
bool lodgenStaticTops( const EsmWorld & world, const QString & dataRoot, int cx0, int cy0, int cx1, int cy1,
	const std::function<bool( int, int )> & cellWanted, LodtStaticTops & out );
QString lodgenLandFillCensusLine();
float lodgenLandShade();
void lodgenSetLandShade( float kDiv );           // 0 = no crevice term
//! The far-terrain colour GRADE: every baked colour texel is multiplied by this before it is quantised, in both
//! writers.
void lodgenSetLandGrade( float k );              // 1 = off, byte-identical
float lodgenLandGrade();

//! THE SHEET FORMAT:
//! `legacy` is the default and is the previous bake's bytes by construction: the writer is called with exactly
//! the arguments it was called with before.
enum LodgenSheetFormat
{
	LODGEN_SHEETFMT_LEGACY = 0,   //!< BC1, 8 mips -- the rung's bytes
	LODGEN_SHEETFMT_VANILLA = 1   //!< BC3, mips to 1x1, alpha 255
};

int lodgenSheetFormat();
void lodgenSetSheetFormat( int fmt );            // LEGACY = off = the rung's bytes

//! THE CLEANED `_msn` CACHE:
//! A directory of `<ws>.<dim>.<x>.<y>.png` sheets. Empty is the default and is off: no directory is read and no
//! chunk changes.
QString lodgenMsnCacheDir();
void lodgenSetMsnCacheDir( const QString & dir );  // empty = off = the rung's bytes; "auto" = the resource stack's upscaled set
bool lodgenMsnCacheAuto();  // the setting is "auto" (lodgenMsnCacheDir() then names what it found, or nothing)

//! The chunk census the ruling asks to be stated, counted on the bake itself.
struct LodgenVanillaReuse
{
	int msnCopied = 0;            //!< chunks whose `_msn` IS vanilla's file
	int msnOurs = 0;              //!< chunks that kept our normal bake
	int colCopied = 0;            //!< layerless chunks whose colour IS vanilla's file
	int colOurs = 0;              //!< chunks that kept our colour composite
	int layered = 0;              //!< chunks with land paint on at least one cell
	int layerless = 0;            //!< chunks with no paint at all (out of bounds)
	int layerlessNoVanilla = 0;   //!< ...and no vanilla colour sheet either
	int shaded = 0;               //!< chunks that took the crevice term
	int sheetFormat = 0;          //!< LodgenSheetFormat in force for this run
	int msnCacheHit = 0;          //!< chunks whose `_msn` came from the cache
	int msnCacheMiss = 0;         //!< chunks with a cache dir set and no file
	int msnCacheRenorm = 0;       //!< cached sheets with texels off the unit circle
};

LodgenVanillaReuse lodgenVanillaReuseCensus();
void lodgenResetVanillaReuseCensus();

QString lodgenVanillaSheetPath( const QString & ws, int dim, int chunkX, int chunkY,
	const QString & suffix );
bool lodgenReadVanillaSheet( const QString & ws, int dim, int chunkX, int chunkY,
	const QString & suffix, QByteArray & out );
bool lodgenChunkHasLandPaint( const EsmWorld & world, int chunkX, int chunkY, int dim );
void lodgenVanillaChunkSheets( const EsmWorld & world, const QString & ws, int dim,
	int chunkX, int chunkY, int res, bool hasPaint,
	std::vector<quint32> & col, std::vector<quint32> & nrm,
	QByteArray & msnCopy, QByteArray & colCopy );
bool lodgenWriteChunkSheets( const QString & base, int res,
	const std::vector<quint32> & col, const std::vector<quint32> & nrm,
	const QByteArray & colCopy, const QByteArray & msnCopy );

// THE QUADRANT BORDER:
// Nothing blends across a landscape quadrant: the base texture and the whole layer SET change at every
// 2,048-unit line (64 texels at dim 4), and the 17x17 opacities are bilinear only WITHIN their own quadrant.
int lodgenBlendEdges();               // 0 = off, 1 = quadrant cross-fade
void lodgenSetBlendEdges( int mode );
float lodgenBlendMargin();            // world units either side of the border
void lodgenSetBlendMargin( float units );

// Mod Organizer 2 Launched from MO2's executable list a process sees the VIRTUAL Data folder - every enabled
// mod's loose files and archives layered by priority - and the profile's plugins.txt, so the generator does
// not parse MO2's own files (ModOrganizer.ini, modlist.txt are never read).

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
//! The stack MO2 mode builds, in LAST-WINS order: the Data folder (its loose files win outright through pass
//! one, its unclaimed archives sink to the bottom through pass two), then the base game's archives, then each
//! enabled plugin's archives in LOAD order, so the last plugin's win.
QStringList lodgenMo2Stack( const QString & dataDir, const QStringList & pluginNames );

// LODGEN rung 1 (docs/LODGEN_PLAN.md): terrain .btr generation from LAND records.
struct LodgenTerrainOptions
{
	int dim = 4;                //!< chunk edge in cells (4/8/16/32)
	bool water = true;
	//! Adaptive water subdivision: the deepest quadtree level a wet cell may refine to, densest where the water
	//! meets land.
	int waterSubdiv = 3;
	//! Drop water leaves that lie entirely under terrain:
	//! Vanilla culls water per CELL, so a 4096-unit cell with a hill in it still gets a full quad and the
	//! buried part is drawn, blended and then depth-rejected.
	bool waterCullBuried = true;
	//! Denser terrain geometry along shorelines:
	//! meshopt_simplify minimises HEIGHT error, and a shoreline is flat -- so collapsing coastline vertices
	//! costs it almost nothing and they go first, while the budget is spent on inland ridges nobody looks at.
	bool shoreDenser = false;
	//! Shore density, 1..10. Scales the weight.
	int shoreDensity = 1;
	//! Decimation target, triangles per cell AT DIM 4 (vanilla dim4 chunks run ~130/cell).
	int targetTrisPerCell = 130;
	/*! CS profile: store per-vertex WORLD height deltas to the parent ring's
	 * surface in Eye Data, enabling continuous (geomorphed) LOD transitions.
	 * Widens the vertex stride -- gated on the stock-engine tolerance check. */
	bool geomorph = false;
	//! CS terrain profile: add COLORS to the Land desc — R = LTEX material class (0 dirt, 32 grass, 64 forest
	//! floor, 96 rock, 128 road/concrete, 160 sand, 192 marsh/wet, 224 snow), G = flow-accumulation wetness, B
	//! = heightfield AO, A = 255.
	bool terrainIdentity = false;
	//! texture path template; {ws}/{dim}/{x}/{y} are substituted
	QString textureBase = QStringLiteral( "Data\\Textures\\Terrain\\%1\\%1.%2.%3.%4.DDS" );
};

//! Build one terrain chunk into a fresh FO4 document. chunkX/chunkY are the
//! SW corner cell coordinates (dim-aligned, per vanilla file naming).
bool lodgenBuildTerrainChunk( NifModel * nif, const EsmWorld & world,
	int chunkX, int chunkY, const LodgenTerrainOptions & opts, QString * error );

// Rung 2: object .bto stitching.
struct LodgenObjectOptions
{
	int dim = 4;
	QString dataRoot;           //!< Data folder holding meshes\\lod\\... sources
	//! Per-vertex identity channels INSIDE the .BTO (CS profile). OFF since 2026-09-12; `--identity` opts in.
	bool identity = false;
	bool bakeAO = true;         //!< ray-cast per-placement AO into channel B
	//! Drop object geometry the TERRAIN hides, which is what vanilla's generator does -- measured on Sanctuary
	//! (-20,24): of our rock vertices, the ones vanilla does NOT have are 97.7% below ground, median 424 units
	//! down.
	bool cullBuried = false;
	//! DEBUG VIEW, not a shipping profile: write the baked AO into R, G and B so the channel can be looked at
	//! on its own, and multiplied over the textured and lit geometry the way the engine would.
	bool aoGrey = false;
	//! Cells of neighbouring terrain and objects to bake AO against, beyond the chunk's own edge.
	int aoSkirtCells = 1;
	//! Per-vertex tree sway weight in the identity profile's vertex ALPHA: 0 at the trunk base, 1 at the branch
	//! tips.
	bool treeSway = true;
	//! Per-vertex sky visibility (UV2.x) and ground-contact blend (Eye Data).
	bool objectChannels = true;
	//! World units below the surface before a vertex counts as under it.
	float cullMargin = 128.0f;
	//! Divisor on the impostor sheets that are NOT the base colour: 1 keeps them at the frame's size, 2 halves
	//! each side.
	int cardAuxDiv = 1;
	int lodLevel = -1;          //!< MNAM slot; -1 = pick by dim (4->0, 8->1, 16->2, 32->3)
	//! Substitute the nearest filled MNAM slot when the requested one is empty.
	bool slotFallback = false;
	//! Impostor card library: a directory of <formid8hex>_front.png / _side.png / <formid8hex>.txt baked by the
	//! WW_IMPOSTOR_BAKE hook (src/features/far-lod/bake_impostor_cards.sh drives it).
	QString impostorDir;
	//! From this MNAM level on (0 = dim 4), a placement whose base has a card stands on the card even where the
	//! ring's slot has a mesh: one quad per tree at the near rings, for a consumer that draws the octahedral
	//! sheets.
	int impostorFromLevel = -1;
	//! Impostor cards are TREES ONLY. ON, the default: only a base the chunk builder's own tree test calls a
	//! tree may stand on a card.
	bool treesOnly = true;
};

/*! The chunk builder's tree test on a model path: under a `trees` folder, or a
 *  file named `tree...` AND SITTING UNDER `Landscape\`. FO4 has no class field;
 *  TREE records count too.
 *
 *  The `Landscape\` scope on the filename clause comes out of
 *  FLAGSCAN1's census: seven shipped models are named `Tree...` outside the
 *  landscape tree set and every one of them is a `SetDressing\` prop -- two
 *  tree swings, a swing rope pile, a grounded swing, a no-swing swing, a noose
 *  branch and a hanging mannequin -- four of which stand in Sanctuary, where
 *  the bare filename clause called them trees. The clause still earns its
 *  place: 167 `LOD\Landscape\Tree*.nif` far models and two
 *  `Landscape\Plants\Tree*.nif` are named, not foldered, so a leading `meshes`
 *  and a leading `lod` are dropped before the `landscape` component test. */
bool lodgenIsTreeModel( const QString & model );

// AGGREGATE RING-3 IMPOSTORS -- the two calls the compositor needs from this file.
QHash<quint32, LodgenAggCard> lodgenAggregateCards( const EsmWorld & world, const int region[4],
	const QString & cardDir, int auxDiv, QStringList * notes );
//! Write one set's three sheets and its `kind: "aggregate"` `.lodm` under a Data root.
bool lodgenAggregateWrite( const QString & outRoot, const QString & worldspace,
	const LodgenAggSet & set, QStringList * written, QString * error );

//! THE FOUR STAGE TIMES of a bake, as one line, stated ONCE so the panel and the command line cannot word them
//! differently.
QString lodgenStageTimeLine( qint64 msLandscape, qint64 msMeshes, qint64 msTextures, qint64 msImpostors,
	const QString & librarySplit = QString() );
struct NativeSrcShape;
bool lodgenNativeLoadModel( void * user, const QString & model, std::vector<NativeSrcShape> * out );
/*! the level-0 alpha of a diffuse texture as the AO
 *  caster tests it (alpha >= ref blocks a ray), from a process-wide cache that
 *  lives until exit. Null when the texture does not load. Thread-safe. */
const struct LodgenAoAlpha * lodgenAoAlphaMap( const QString & dataRoot, const QString & texPath, quint8 ref );

/*! MATERIAL SWAPS. One MSWP substitution as the
 *  loader applies it: `first` is the ORIGINAL material and `second` the
 *  REPLACEMENT, both as lodgenMaterialSwapKey folds them. */
typedef QVector<QPair<QString, QString>> LodgenMaterialSubst;

//! The one folding both sides of a swap are compared in: lower case, backslashes, everything up to and
//! including the LAST `materials\` cut, so a NIF's `Materials\LOD\X.BGSM`, an MSWP row's `lod\x.bgsm` and a
//! build machine path `c:\...\materials\lod\x.bgsm` all come out `lod\x.bgsm`.
QString lodgenMaterialSwapKey( const QString & material );

//! lodgenNativeLoadModel with a material swap applied: every shape whose material (folded) is some `first` is
//! loaded as if the NIF named `second` (as `Materials\<second>`), and its textures and constants are resolved
//! from that material through the resource stack, exactly as for a NIF's own.
bool lodgenNativeLoadModelSwapped( void * user, const QString & model, const LodgenMaterialSubst & swap,
	std::vector<NativeSrcShape> * out );

/*! the same load, swap optional, with NO cache -- the
 *  parse is dropped on return. The near-library bake visits every full-detail
 *  model of a worldspace and must not hold them all. */
bool lodgenNativeLoadModelOnce( void * user, const QString & model, const LodgenMaterialSubst * swap,
	std::vector<NativeSrcShape> * out );

/*! the load for a PLACED reference, swap optional. The game gives a placed
 *  model's root node the reference's own transform, so the transform the file's root carries is left
 *  out here; every node below the root is composed as before. The bakes keep their own loads. */
bool lodgenNativeLoadModelPlaced( void * user, const QString & model, const LodgenMaterialSubst * swap,
	std::vector<NativeSrcShape> * out );

//! a texture's format and size WITHOUT decoding it, for the near library's texture-array sidecar.
bool lodgenTextureInfo( const QString & dataRoot, const QString & texPath, quint32 * dxgi, quint32 * width,
	quint32 * height, quint32 * mips, QString * source = nullptr, bool * cube = nullptr );

//! A model's world extent, for the impostor baker's size ladder.
bool lodgenModelExtent( const QString & dataRoot, const QString & meshPath,
	float * halfW, float * halfH );

bool lodgenBuildObjectChunk( NifModel * nif, const EsmWorld & world,
	int chunkX, int chunkY, const LodgenObjectOptions & opts,
	QString * manifestOut, QString * error );

//! Rung 3: bake a chunk's terrain textures from the LAND splat — evaluate the CK paint (per-quadrant LTEX
//! palette + 17x17 opacities) with the source landscape textures world-tiled, plus a model-space normal map
//! from the heightfield.
bool lodgenBakeHeightmap( const EsmWorld & world, const QString & outDir,
	int resolution, QString * outPath, QString * error );


//! Ground cover and the grass tint — docs/LODGEN_TERRAIN_VT.md §2 and §3.
struct LodgenCoverOptions
{
	bool cover = false;
	float tintStrength = 0.35f;     //!< 0 keeps the albedo byte-identical
	float coverFull = 96.0f;
	QString dumpCoverPath;          //!< raw 512^2 u8 plane, north-up, headerless
	/*! ROADS AND DECALS IN THE FAR TERRAIN COLOUR (bungo 2026-09-11, verbatim:
	 *  "We do the same with roads and decals as vanilla").
	 *
	 *  ON by default under BOTH targets, because vanilla's own graded bakes
	 *  carry them: measured on Bethesda's shipped
	 *  `Commonwealth.4.-20.20.DDS` against a top-down projection of the placed
	 *  road meshes and got AUC 0.716 on brightness and 0.678 on greyness
	 *  against a displaced floor that never passed 0.601 / 0.513, while trees,
	 *  rocks, architecture, set dressing and generic `bDecal` shapes all stayed
	 *  inside their own floors. `--no-roads` is the exact way back and is
	 *  byte-identical to the bake before this existed.
	 *
	 *  The NORMAL sheet is deliberately NOT touched: the same lane measured
	 *  vanilla's `_msn` against a normal computed from the LAND heightmap alone
	 *  and the road footprint agrees with the bare heightmap BETTER than the
	 *  background does (13.58 deg against 14.14 deg), so vanilla does not put
	 *  the road mesh in the normal and neither do we. */
	bool roads = true;
	//! Ground cover under a road. A road surface is not soil, so the cover byte is scaled by one minus the
	//! road's coverage and the grass tint goes with it -- grass grows BESIDE a road, not through it.
	float roadCoverSuppress = 1.0f;

	//! HOW STRONGLY the road paint is mixed into the ground it lies on the alpha of the road plane is scaled by
	//! this before the composite `colour = ground + ( roadColour - ground ) * a * roadOpacity`.
	float roadOpacity = 1.0f;

	//! HOW road pieces combine where they overlap:
	//! `RoadMaxZ`: a z buffer over the pieces and the topmost triangle OVERWRITES the texel.
	enum RoadComposite { RoadMaxZ = 0, RoadBlend = 1 };
	int roadComposite = RoadMaxZ;

	//! HOW MUCH of the road diffuse's own pattern is printed into the sheet: `c.rgb = lerp(
	//! wholeTextureAverage, footprintSample, roadDetail )`.
	float roadDetail = 1.0f;

	//! THE COVERAGE MULTIPLIER for a road shape whose MATERIAL IS A LANDSCAPE GROUND MATERIAL -- one that lives
	//! under `materials/Landscape/Ground/`, the same folder the landscape's own painted textures come from.
	float roadGroundPaint = 0.0f;

	//! Put the RAISED road families back into the ground sheet. Off, the default, anything under
	//! `Landscape\Roads\HighwayOverpass\` or `...\Bridge\` is refused -- see `lodgenIsRaisedRoadModel`.
	bool roadRaised = false;

	//! Paint `Landscape\Sidewalks\` with the roads. ON IS THE DEFAULT, by bungo's ruling: "I
	//! approve of the roads" and "if that is their in game texture, it is their texture on our terrain too".
	bool roadSidewalks = true;

	//! FLAT GROUND OBJECTS: a placed object that lies flat ON the ground -- a slab, a railway track, a path, a
	//! decal, a debris pile -- is painted into the far terrain's colour beside the roads, with its in-game
	//! diffuse.
	bool flatObjects = true;
	/*! The override file: one line per model or folder, `bake` or `nobake`.
	 *  Empty = `lodgenFlatObjectsDefaultFile()`. Read on every bake, never
	 *  written by one (created, header only, when the default is missing). */
	QString flatObjectsFile;

	/*! THE NORMAL STAMP (bungo: "normal map on the
	 *  ground has no details baked from the objects"). Wherever the colour sheet
	 *  is stamped from a road or a flat object, the object's normal map is
	 *  stamped into the VT `msn` too: sampled at the fragment's own UV, carried
	 *  to world space through the triangle's UV frame and its vertex normal,
	 *  composited by the colour's rule, and folded over the surface normal with
	 *  the colour lerp's own weight (lodgenVtStampMsn). An effect (unlit) shape
	 *  paints colour and no normal. The VT path only; the stock chunk path's
	 *  `_msn` is not stamped.
	 *
	 *  ON by default; `--no-stamp-normals` is the off switch (CLI only), and off
	 *  is the previous bake's bytes (the stamp only reads the colour pass). */
	bool stampNormals = true;

	//! THE FAR TERRAIN RECEIVES AMBIENT OCCLUSION FROM THE PLACED OBJECTS.
	bool terrainObjectAo = false;

	/*! HOW STRONGLY the object occlusion is mixed in -- it scales the object
	 *  march's accumulated occlusion before the visibility form, exactly as the
	 *  1.6 constant beside it does:
	 *
	 *      vis_object = clamp( 1 - occl_object / 8 * 1.6 * strength, 0, 1 )
	 *
	 *  1.0 is the law as written. The DEFAULT is 0.5, and the reason is a
	 *  measurement rather than a taste: at 1.0 the term SATURATES. Measured on
	 *  the Commonwealth region (-24,24)..(-17,31), shipped pyramid level 2,
	 *  1,048,576 content texels, the AO byte (mask sheet B):
	 *
	 *    strength   mean    min   texels moved   mean drop   max drop   at 0
	 *      off     211.55    65        --            --          --       0
	 *      0.15    191.84    49     833,111 79.5%    24.81       83       0
	 *      0.25    178.79    41     851,276 81.2%    40.35      116       0
	 *      0.50    146.40    24     861,873 82.2%    79.27      198       0
	 *      1.00     93.33     0     863,586 82.4%   143.55      255   276,234
	 *
	 *  At 1.0 a quarter of the region's terrain AO is clamped flat to zero, and
	 *  a clamped byte has stopped carrying occlusion at all -- "under a tree"
	 *  and "under a tower" become the same byte. 0.5 is the largest sampled
	 *  strength at which NOTHING clamps anywhere in the region (the darkest
	 *  texel keeps 24 of 255), so it is the largest value that still spends the
	 *  whole channel on a difference. That is the floor under the default; it is
	 *  not a fit to any artefact, because vanilla's far terrain carries no
	 *  object occlusion for a fit to score against.
	 *
	 *  It is ONE region, and a forested one (45,222 of 147,456 lattice squares
	 *  occupied). A bare region will clamp later and a denser one sooner; the
	 *  number was not measured anywhere else. The knob exists for the same
	 *  reason `roadOpacity` does -- so the dial is in bungo's hand rather than
	 *  in a recompile -- and the whole feature is off unless asked for.
	 *
	 *  0 makes the object march return exactly 1.0f and is therefore the rung's
	 *  bytes, the same way the switch being off is. */
	float terrainObjectAoStrength = 0.5f;

	/*! THE SLAB LATTICE. The object march reads the
	 *  lowest object surface over a square as well as the highest, so a deck
	 *  1,000 units up is a CEILING -- sky still reaches the ground under it
	 *  from the sides -- instead of a solid block from the ground to its top.
	 *
	 *  Default = the new law. It is a sub-toggle inside `--terrain-object-ao`
	 *  and NOT a dial: there is nothing to tune. It exists for two reasons and
	 *  both are gates, not taste: it makes "red on the old formula" runnable on
	 *  one exe (tests/features/far-lod/lodgen_slab.sh), and 0 is the exact way back to
	 *  the sheet bytes of the bakes bungo has already looked at (hotfix 7c).
	 *
	 *  0 reproduces the max-Z reading bit for bit -- see
	 *  `lodgenObjectSkyVis`, where the wall branch is the whole of the old
	 *  loop and the ceiling term is never even added. */
	bool terrainObjectAoSlab = true;

	//! THE GROUND'S SKY WITH THE OBJECTS IN IT.
	bool skyObjects = true;

	//! Write the object height lattice itself to a file, so the term can be audited against something other
	//! than its own output.
	QString dumpObjectAoPath;
};

/*! What one bake's object-AO pass did. Written UNCONDITIONALLY while the
 *  feature is on, every field moving with the thing it measures, so a region
 *  that found no occluder says so with zeros instead of going silent. */
struct LodgenObjectAoCensus
{
	int placements = 0;       //!< LOD-bearing references rasterised (SCOL parts singly)
	int meshes = 0;           //!< distinct LOD models actually loaded
	int triangles = 0;        //!< triangles rasterised into the lattice
	int squares = 0;          //!< 128-unit lattice squares that carry an object top
	int slabSquares = 0;      //!< ...of those, the ones whose LOWEST object surface stands more than one cell (128 units) above the ESM terrain under them: the squares the ceiling term exists for. 0 on a region with no elevated deck.
	int refusedNoLod = 0;     //!< references whose base carries no MNAM LOD mesh
	int noLodBases = 0;       //!< distinct bases behind those refusals
	int refusedNoLoad = 0;    //!< LOD models that would not load
	qint64 texels = 0;        //!< content texels the object term actually darkened
	double darkSum = 0.0;     //!< sum of (AO byte before - after) over those texels
	QStringList refusals;     //!< "<reason> <name>", deduplicated, capped at 16
	void addRefusal( const char * why, const QString & name );
	void add( const LodgenObjectAoCensus & o );
	//! Mean darkening in AO bytes over the texels it touched; 0 when it touched none.
	double meanDarkening() const
	{
		return texels > 0 ? darkSum / double( texels ) : 0.0;
	}
};

/*! THE ROAD TEST. Never a bare substring: `MISTAKES.md`'s "sTREEt" lesson has a
 *  live counter-example in Sanctuary itself, where twelve
 *  `SetDressing\RailRoad\WaxCandle02Off.nif` are placed and the letters `road`
 *  sit in every one of their paths. This is component equality:
 *
 *    the model path, separators normalised, lower-cased, a leading `meshes`
 *    component dropped, has `landscape` as its FIRST component and `roads` or
 *    `sidewalks` as its SECOND.
 *
 *  The caller additionally requires the base record's type to be `STAT`; the
 *  two together are the measured rule. */
bool lodgenIsRoadModel( const QString & modelPath );

//! THE RAISED-ROAD TEST, on the model path alone: `landscape` / `roads` / (`highwayoverpass` | `bridge`) / at
//! least one more component.
bool lodgenIsRaisedRoadModel( const QString & modelPath );

//! `Landscape\\Sidewalks\\` -- the pavement and kerb family, component equality like the two tests above (a
//! leading `meshes` dropped, then `landscape` / `sidewalks` / at least one more).
bool lodgenIsSidewalkModel( const QString & modelPath );

/*! where the flat-object override file lives when no flag names
 *  one -- `lodgen_flat_objects.txt` beside the NifSkope executable. */
QString lodgenFlatObjectsDefaultFile();

/*! the override file's effective rules as one digest line (the
 *  settings digest reads it, so editing the file rebakes). Creates the DEFAULT
 *  file, header only, when it is missing; never writes any other path. */
QString lodgenFlatObjectsRulesDigest( const QString & file );

//! What one bake's road pass did, for the census line. Every field is written
//! unconditionally and moves with the thing it measures (the three rules of
//! 2026-09-04 21:33): a region with no roads reads zeros across the row.
struct LodgenRoadCensus
{
	int placements = 0;         //!< road references (SCOL parts counted singly)
	int meshes = 0;             //!< distinct road models actually loaded
	int shapes = 0;             //!< road shapes offered to the scan converter
	int decalShapes = 0;        //!< of those, shapes whose material says bDecal
	int triangles = 0;          //!< triangles offered to the scan converter
	int texels = 0;             //!< texels a road wrote
	int decalTexels = 0;        //!< of those, texels a decal shape wrote
	int alphaRejected = 0;      //!< texels an alpha-tested shape refused
	int refusedNoLoad = 0;      //!< road models that would not load
	int refusedNoTexture = 0;   //!< road shapes whose diffuse would not resolve
	int refusedRaised = 0;      //!< road placements refused as raised (lodgenIsRaisedRoadModel; hasLod no longer refuses, ROADS1)
	int raisedBases = 0;        //!< distinct raised bases behind those refusals
	int blendTexels = 0;        //!< texels a partially transparent road shape composited into
	int refusedSidewalk = 0;    //!< placements refused as `Landscape\Sidewalks\` (roadSidewalks off)
	int sidewalkBases = 0;      //!< distinct sidewalk bases behind those refusals
	int groundShapes = 0;       //!< road shapes whose material is under materials/Landscape/Ground/
	int groundTexels = 0;       //!< texels such a shape wrote (0 when roadGroundPaint is 0)
	int swappedPlacements = 0;  //!< stamped placements drawn with a material swap (XMSP / MODS), ROADS1
	/* the flat ground objects (LodgenCoverOptions::flatObjects). */
	int flatExamined = 0;       //!< non-road STAT placements the flat rule measured
	int flatPainted = 0;        //!< of those, painted (the rule or a `bake` line)
	int flatOverridden = 0;     //!< placements an override line decided
	int flatHasLod = 0;         //!< painted placements whose base carries its own distant LOD
	int flatShapes = 0;         //!< flat shapes offered to the scan converter
	int flatTexels = 0;         //!< texels a flat object wrote last (tile content only)
	int flatDecalTexels = 0;    //!< of those, by a decal / blended / alpha-tested shape
	int flatRefusedNoTexture = 0; //!< flat shapes whose diffuse would not resolve
	/* THE NORMAL STAMP (LodgenCoverOptions::stampNormals). All zero
	 * when the stamp is off. */
	int nrmShapes = 0;          //!< stamped shapes (road + flat) whose normal map read
	int nrmNoMap = 0;           //!< stamped shapes with no readable normal map: their vertex normal is stamped
	int nrmUnlit = 0;           //!< effect (unlit) flat shapes: colour only, no normal
	int nrmFrameAgree = 0;      //!< triangles whose NIF Tangent points along the UV frame's dP/dv
	int nrmFrameFlip = 0;       //!< triangles whose NIF Tangent points against it
	int nrmTexels = 0;          //!< tile-content texels whose msn the stamp rewrote
	QStringList refusals;      //!< "<reason> <name>", deduplicated, capped at 16
	void addRefusal( const char * why, const QString & name );
	void add( const LodgenRoadCensus & o );
	//! ONE physical line, `key=value` tokens, never parsed by field position.
	QString line() const;
};

// THE MASK LAW -- one home for "what roughness and metallic does this material have", shared by the object
// path's gloss composition and the far-terrain mask sheet (CONSTITUTION 10: what is shared lives in the shared
// code).

//! Which rule produced a material's mask channels. The word is written into the
//! census and, per layer, into the terrain index.
enum LodgenMaskRule
{
	LODGEN_MASK_NONE = 0,               //!< no material and no `_s` map: roughness 1.0, metallic 0
	LODGEN_MASK_LEGACY_INVERTED = 1,    //!< a legacy material: roughness = 1 - gloss, metallic 0
	LODGEN_MASK_PBRM = 2                //!< a PBRM: its own roughness and metallic
};

//! The word a census or an index writes for a rule. Never empty.
const char * lodgenMaskRuleName( LodgenMaskRule rule );

//! THE LEGACY GLOSS, as this tree has always composed it: `smoothness` times the `_s` map's GREEN channel
//! (lodgen.cpp's arrays pass), or the normal map's alpha where there is no `_s` map.
float lodgenLegacyGloss( float smoothness, float specGreen );

/*! What a material contributes to a mask sheet. Paths are game paths for
 *  lodgenLoadTexture; the CALLER samples them, because the object path and the
 *  terrain path sample at completely different footprints. */
struct LodgenMaterialMask
{
	LodgenMaskRule rule = LODGEN_MASK_NONE;
	QString roughnessTex;           //!< the PBRM's RMAOS map, or the legacy `_s` map
	int roughnessChannel = 0;       //!< 0 = R (PBRM RMAOS), 1 = G (legacy `_s` gloss)
	bool invertRoughness = false;   //!< legacy only: roughness = 1 - gloss
	float glossScale = 1.0f;        //!< legacy only: gloss = glossScale * map.G
	float roughnessConst = 1.0f;    //!< used when no map loads; 1.0 = fully rough, the honest unknown
	bool haveRoughnessMap = false;
	QString metallicTex;            //!< PBRM only, the SAME RMAOS map
	int metallicChannel = 1;        //!< G
	float metallicConst = 0.0f;     //!< legacy contributes 0, never a guess
	bool haveMetallicMap = false;
	QString emissiveTex;            //!< `_e` (pbr) or `_g` (legacy), when the material names one
	bool haveEmissive = false;
	QString servedBy;               //!< the material file that answered, for the census
};

/*! Resolve one material's mask contribution.
 *
 *  `matName` is a `.bgsm` / `.bgem` / `.pbrm` game path and may be empty;
 *  `specularTex` is the vanilla slot-7 `_s` map the shape or the TXST names and
 *  may be empty; `smoothness` is the shape's or the material's own constant.
 *
 *  WHERE A PBRM IS LOOKED FOR, and why there are two places. A same-name `.pbrm`
 *  beside a `.bgsm` wins, which is the discovery rule the renderer already uses
 *  (`BSShaderLightingProperty::resolvePbrm`), so an asset that previews as PBR
 *  in this application bakes as PBR. But **more than half of Fallout 4's
 *  landscape textures name no material at all** -- their TXST carries TX00 and
 *  TX07 and nothing else -- and without a second place to look, those could
 *  never be given a PBRM by anybody. So when there is no material, `diffuseTex`
 *  supplies the stem, through `lodmSourceCandidate()`'s own convention: the
 *  diffuse path with `textures\` swapped for `materials\` and the extension
 *  replaced. That is the same rule a source `.lodm` is already found by, which
 *  is what keeps ONE convention rather than two.
 *
 *  Never fails: with nothing to read it answers LODGEN_MASK_NONE, roughness
 *  1.0, metallic 0, and says so. */
void lodgenResolveMaterialMask( const QString & dataRoot, const QString & matName,
	const QString & specularTex, float smoothness, LodgenMaterialMask * out,
	const QString & diffuseTex = QString() );

//! Caches shared by every bake unit of one pass: the landscape texture LRU, the LTEX/GRAS/tint answers and the
//! per-chunk terrain channels. Opaque — the texture type is internal to lodgen.cpp.
struct LodgenBakeCaches;

//! textureBudgetBytes bounds the texture LRU; 512 MiB is ~24 landscape
//! diffuses at 2048^2 with mips, and the pyramid's 9,216 tiles would
//! otherwise hold every one of ~60 distinct diffuses at once (1.25 GiB).
//! the budget is in the LRU's units (texels, 8 bytes each); 0 = the default, WW_LODGEN_TEXBUDGET_MB real MB (2048)
LodgenBakeCaches * lodgenCreateBakeCaches( qint64 textureBudgetBytes = 0 );
void lodgenDestroyBakeCaches( LodgenBakeCaches * caches );
//! Census counters the caches accumulate, for the bake's self-accusing line.
void lodgenBakeCacheCounts( const LodgenBakeCaches * caches, int * nifReads, int * texLoads );
//! free the decoded textures (not the counters); a no-op under WW_LODGEN_BAKEMEM_RED=1
void lodgenBakeCachesReleaseTextures( LodgenBakeCaches * caches, const char * phase );
//! set the texture LRU's budget (texel units), returning the old one
qint64 lodgenBakeCachesSetTextureBudget( LodgenBakeCaches * caches, qint64 units );

//! The terrain virtual texture (docs/LODGEN_TERRAIN_VT.md):
//! A pyramid of bordered tiles, one container per level under Data/Terrain, indexed by a `terrainVT` .lodm. Off
//! by default and off changes nothing: the pass is not entered and no file is written.
struct LodgenVtOptions
{
	int finestDim = 2;              //!< cells per tile at the finest level, 1 or 2
	/*! levels finer than this dim are
	 *  STAGED -- the .btr chunk sheets are assembled from them -- but never
	 *  encoded or written. 0 = every level is written (today's bake). */
	int writeFinestDim = 0;
	int content = 256;
	int border = 8;
	int mips = 2;
	int compression = 0;            //!< 0 raw, 1 zlib
	//! The fourth sheet, R16 height, one per tile.
	bool height = false;
	//! THE VANILLA-COLOUR FILL: blend the ground no LAND record paints toward Bethesda's own LOD diffuse for
	//! those cells, tone-matched on the overlap. Read at bake time only.
	bool vanillaFill = false;
	//! THE RULE PAINT OUTSIDE. Needs `vanillaFill`.
	bool outsideRule = false;
	//! Where the ground-cover byte lives:
	//! FALSE -- the mask -- is what ships, because the colour sheet's alpha is the one slot `.lodm` 2.1 defines
	//! as OPACITY and a family-conformant consumer alpha-tests it: ground cover there would punch holes in the
	//! ground wherever grass is thin.
	bool coverInColor = false;
	//! HALF-RESOLUTION AUX SHEETS. The colour sheet stays at the chosen texel density; msn, mask, height and
	//! emissive store half the texels a side (their mip 0 is dropped, descriptor byte 6 says so).
	bool halfAux = false;
	/*! ONE-VALUE SHEETS (bungo: "it doesn't need to
	 *  render 1024 for a whole one color mask"). A tile sheet that is one value
	 *  over every texel of every mip is stored as a 16-byte record and a tile
	 *  flag bit instead of its mips (docs/LODGEN_TERRAIN_VT.md 3.2). ON by
	 *  default: it is a size fix and changes no texel. `--no-collapse-uniform`
	 *  (CLI only, for the byte-identity gate) writes today's bytes. */
	bool collapseUniform = true;
	// THE TERRAIN HORIZON SHEET, `.lodt` role 7, REMOVED 2026-09-19 on bungo's "horizon
	// goes bye bye now, we're back to identity".
	LodgenCoverOptions cover;
	//! When set, the .btr chunk sheets for `btrDims` are ASSEMBLED from the
	//! pyramid's own staging as it is built, rather than baked again.
	QString btrTexDir;
	QVector<int> btrDims;
	//! A cell rectangle to bake instead of the whole worldspace, inclusive, x0 y0 x1 y1.
	bool haveRegion = false;
	int region[4] = { 0, 0, 0, 0 };
	/*! Called once per finest-level tile row; return false to cancel. A
	 *  whole-worldspace pass is minutes long, so a GUI caller repaints and
	 *  honours its own Cancel here rather than freezing its window. */
	bool ( *progress )( void * user, int rowsDone, int rowsTotal ) = nullptr;
	void * progressUser = nullptr;
	/*! resume: the checkpoint file (empty = none) and the key of the inputs it is valid
	 *  for. A rerun with the same key starts after the last saved row (all of them: no row runs). The caller removes it when the bake is done. */
	QString resumeFile;
	QString resumeKey;
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

/*! THE TERRAIN OPTION. Which part of the LOD
 *  terrain's colour is baked and which the consumer builds live:
 *
 *   Hybrid   the pyramid from dim 8 up (64 world units a texel at content
 *            512), baked directly, so VT.2 and VT.4 are never computed; near
 *            and mid distance are the consumer's live splat from the .lodl.
 *   Dynamic  no pyramid at all; the live splat everywhere.
 *
 *  Every option also writes the ground decals (`<ws>.lodd` + `<ws>.lodg`,
 *  io/loddecal.h). Hybrid is the default. */
enum class LodgenTerrainOption : int
{
	// 0 was Full (today's whole pyramid), ditched 2026-09-29 08:19; the numbers stay
	Hybrid = 1,
	Dynamic = 2
};

//! "hybrid", "dynamic".
QString lodgenTerrainOptionName( LodgenTerrainOption o );
//! Parses the two names (any case); false on anything else ("full" included: ditched).
bool lodgenTerrainOptionParse( const QString & s, LodgenTerrainOption * out );
/*! The vanilla-colour fill's blend band (law 2): world units
 *  INSIDE the painted ground over which our colour rises from vanilla's
 *  diffuse (0) to ours (1). One number for the bake and the live preview. */
constexpr float LODGEN_VT_FILL_BAND = 8192.0f;
//! THE ROUNDED OUTLINE. Painted is decided per 2,048-unit quadrant, so the painted/unpainted line is a
//! staircase; law 2's band followed it step for step.
constexpr float LODGEN_OUTLINE_SIGMA = 3072.0f;
constexpr float LODGEN_OUTLINE_RES = 512.0f;
float lodgenOutlineOffset();
//! Quadrants from a point within which an unpainted one can pull w below 1.
int lodgenOutlineReachQuads( float band );
struct LodgenOutlineField
{
	int qx0 = 0, qy0 = 0;      //!< SW quadrant; grid point (i, j) at world ((qx0 * 4 + i + 0.5) * 512, (qy0 * 4 + j + 0.5) * 512)
	int nx = 0, ny = 0;
	std::vector<float> f;      //!< blurred signed distance, world units, row 0 = south
	float maxUnpainted = -1e30f;  //!< census: the largest f at an unpainted grid point (must stay below the offset)
	bool empty() const { return f.empty(); }
	float at( float wx, float wy ) const;
	//! Our paint's weight 0..1 at a world point (1 when the field is empty).
	float weight( float wx, float wy, float band ) const;
};
//! Builds the field over quadrants [qx0, qx1] x [qy0, qy1] (inclusive). The raw distance is capped at
//! band + 2 offset + 3 sigma (w = 1 either side of it), so any window reaching
//! lodgenOutlineWindowQuads( band ) quadrants past what it samples gives the same field as the whole map.
void lodgenOutlineBuild( const std::function<bool( int, int )> & paintedQ, int qx0, int qy0, int qx1, int qy1,
	float band, LodgenOutlineField & out );
int lodgenOutlineWindowQuads( float band );
//! THE RULE PAINT MAP, `<ws>.lodr`. A NEW file; no other format changes.
struct LodgenRuleMap
{
	int cellMinX = 0, cellMinY = 0, cellsX = 0, cellsY = 0, spc = 8;
	float band = 0.0f;
	QVector<quint32> palette;
	std::vector<quint8> a, b, w, g;       //!< nx * ny each
	float gainOf( size_t s ) const { return g.empty() ? 1.0f : float( g[s] ) / 128.0f; }
	int nx() const { return cellsX * spc; }
	int ny() const { return cellsY * spc; }
	float spacing() const { return 4096.0f / float( spc ); }
	float originX() const { return ( float( cellMinX ) * float( spc ) + 0.5f ) * spacing(); }
	float originY() const { return ( float( cellMinY ) * float( spc ) + 0.5f ) * spacing(); }
	//! The bilinear mix at world (wx, wy): up to 8 (palette index, weight) pairs, merged, weights summing to 1
	//! over the samples that exist; the count, 0 where no surrounding sample exists.
	int mixAt( float wx, float wy, quint8 * ids, float * wts, float * gain = nullptr ) const;
};
//! Write `m` to `path`; the byte count, or -1 (and `why`).
qint64 lodgenRuleWrite( const QString & path, const LodgenRuleMap & m, QString * why );
//! Read and check a `.lodr` (magic, sizes, CRC, ids within the palette); false and `why` on any fault.
bool lodgenRuleRead( const QString & path, LodgenRuleMap & m, QString * why );
/*! DYNAMIC's half of the rule paint: build the rule map over the worldspace (or
 *  the region in `opts`) and write `<outDir>/<ws>.lodr`. Needs a vanilla LOD
 *  root. `log` gets the census lines. */
bool lodgenBakeOutsideRule( const EsmWorld & world, const QString & dataRoot,
	const QString & outDir, const LodgenVtOptions & opts, QStringList * log, QString * why );

/*! THE AO MAP (bungo 2026-09-29: "Do what you think will
 *  improve things"). HYBRID and DYNAMIC draw our ground near and mid with the
 *  live splat, which has no shading of its own; the baked levels carried the
 *  sky AO (terrain horizon + the objects' sky union) in the mask sheet's B.
 *  This is that same value, computed by the tile bake itself (AO-only mode),
 *  at 32 units, ONLY over our painted quadrants (a base or any layer; the fade
 *  band lies inside them). Outside, nothing: vanilla's far diffuse already
 *  carries its shading. Part of HYBRID and DYNAMIC, no switch; the live splat
 *  multiplies OUR colour by it before the blend to vanilla.
 *
 *  `<ws>.loda`, little-endian, 64-byte header:
 *    0 "LODA" | 4 u32 version 1 | 8 u32 header 64 | 12 u32 texels per quadrant side (64)
 *    16 u32 quadrant count | 20 i32 qMinX | 24 qMinY | 28 qMaxX | 32 qMaxY
 *    36 u32 payload bytes (zlib) | 40 u32 raw bytes (count x 4096)
 *    44 u32 CRC32 of index + payload | 48..63 zero
 *  then the index, count x (i16 qx, i16 qy) sorted by qy then qx, then ONE
 *  zlib stream of the quadrants' 64 x 64 u8 AO in index order, row 0 = south.
 *  Texel (u, v) of quadrant (qx, qy) is centred at world
 *  ((qx * 64 + u + 0.5) * 32, (qy * 64 + v + 0.5) * 32). */
constexpr int LODGEN_AO_QUAD = 64;
constexpr float LODGEN_AO_UNITS = 32.0f;
struct LodgenAoMap
{
	std::vector<std::pair<qint16, qint16>> quads;   //!< (qx, qy), sorted by qy then qx
	std::vector<quint8> ao;                         //!< quads.size() x 64 x 64
	QHash<qint64, int> index;                       //!< filled by lodgenAoRead / rebuildIndex
	static qint64 key( int qx, int qy ) { return ( qint64( qy ) << 32 ) | quint32( qx ); }
	void rebuildIndex();
	//! The quadrant's 64 x 64 block, or nullptr (not painted: no AO of ours).
	const quint8 * block( int qx, int qy ) const;
};
qint64 lodgenAoWrite( const QString & path, const LodgenAoMap & m, QString * why );
bool lodgenAoRead( const QString & path, LodgenAoMap & m, QString * why );
//! Bake `<outDir>/<ws>.loda` over the worldspace (or opts' region); `log` gets the census.
//! `caches`: the pyramid's object height field is taken from it when gathered over the same rectangle.
bool lodgenBakeAoMap( const EsmWorld & world, const QString & dataRoot,
	const QString & outDir, const LodgenVtOptions & opts, QStringList * log, QString * why,
	LodgenBakeCaches * caches = nullptr );

/*! What the option does to the pyramid's options: Hybrid sets the finest level
 *  to dim 8. Returns a one-line note when it could not (the .btr chunk sheets
 *  are assembled from level 4 and need it), else empty. */
QString lodgenTerrainOptionApply( LodgenTerrainOption o, LodgenVtOptions & opts );

//! THE GROUND DECALS: every road placement and every painted flat object in the region (opts.region when
//! opts.haveRegion, else the worldspace) as one projected decal, one picture set per distinct piece.
bool lodgenBakeDecals( const EsmWorld & world, const QString & dataRoot,
	const QString & outDir, const LodgenVtOptions & opts, LodgenBakeCaches * caches,
	QString * report, QString * error );

//! one LTEX record's diffuse as `side` x `side` texels, 0xAARRGGBB, row 0 = v 0 (the bake's own lookup),
//! resampled from the mip nearest `side`.
bool lodgenLtexPicture( const EsmWorld & world, const QString & dataRoot, LodgenBakeCaches * caches,
	quint32 ltexForm, int side, std::vector<quint32> & rgba, QString * path = nullptr );

bool lodgenBakeTerrainTextures( const EsmWorld & world, int chunkX, int chunkY,
	int dim, const QString & dataRoot, const QString & outDir,
	const LodgenCoverOptions & coverOpts, LodgenBakeCaches * caches, QString * error );

// Object atlas pass (vanilla-style: one 4096x2048 sheet per worldspace).
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

// Merge each chunk's shapes down to one per material the engine can tell apart (name, the ten texture slots,
// alpha, shader flags and constants, vertex descriptor, and the array set its `A` line names), AFTER the
// arrays and the atlas: vanilla chunks hold three shapes, ours held ten before this.
bool lodgenMergeChunkShapes( const QStringList & btoPaths, QString * report, QString * error );

//! Far-ring proxy simplification: how much of a chunk's geometry survives at each ring.
struct LodgenSimplifyOptions
{
	bool enabled = true;
	float ratio8 = 1.0f;        //!< ring 1 (dim 8): off by default
	float ratio16 = 0.35f;      //!< ring 2 (dim 16)
	float ratio32 = 0.20f;      //!< ring 3 (dim 32)
	//! Tolerated deviation in WORLD units at ring 0, scaled by the ring's dim (ring 2 tolerates 4x it, ring 3
	//! 8x): the simplifier stops early rather than exceed it, so the ratio is a target and this is the rail.
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

// THE INPUT LEDGER (.lodb):
// An incremental bake is only worth having if it is BYTE-IDENTICAL to the full bake it replaces, and the only
// way to be sure of that is to record, per chunk, a digest of every input that chunk's outputs actually read.

//! One chunk's row in the ledger.
struct LodgenLedgerEntry
{
	int dim = 4, cx = 0, cy = 0;
	QString inputs;                 //!< sha1 hex over the chunk's inputs, incl. the one-cell ring
	QStringList outFiles;           //!< paths relative to the ledger's directory
	QStringList outDigests;         //!< sha1 hex, parallel to outFiles
};

//! A whole ledger: the run-level identity plus one row per chunk.
struct LodgenLedger
{
	quint32 worldspace = 0;
	QString worldEdid;
	int dim = 4;
	int region[4] = { 0, 0, 0, 0 };
	QString switches;               //!< sha1 hex over the argument vector (see the format doc)
	QString loadOrder;              //!< EsmWorld::loadOrderHash as 16 hex digits
	QVector<LodgenLedgerEntry> chunks;

	/* ---- v2, the bake record ------------------------------------------- */
	QString exeStamp;               //!< WW_EDITION_VERSION+<build rev>, read at write time
	qint64  exeBytes = 0;           //!< the exe's own byte size
	QString bakedUtc;               //!< ISO-8601 UTC -- THE ONE VOLATILE LINE
	bool    fo4csTarget = false;    //!< `--native` was given: the record sits under FO4CSLOD/
	/*! The five staleness hashes exactly as the `.lodo`/`.lodi` headers carry
	 *  them, 16 hex digits each; empty when this bake wrote no native pair. The
	 *  record NEVER disagrees with the pair -- that is a gate. */
	QString loadOrderHashHex, pluginCorpusHashHex, objectCorpusHashHex,
	        modelCorpusHashHex, cardCorpusHashHex;
	QVector<LodbPlugin>   plugins;   //!< one a plugin, IN LOAD ORDER
	QVector<LodbResource> resources; //!< one a folder/archive, in stack order
	QStringList switchTokens;        //!< the argument vector, verbatim, one token a line
	QStringList census;              //!< every census line the bake printed, verbatim
	int     endFiles = 0;            //!< files under the record's own directory tree, READ BACK
	qint64  endBytes = 0;            //!< their total size, READ BACK
	//! THE REGION PRODUCTS: every file under the record's own folder that no chunk row claims -- the
	//! `.lodo`/`.lodi`, the texture and card arrays, the VT levels -- with its sha1, sorted by path.
	QStringList productFiles;        //!< paths relative to the record's folder
	QStringList productDigests;      //!< sha1 hex, parallel to productFiles
};

//! The digest of everything chunk (cx,cy) at `dim` reads:
//! Covers the chunk's own cells AND the ONE-CELL RING around them, because the terrain ring
//! (LODGEN_TERRAIN_RING_CELLS) and the AO skirt (aoSkirtCells) both reach exactly that far and the land-guide
//! macro gradient reaches less.
bool lodgenReadAssetBytes( const QString & dataRoot, const QString & relPath, const char * archiveFolder,
	const char * extension, QByteArray & out );
QString lodgenChunkInputDigest( const EsmWorld & world, int dim, int cx, int cy,
	const QString & dataRoot );

//! sha1 hex of a file's bytes; empty when it cannot be read.
QString lodgenFileDigest( const QString & path );

/*! Write / read the record. v2 is PLAIN TEXT (UTF-8, LF, `key<TAB>fields`); the
 *  reader recognises the v1 binary container by its magic and refuses it by
 *  name rather than mis-parsing it. Implemented in `src/features/far-lod/lodbfile.cpp`. */
bool lodgenWriteLedger( const QString & path, const LodgenLedger & led, QString * error );
bool lodgenReadLedger( const QString & path, LodgenLedger * led, QString * error );

/*! Why an incremental run cannot proceed, or 0 when it can.
 *  The strings are printed verbatim: a refusal that does not say what to do
 *  instead is a bug report addressed to the operator. */
enum LodgenIncrRefusal
{
	LODGEN_INCR_OK = 0,
	LODGEN_INCR_NO_LEDGER,
	LODGEN_INCR_WRONG_SHAPE,
	LODGEN_INCR_SWITCHES,
	LODGEN_INCR_WHOLE_REGION,
	LODGEN_INCR_OUTPUT_GONE
};

#endif // LODGEN_H
