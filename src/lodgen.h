/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef LODGEN_H
#define LODGEN_H

#include "lodgenaggregate.h"
#include "lodbfile.h"       // LodbPlugin / LodbResource: the record's v2 rows

#include <QHash>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>
#include <vector>

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

/*! Build every "once, on first use" index the generator owns, NOW, on the
 *  calling thread: the resource stack's own BA2File, the mesh archive index,
 *  and the terrain ring self-test's one-shot.
 *
 *  First use is the only moment those statics can race, and a chunk fan-out
 *  would otherwise have several workers reach them at once. Idempotent and
 *  cheap on a second call; a no-op where no stack is set. (lane BAKEPERF1) */
void lodgenWarmSharedIndices();

/* --- The landscape textures' world-space tiling ------------------------------
 *
 * How many world units one repeat of a landscape diffuse covers. The engine's
 * own number is 341.3333 = 128 / 0.375, read out of Fallout4.exe 1.10.155:
 * `fLandTextureTilingMult:Landscape` = 1.5f (Setting record at file 0x36E83A8,
 * data slot VA 0x1436E97B0, its single code reference at 0x1403A74C6), and the
 * 17x17 landscape quadrant loop at 0x1403A7620 stores u = column * mult/4 =
 * column * 0.375 per vertex over a 2,048-unit quadrant, i.e. 128 units a
 * vertex step. 6 repeats a quadrant, 12 a cell. (lane SPLAT1, 2026-09-11.)
 *
 * The bake used 2,048 -- exactly 6.0000x too large -- so every landscape
 * texture was stretched six-fold and the far sheet printed a 64x64 image of
 * the ground texture at full contrast, tiled 8x8 across the chunk. All
 * FOURTEEN sampling sites read this one value: colour, mask and emissive, in
 * both the stock chunk bake and the pyramid. The normal sheet (`_msn`) is
 * computed from VHGT and no tiling term reaches it.
 *
 * `--land-tiling 2048` is the exact way back and is byte-identical to the
 * pre-2026-09-11 bake. Another user CAN set fLandTextureTilingMult in an INI,
 * which is why this is a value and not a new hard-coded constant. */
float lodgenLandTiling();
void lodgenSetLandTiling( float unitsPerRepeat );

/* --- HOW THE LAND TEXTURE IS SAMPLED INSIDE THAT REPEAT (lane TILING2) ------
 *
 * `footprint` (the default, and byte-identical to the 2026-09-11 bake) reads
 * ONE texel of the mip whose texel equals the bake texel's world footprint, so
 * every 341.3333-unit repeat prints the same ~11-texel picture of the texture
 * and the repeat itself becomes a visible pattern. MEASURED over 22 shipped
 * dim-4 sheets: no vanilla sheet carries more than 0.264 of 255 at that period
 * and none stands above its own null floor; ours carried 1.037 and 1.261.
 *
 * `average` reads the texture's 1x1 mip -- its exact mean over one whole
 * repeat, since the repeat IS the whole texture -- so no periodic term can
 * reach the sheet at all. What it costs is the texture's own fine detail, and
 * `--land-detail` adds a fraction of it back: the difference between the
 * footprint sample and the average, scaled, which scales the repeat by the
 * same factor. 0 = the pure average, 1 = the footprint bake exactly. */
bool lodgenLandSampleAverage();
void lodgenSetLandSampleAverage( bool average );
float lodgenLandDetail();
void lodgenSetLandDetail( float strength );

/* --- THE STOCHASTIC-PHASE LAND SAMPLE (lane TILING3) ------------------------
 *
 * `average` above kills the repeat by killing the grain -- bungo's words on the
 * bake it produced were "it lost all the texture to it, now it's only solid
 * color blobs". Neither that nor the repeat ships. What vanilla has is the land
 * textures' GRAIN WITHOUT THEIR REPEAT, and the measurements say why that is
 * possible: vanilla's fine colour residual is strongly non-Gaussian (skew
 * +1.007 / +1.123, kurtosis 7.373 / 6.539 against 0 / 3 for noise), i.e. it is
 * TEXTURE, and phase randomisation changes neither a field's power spectrum nor
 * its histogram. So the repeat is a property of the PHASE alone, and can be
 * removed without touching the grain.
 *
 * The mechanism is a smooth deterministic DOMAIN WARP of the world position,
 * applied to the land diffuse lookup and to nothing else:
 *
 *     wx' = wx + A * ox(wx,wy),   wy' = wy + A * oy(wx,wy)
 *
 * with (ox,oy) value noise on a lattice of L world units, smoothstep
 * interpolated, hashed from the integer lattice coordinate. Three properties
 * follow by CONSTRUCTION, not by testing:
 *   * it is a pure function of world position, so there is no seam anywhere --
 *     not at a quadrant line, not at a cell line, not at a chunk edge;
 *   * it depends on nothing per-chunk and on no evaluation order, so the bake is
 *     byte-identical at 1 chunk thread and at 16, and run to run;
 *   * it is a RESAMPLING of the texture, not a filter on it, so the grain's
 *     amplitude spectrum and its histogram survive intact.
 * When A is comparable to one repeat the phase wanders by more than a whole
 * cycle across a sheet and the 10.667-texel spectral peak smears into the
 * broadband.
 *
 * `--land-mip-bias` is the other half. The shipped code samples the mip whose
 * texel matches the BAKE texel's world footprint, which is about three mips
 * coarser than the texture's own texel, and that is where TILING2's factor of
 * eight in fine variance went. A negative bias samples finer and restores the
 * amplitude.
 *
 * OFF IS THE WAY BACK, EXACT: amplitude 0 and bias 0 leave both expressions
 * unevaluated, so the sheet is the 2026-09-11 bake byte for byte.
 *
 * MEASURED (7 shipped dim-4 sheets, SPLAT1's independent offline model; lane
 * TILING3). At A=683 L=1024 one octave with bias -1.00 the worst-sheet repeat
 * falls from 1.968 to 0.340 while the median high-pass SD sits at 4.565 against
 * vanilla's 4.476 (+2%). THE REPEAT GATE IS 6 OF 7, NOT 7: chunk (-36,-20)
 * reads 0.095 absolute -- 2.8x under vanilla's 0.264 ceiling -- but 0.698 over
 * its own null floor against a 0.448 ceiling, because that bake's null floor
 * collapses to 0.137 where vanilla's on the same ground is 0.493. That is a red
 * and is reported as one, which is why the switch ships OFF. */
void lodgenLandWarp( float wx, float wy, float * wxOut, float * wyOut );
float lodgenLandWarpAmp();
void lodgenSetLandWarpAmp( float units );        // 0 = off = the rung's bytes
float lodgenLandWarpLattice();
void lodgenSetLandWarpLattice( float units );
int lodgenLandWarpOctaves();
void lodgenSetLandWarpOctaves( int octaves );
float lodgenLandMipBias();
void lodgenSetLandMipBias( float bias );         // 0 = off = the rung's bytes

/* --- THE HISTOGRAM-PRESERVING HEX TILING (lane TILING4) ---------------------
 *
 * bungo on TILING3's warp, over cmp_tiling3.png: "the proposal looks pretty
 * good, but maybe it could use some improvement".  The improvement is the
 * SWIRLS, and they are the warp's strain: a smooth domain warp removes the
 * repeat only by stretching the texture enough to be seen.  Lane TILING4 built
 * an instrument for that stretch (the structure-tensor orientation coherence of
 * the 1-5 texel grain, the land repeat's own frequency family notched out, read
 * against the same chunk's vanilla sheet + 20 %) and it convicts the shipped
 * warp on 5 of 7 sheets and the isolated warp on 7 of 7.
 *
 * This replaces the warp behind the same switch value.  Heitz & Neyret 2018: a
 * triangle lattice of `--land-hex` world units, one random texture offset per
 * lattice VERTEX, the three offsets of a position's triangle blended with its
 * barycentric weights over a variance-preserving denominator.  The offsets are
 * PIECEWISE CONSTANT, so the strain is exactly zero away from a lattice edge
 * and the swirl cannot exist by construction.
 *
 * MEASURED (SPLAT1's independent offline model; 7 selection sheets frozen by
 * TILING3 and 7 VALIDATION sheets frozen before any candidate was scored; lane
 * TILING4).  Tile 256 world units, mip bias -0.22:
 *   * the swirl: 7 of 7 and 7 of 7, against TILING3's warp at 2 of 7;
 *   * the repeat: 6 of 7 and 6 of 7 -- worst sheet 0.350 where that sheet's own
 *     no-repeat control licenses 0.366, and ONE RED PER SET: (-4,-20) 0.308 and
 *     (-12,-20) 0.279 against the 0.264 absolute ceiling, +17 % and +6 % over.
 *     Both pass the ratio half (0.155 and 0.298 against 0.448) with a wide
 *     margin, so what fails on them is the absolute amplitude against vanilla's
 *     WORST of 22, not a repeat visible over that sheet's own energy;
 *   * the grain: no regression on any sheet (within 20 % of the rung's own hp
 *     SD, 7 of 7 and 7 of 7), and the median 19.9 % under vanilla's median on
 *     the selection seven where the rung is 26.7 % under it.
 * A per-sheet grain gate against each chunk's OWN vanilla sheet is not a gate
 * on the sampler and the numbers say so: vanilla's grain over these sheets
 * spans a factor of 5.1 because it is set by what terrain is there, while
 * anything this compositor can produce spans 1.3.  The three finest radial
 * bands are 46 %, 75 % and 97 % short of vanilla ON THE RUNG, whatever the
 * sampler does.
 *
 * THE REPEAT GATE IS THEREFORE 6 OF 7 AND NOT 7, ON BOTH SETS, WHICH IS WHY
 * THIS SHIPS OFF exactly as TILING3's warp did.  `--land-sample stochastic`
 * turns it on; `--land-hex 0` is the exact way back and the default is the
 * 2026-09-11 bake byte for byte.
 *
 * H3, the warp capped at strain 0.5 on top of this, was built and REFUSED by
 * its own numbers: it buys nothing on the repeat (6 of 7 with it, 6 of 7
 * without) and it costs the swirl -- 7 of 7 at strain 0, 6 of 7 at 0.20, 2 of 7
 * at the 0.50 the brief allowed.  Both are still reachable together, and that
 * combination is what they produce. */
float lodgenLandHexSize();
void lodgenSetLandHexSize( float units );        // 0 = off = the rung's bytes

/* --- TERRAIN-GUIDED LAND SAMPLING (lane LAND1, bungo 2026-09-12) ----------
 *
 * bungo, after the warp sweep picture: "what is used for the land sample
 * warp? the normal or slope map?" -- neither, a hashed lattice on world X/Y
 * -- and then "since we're reusing vanilla terain normals and slope maps,
 * might as well use them to guide this a bit".
 *
 * The guide is the TERRAIN'S OWN LOW-PASS SLOPE, read off the ring height
 * grid the `_msn` normal is built from (a Sobel 3x3 at a half-step of
 * --land-guide-scale/2), NOT off vanilla's `_msn` sheet: the heightmap is
 * continuous across chunk, tile and region borders and the sheet is not, and
 * at the scales these rules use the two agree -- see the lane report's
 * section 1 for the number.
 *
 * Five rules, all off by default and all off by RETURN, so the default bake
 * is the rung's bytes at both sampling sites:
 *   drag       the sample slides downhill by k * the macro normal's xy;
 *   aspect     the sampling frame is rotated by the downhill azimuth about
 *              the macro lattice cell's centre, weighted by slope;
 *   aspecthex  the same rotation carried by the hex lattice's three taps,
 *              which is seamless and shear-free by construction;
 *   slopewarp  TILING3's hash warp, amplitude scaled by the macro slope;
 *   flatwarp   the same, scaled by 1 - that.
 *
 * WHAT THE LANE MEASURED is in the report and in WW_CHANGES.md; nothing here
 * becomes a default without bungo. */
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

/* --- VANILLA FAR-TERRAIN REUSE (lane TILING3, bungo's ruling 2026-09-11) ---
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

/*! What one chunk's erosion lattice did, so the log can say it rather than the
 *  reader having to decode a sheet to find out. `moved` is lattice cells whose
 *  delta is not exactly zero, which is also the count that must be 0 when
 *  `--erosion 0` -- the off value returns before a lattice is even built. */
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

/*! THE EROSION PASS (lane GROUND1 Part B). 0 is the default, is off, and is the
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
/*! THE LANDLESS-CELL HEIGHT FILL (lane FIX1, 2026-09-26; `--land-fill-vanilla`).
 *  OFF by default. On, a cell with no LAND record takes its heights from the
 *  game's own dim-4 terrain LOD (`Meshes/Terrain/<ws>/<ws>.4.X.Y.BTR` under
 *  lodgenVanillaLodRoot(), read as input only) in the `.lodl` and in the VT
 *  pyramid's height grid. lodgenVanillaCellHeights: that cell's 33x33 heights
 *  (row 0 south, world units); false = no chunk file, or a sample no triangle
 *  covers (never a partial cell). */
bool lodgenLandFillVanilla();
void lodgenSetLandFillVanilla( bool on );
bool lodgenVanillaCellHeights( const QString & ws, int cx, int cy, float * h33x33 );
QString lodgenLandFillCensusLine();
float lodgenLandShade();
void lodgenSetLandShade( float kDiv );           // 0 = no crevice term
/*! The far-terrain colour GRADE: every baked colour texel is multiplied by this
 *  before it is quantised, in both writers. 1.0 is the default and is not a
 *  multiply -- the branch is skipped, so the off value is the previous bake's
 *  bytes by construction and not by IEEE argument. Lane GRADE1 measured the
 *  transfer curve on 25 tiles and found NO constant that helps everywhere (see
 *  docs/LODGEN_TERRAIN_VT.md 2.5f), which is why the default is 1.0 and not the
 *  pooled optimum of 0.840. */
void lodgenSetLandGrade( float k );              // 1 = off, byte-identical
float lodgenLandGrade();

/*! THE SHEET FORMAT (lane TERRAINFMT1, 2026-09-12).
 *
 *  `legacy` is the default and is the previous bake's bytes by construction:
 *  the writer is called with exactly the arguments it was called with before.
 *
 *  `vanilla` is Bethesda's measured law, not a guess. Every one of the 6,120
 *  shipped Commonwealth terrain sheets (3,060 colour + 3,060 `_msn`, over dims
 *  4/8/16/32) is DXT5, 512x512, 10 mips -- 512 down to 1, past the 4x4 block
 *  floor our writer stops at. The ALPHA of both families is a constant 255:
 *  over 50 sampled `_msn` and 50 sampled colour sheets, min 255, max 255, one
 *  distinct value across 13,107,200 texels each side. So vanilla's alpha
 *  carries NOTHING, and `vanilla` writes 255 in it. It does not collide with
 *  the `WWCV` cover stamp, which lives in dwReserved1 of `_data.DDS` -- a
 *  sheet vanilla does not ship at all (and no shipped sheet has a non-zero
 *  dwReserved1: measured over all 6,120).
 *
 *  A sheet COPIED from vanilla is not touched by this switch: re-encoding a
 *  copied BC block is what "byte for byte" forbids. */
enum LodgenSheetFormat
{
	LODGEN_SHEETFMT_LEGACY = 0,   //!< BC1, 8 mips -- the rung's bytes
	LODGEN_SHEETFMT_VANILLA = 1   //!< BC3, mips to 1x1, alpha 255
};

int lodgenSheetFormat();
void lodgenSetSheetFormat( int fmt );            // LEGACY = off = the rung's bytes

/*! THE CLEANED `_msn` CACHE (lane TERRAINFMT1, ADDED ITEM 8).
 *
 *  A directory of `<ws>.<dim>.<x>.<y>.png` sheets. Empty is the default and is
 *  off: no directory is read and no chunk changes. MEASURED on bungo's cache
 *  (2,304 files, 2048x2048 RGB) before any of this was written:
 *    * the layout is NOT vanilla's. cache R <-> vanilla R (east) r 0.956,
 *      cache G <-> vanilla B (north) r 0.794, cache B is identically 0 on 14
 *      of 16 sampled sheets (max 1 on the other two). It is a two-channel
 *      tangential normal and UP is the channel that is gone.
 *    * up is therefore RECOMPUTED here, and the triple is RENORMALISED, not
 *      clamped: a median 0.11% of texels have east^2 + north^2 > 1 (one chunk,
 *      Commonwealth.4.-40.72, has 26.28%), and clamping those would leave a
 *      normal that is not unit length.
 *    * the sheet is written UNCOMPRESSED (B8G8R8A8 through a DX10 header) with
 *      a full mip chain. A DXT re-encode would put a 4x4 block grid back into
 *      the very thing the cache removed, which is measured rather than assumed:
 *      vanilla's own 512 `_msn` reads a block-grid line of 1.0975 at period 4,
 *      the cleaned 2K reads 1.0259 at period 16, and a plain bicubic 4x upscale
 *      of vanilla -- a control that cleans nothing -- still reads 1.3460.
 *  A BC7 encoder exists (src/lodgenbc7.h, lane IMPOSTORDEPTH2, used for the
 *  card `_n` sheets) but this cache does not use it: BC7 here is unruled, and
 *  the size the uncompressed sheet costs is in the lane report. */
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

/* --- THE QUADRANT BORDER (lane TILING2) -------------------------------------
 *
 * Nothing blends across a landscape quadrant: the base texture and the whole
 * layer SET change at every 2,048-unit line (64 texels at dim 4), and the
 * 17x17 opacities are bilinear only WITHIN their own quadrant. MEASURED on
 * chunk (-20,24): all fourteen quadrant lines carry 23.6% more gradient than
 * the sheet's own mean, where vanilla's worst of 22 sheets is 10.0%, and 13 of
 * its 52 sub-texel edges sit on such a line against a 9.2% chance (p=0.0007).
 *
 * `--blend-edges quadrant` cross-fades the neighbouring quadrant's composite
 * over a margin either side of the border, evaluated at the SAME world point
 * with the neighbour's own layer set and its own edge opacities. Quadrants
 * outside the chunk have no paint loaded, so a border on the chunk's own edge
 * is not blended -- and is not blended from the other side either, so no new
 * seam is created. `off` is byte-identical to the bake before this.
 * DEFAULT `quadrant` since 2026-09-23 (bungo: "Yes, default on"; lane
 * DEFAULTS2); `--blend-edges off` is the way back, byte for byte. */
int lodgenBlendEdges();               // 0 = off, 1 = quadrant cross-fade
void lodgenSetBlendEdges( int mode );
float lodgenBlendMargin();            // world units either side of the border
void lodgenSetBlendMargin( float units );

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
	 * B = heightfield AO, A = 255. The Physical Weathers tie-in.
	 *
	 * OFF since 2026-09-12 (lane DEFAULTS1). bungo, 15:53: "We don't bake BTR
	 * for FO4CS, and so we do not use of that data for it at all." The legacy
	 * .BTR carries vanilla's vertex layout; `--terrain-identity` opts in. */
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
 * from docs/TO_BE_IMPLEMENTED.md.
 *
 * A manifest text file (one line per index: formID, base type, position,
 * scale) is written beside the chunk ALWAYS, identity or not (lane DEFAULTS1,
 * 2026-09-12): it is a SIDECAR, not chunk data, and the texture arrays, the
 * impostor cards, the shape merge and the far-ring cut all read it back.
 */
struct LodgenObjectOptions
{
	int dim = 4;
	QString dataRoot;           //!< Data folder holding meshes\\lod\\... sources
	/*! Per-vertex identity channels INSIDE the .BTO (CS profile). OFF since
	 *  2026-09-12 (bungo 15:56: "Legacy terrain bakes stay as they were, no
	 *  extra data for FO4CS to be included in them"); `--identity` opts in.
	 *  The manifest sidecar is NOT gated on this. */
	bool identity = false;
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
	 * Data\FO4CSLOD\Cards\<id>.DDS (lane LAYOUT1, 2026-09-16) — ship that directory there. */
	QString impostorDir;
	/*! From this MNAM level on (0 = dim 4), a placement whose base has a card
	 * stands on the card even where the ring's slot has a mesh: one quad per
	 * tree at the near rings, for a consumer that draws the octahedral
	 * sheets. -1 = cards only where a slot is empty (the default). The stock
	 * engine sees the crossed quads there, so the panel offers it under the
	 * FO4CS target only. bungo, 2026-09-06: "some of the closer ones could be
	 * replaced with a higher res LOD impostor for performance gain". */
	int impostorFromLevel = -1;
	/*! Impostor cards are TREES ONLY (bungo, 2026-09-11 07:0x: "I've only
	 * wanted trees for the impostors"; amended 07:1x "Make trees only a
	 * toggle"). ON, the default: only a base the chunk builder's own tree
	 * test calls a tree may stand on a card. OFF: every base in the plugin's
	 * own LOD set whose ring slot is EMPTY may stand on one -- the old
	 * "missing" rule and nothing else, no type, folder or size filter.
	 *
	 * `impostorFromLevel` (the panel's "Tree cards from ring") is tree-only
	 * in BOTH states: replacing a non-tree's authored mesh with a quad was
	 * never asked for, and bungo's ruling names trees. A per-object picker
	 * is parked, by his word, until he asks for it. */
	bool treesOnly = true;
};

/*! The chunk builder's tree test on a model path: under a `trees` folder, or a
 *  file named `tree...` AND SITTING UNDER `Landscape\`. FO4 has no class field;
 *  TREE records count too.
 *
 *  The `Landscape\` scope on the filename clause is lane ROADS2's, out of lane
 *  FLAGSCAN1's census: seven shipped models are named `Tree...` outside the
 *  landscape tree set and every one of them is a `SetDressing\` prop -- two
 *  tree swings, a swing rope pile, a grounded swing, a no-swing swing, a noose
 *  branch and a hanging mannequin -- four of which stand in Sanctuary, where
 *  the bare filename clause called them trees. The clause still earns its
 *  place: 167 `LOD\Landscape\Tree*.nif` far models and two
 *  `Landscape\Plants\Tree*.nif` are named, not foldered, so a leading `meshes`
 *  and a leading `lod` are dropped before the `landscape` component test. */
bool lodgenIsTreeModel( const QString & model );

/* AGGREGATE RING-3 IMPOSTORS -- the two calls the compositor needs from this
 * file (bungo 2026-09-11 08:3x). The composite itself is
 * src/lodgenaggregate.{h,cpp}; these two are here because this file owns the
 * ONE card-sidecar reader and the DDS writer. */
QHash<quint32, LodgenAggCard> lodgenAggregateCards( const EsmWorld & world, const int region[4],
	const QString & cardDir, int auxDiv, QStringList * notes );
//! Write one set's three sheets and its `kind: "aggregate"` `.lodm` under a Data root.
bool lodgenAggregateWrite( const QString & outRoot, const QString & worldspace,
	const LodgenAggSet & set, QStringList * written, QString * error );

/*! THE FOUR STAGE TIMES of a bake, as one line, stated ONCE so the panel and
 *  the command line cannot word them differently (bungo asked for the four
 *  times of a GUI bake, 2026-09-11; the CLI prints the same four).
 *
 *  landscape = the `.lodl` and the shadow heightmap; meshes = the chunk
 *  builders, the shape merge, the far-ring cut and the native pair; textures =
 *  the terrain pyramid, the chunk sheets, the object texture arrays and the
 *  atlas; impostors = the card arrays. Milliseconds in, seconds to one decimal
 *  out. A stage that did not run is exactly 0.0, which is what makes the line
 *  testable rather than decorative. */
/*! The four stage times of a bake, in one line, in the order the work happens.
 *
 *  `librarySplit` (lane PERF1, 2026-09-17) is `lodgenNativeLibrarySplit()`, the
 *  FO4CS library build's own seven-way split, appended in parentheses when a
 *  native pair was written and omitted entirely when one was not -- so a stock
 *  bake's line is byte for byte the line it always had, and an FO4CS bake says
 *  where the seconds inside `meshes` went. It rides on THIS line on purpose: the
 *  line is already the record's fourth volatile field and is already masked
 *  whole by both normalisers, so the split costs no sixth volatile field. */
QString lodgenStageTimeLine( qint64 msLandscape, qint64 msMeshes, qint64 msTextures, qint64 msImpostors,
	const QString & librarySplit = QString() );
struct NativeSrcShape;
bool lodgenNativeLoadModel( void * user, const QString & model, std::vector<NativeSrcShape> * out );

/*! MATERIAL SWAPS (lane SWAP1, 2026-09-25). One MSWP substitution as the
 *  loader applies it: `first` is the ORIGINAL material and `second` the
 *  REPLACEMENT, both as lodgenMaterialSwapKey() folds them. */
typedef QVector<QPair<QString, QString>> LodgenMaterialSubst;

/*! The one folding both sides of a swap are compared in: lower case,
 *  backslashes, everything up to and including the LAST `materials\` cut, so
 *  a NIF's `Materials\LOD\X.BGSM`, an MSWP row's `lod\x.bgsm` and a build
 *  machine path `c:\...\materials\lod\x.bgsm` all come out `lod\x.bgsm`. */
QString lodgenMaterialSwapKey( const QString & material );

/*! lodgenNativeLoadModel with a material swap applied: every shape whose
 *  material (folded) is some `first` is loaded as if the NIF named `second`
 *  (as `Materials\<second>`), and its textures and constants are resolved from
 *  that material through the resource stack, exactly as for a NIF's own. A
 *  shape the swap does not name is loaded unchanged. */
bool lodgenNativeLoadModelSwapped( void * user, const QString & model, const LodgenMaterialSubst & swap,
	std::vector<NativeSrcShape> * out );

/*! Lane NEAR1 (2026-09-26): the same load, swap optional, with NO cache -- the
 *  parse is dropped on return. The near-library bake visits every full-detail
 *  model of a worldspace and must not hold them all. */
bool lodgenNativeLoadModelOnce( void * user, const QString & model, const LodgenMaterialSubst * swap,
	std::vector<NativeSrcShape> * out );

/*! Lane NEAR1 (2026-09-26): a texture's format and size WITHOUT decoding it, for
 *  the near library's texture-array sidecar. A BA2 texture record carries the
 *  DXGI code, width, height and mip count in its own index entry, so no chunk is
 *  read; a loose file gives its 148-byte header. Anything else (the game
 *  manager's archives, a legacy DDS) reads the file through lodgenReadAsset and
 *  parses its header. `dxgi` is the DXGI_FORMAT code (a legacy FourCC is mapped:
 *  DXT1 71, DXT3 74, DXT5 77, ATI1/BC4U 80, ATI2/BC5U 83; uncompressed 32-bit
 *  87 for BGRA, 28 for RGBA). `source` says which route answered: "ba2",
 *  "loose" or "read". False when nothing supplies the file. */
bool lodgenTextureInfo( const QString & dataRoot, const QString & texPath, quint32 * dxgi, quint32 * width,
	quint32 * height, quint32 * mips, QString * source = nullptr );

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
	/*! ROADS AND DECALS IN THE FAR TERRAIN COLOUR (bungo 2026-09-11, verbatim:
	 *  "We do the same with roads and decals as vanilla").
	 *
	 *  ON by default under BOTH targets, because vanilla's own graded bakes
	 *  carry them: lane ROADS1 measured Bethesda's shipped
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
	/*! Ground cover under a road. A road surface is not soil, so the cover byte
	 *  is scaled by one minus the road's coverage and the grass tint goes with
	 *  it -- grass grows BESIDE a road, not through it. 1.0 = full suppression
	 *  (the default), 0.0 = the cover plane is left alone. */
	float roadCoverSuppress = 1.0f;

	/*! HOW STRONGLY the road paint is mixed into the ground it lies on --
	 *  the alpha of the road plane is scaled by this before the composite
	 *  `colour = ground + ( roadColour - ground ) * a * roadOpacity`.
	 *
	 *  1.0 is the shipped default and is the bake as it has always been: the
	 *  multiply is branched over entirely at 1.0, so off is the previous
	 *  bake's BYTES and not a float argument about 1.0f. 0.0 leaves the
	 *  ground untouched by the paint while the road GEOMETRY is still there
	 *  -- the ground-cover suppression below deliberately keeps using the
	 *  UNSCALED coverage, because a faintly painted road is still a road and
	 *  grass still does not grow through it.
	 *
	 *  WHY THE KNOB EXISTS (lane ROADS3, measured on the rung's own sheets,
	 *  every number beside the floor it was read against):
	 *
	 *    vanilla's far road stands +4.29 luminance levels over the ground
	 *    around it on chunk (-20,20) and +4.40 on (-8,8) -- two tiles a whole
	 *    biome apart -- while ours stands +29.96 and +3.84. So ours is right
	 *    on one tile (0.56 of a level) and 25.67 levels too contrasty on the
	 *    other, which is the one bungo looked at.
	 *
	 *    the reason is that our paint is a FIXED material colour while
	 *    vanilla's road follows the ground under it. Road luminance regressed
	 *    on the mean luminance of the non-road texels within 8 texels:
	 *    vanilla +0.714 and +0.755, our own unpainted ground +0.637 and
	 *    +0.565, ours +0.339 and +0.209. Floors, both sides: the same field
	 *    translated reads -0.009 mean and 0.298 worst over 5 draws, a
	 *    known-answer fixed paint reads +0.000, a known-answer ground+4 reads
	 *    +1.000.
	 *
	 *  It is NOT set away from 1.0 here, and that is a refusal with numbers,
	 *  not an omission. No single opacity meets this lane's gates at once: on
	 *  (-20,20) vanilla's absolute level wants 0.83, vanilla's rise over the
	 *  ground wants 0.326, the two-tone step wants 0.25 or less and the local
	 *  detail SD wants 0.75 or more; on (-8,8) NO opacity can reach vanilla's
	 *  road at all, because the composite can only land between our ground
	 *  (102.12) and our paint (106.68) and vanilla's road is at 94.59, 7.53
	 *  levels outside that interval. The gap is the GROUND's, which lane
	 *  TILING2 measured and told this lane in writing not to chase with the
	 *  road pass. The default is bungo's call; the priced table and the
	 *  pictures are in scratchpad/lane_roads3_report.md.
	 *
	 *  NOT a hue knob, and that is measured too: road minus surround on the
	 *  opponent axes reads vanilla +2.30 / -2.14 and ours +3.35 / -3.25 on
	 *  (-20,20), vanilla +4.10 / -3.07 and ours +1.96 / -1.24 on (-8,8) --
	 *  same sign, same direction, every gap under 3 levels. The difference
	 *  bungo sees is brightness, not colour. */
	float roadOpacity = 1.0f;

	/*! HOW road pieces combine where they overlap.
	 *
	 *  `RoadMaxZ` is lane ROADS1's: a z buffer over the pieces and the topmost
	 *  triangle OVERWRITES the texel. `RoadBlend` composites instead -- the
	 *  pieces are painted in a stated order (ascending mean world Z; at equal
	 *  height a non-decal before a decal; at equal both, gather order) and each
	 *  writes `dst = lerp( dst, src, srcAlpha )`, srcAlpha being the material's
	 *  opacity times the interpolated vertex alpha for an alpha-BLENDED shape,
	 *  the alpha test's 0-or-1 for an alpha-TESTED one, and 1 for an opaque one.
	 *
	 *  `max-z` IS THE DEFAULT, and the reason is a measured conflict the brief
	 *  did not know about. Both composites were baked on chunk (-20,20) at
	 *  `roadDetail` 0 and scored by `tests/spells/lodgen_roads_metric.py`, the
	 *  pre-registered road-presence check: the fraction of vanilla's own road
	 *  centreline where our RGB lands within 16/255 of Bethesda's, per channel.
	 *  max-z 0.3404, blend 0.2669, against bar 1 (twice the `--no-roads` floor)
	 *  0.2694 and bar 2 (0.8 x the surrounding ground's own agreement) 0.3228.
	 *  max-z clears both; blend fails both, and lane RESUME3's inherited red
	 *  stays red. On the seam metric max-z is also the better of the two at the
	 *  piece boundaries (5.996 against 6.204, vanilla 5.271). Blend wins three
	 *  smaller rows -- local 5x5 SD on the road 6.542 against 6.820, texture
	 *  phase R2 0.033 against 0.052, mean road colour error 12.50 against 14.09
	 *  -- which is why it is kept and offered, not deleted.
	 *
	 *  `--roads-legacy` is the EXACT WAY BACK, one token, and it means ROADS1's
	 *  whole pipeline: max-z AND `roadDetail` 1 AND `roadRaised` AND
	 *  `roadSidewalks`, so the bake is byte-identical to the one before this
	 *  lane existed (tests/spells/lodgen_roads.sh R6). */
	enum RoadComposite { RoadMaxZ = 0, RoadBlend = 1 };
	int roadComposite = RoadMaxZ;

	/*! HOW MUCH of the road diffuse's own pattern is printed into the sheet:
	 *  `c.rgb = lerp( wholeTextureAverage, footprintSample, roadDetail )`.
	 *
	 *  1 IS THE DEFAULT BY BUNGO'S RULING, 2026-09-12, verbatim: *"--road-detail
	 *  1 is always on, do not ever use road detail 0, that looks terrible"*. It
	 *  is the footprint sample -- ROADS1's original behaviour. 0 is the other
	 *  end, one flat colour a material, which is what lane ROADS2 measured
	 *  vanilla's far road to be and shipped as the default on 2026-09-11; he
	 *  looked at both in one picture (`scratchpad/road_detail_look.png`,
	 *  vanilla | detail 0 | detail 1 on chunk (-20,20)) and rejected 0. His eye
	 *  outranks the measurement here, and the measurement is kept below so the
	 *  cost of 1 is not forgotten rather than because it argues for 0.
	 *
	 *  What 0 was measured to fix, and therefore what 1 costs: on chunk
	 *  (-20,20), with the material held fixed so only the texture's own pattern
	 *  can explain anything, our sheet's road luminance correlates 0.852 with
	 *  that footprint sample and vanilla's correlates 0.016; the road's own UV
	 *  repeat measures 256 world units, which at 32 units a texel is 8 texels,
	 *  and that is the spacing of the bands bungo saw. The same tile: variance
	 *  of the road luminance explained by the texture phase alone, ours 0.140,
	 *  vanilla 0.013, our own `--no-roads` floor 0.022; local 5x5 luminance SD
	 *  on the road, ours 10.33, vanilla 6.62, and off the road 5.52 against
	 *  5.38, so the excess is the road's and not the ground's.
	 *
	 *  Vanilla's far road is a flat colour a material. The alpha channel is NOT
	 *  flattened -- it cuts alpha-tested decals out, and averaging it would
	 *  dissolve them.
	 *
	 *  At the default of 1 the lerp is BRANCHED OVER entirely (`detail < 1.0f`
	 *  in `rasterise`), so the flat-colour lookup never happens and a default
	 *  bake is a `--road-detail 1` bake byte for byte. `--roads-legacy` still
	 *  writes 1 explicitly, which is now a no-op and stays written so the way
	 *  back survives a further move of this default. */
	float roadDetail = 1.0f;

	/*! THE COVERAGE MULTIPLIER for a road shape whose MATERIAL IS A LANDSCAPE
	 *  GROUND MATERIAL -- one that lives under `materials/Landscape/Ground/`,
	 *  the same folder the landscape's own painted textures come from.
	 *
	 *  WHAT IT IS FOR. A Fallout 4 road model is not only asphalt. Inside
	 *  `Landscape/Roads/Sanctuary/SancRoadStr01.nif` and its siblings there are
	 *  shapes carrying `CommonwealthDefault01.bgsm` and `DirtGravel01.BGSM` --
	 *  the verge, modelled as terrain and materialled as terrain. The road pass
	 *  cannot tell them from the road surface: they are opaque, so the coverage
	 *  clause gives them 1, and the max-z composite prints them over the ground
	 *  the landscape pass already painted from the cell's own LAND record.
	 *
	 *  MEASURED (lane ROADS4, 2026-09-12, on the 04:10:38 exe's sheets, the
	 *  classification by the material's own FOLDER and not by its name):
	 *
	 *    chunk (-20,20): 8,337 of 23,116 painted road texels (36.1%) are won by
	 *    a ground-material shape; chunk (-8,8): 2,756 of 11,069 (24.9%).
	 *
	 *    where such a patch meets the road surface INSIDE the road plane, the
	 *    mean luminance gradient is 15.387 on (-20,20) and 8.010 on (-8,8);
	 *    Bethesda's shipped sheet reads 5.362 and 5.138 at the same texels, and
	 *    the same boundary set displaced five ways reads 6.077 and 4.805 for
	 *    ours. So the step is roughly three times vanilla's on (-20,20) and
	 *    well clear of its own floor on both.
	 *
	 *    the two classes are 25.3 levels apart in our bake on (-20,20) (road
	 *    surface 110.83, ground-material 85.53) where vanilla's are 2.5 apart
	 *    (93.52 and 91.01). Vanilla's far road is nearly one tone; ours is two.
	 *
	 *  bungo, 2026-09-12, verbatim: *"the issue with the roads is, these meshes
	 *  have some terrain included there, you can see the sharp mesh terrain
	 *  being included into the chunk's bake"*. That is this, and the numbers
	 *  above are its size.
	 *
	 *  WHAT THE VALUE MEANS. The multiply happens on COVERAGE, so it moves the
	 *  paint and the ground-cover suppression together: at 0 a ground-material
	 *  shape neither paints the sheet nor keeps grass off the verge, and the
	 *  landscape's own colour is what stays. 1.0 is branched over entirely, so
	 *  a bake at 1.0 is the pre-2026-09-12 bake's BYTES.
	 *
	 *  DEFAULT 0 since 2026-09-12 (lane DEFAULTS1). bungo, 16:55, over a
	 *  picture of SancRoadCrvCustom02.nif's verge plane: "the grass meshes
	 *  included with the road nifs" are excluded from the road plane.
	 *  `--road-ground-paint 1` is the way back. */
	float roadGroundPaint = 0.0f;

	/*! Put the RAISED road families back into the ground sheet. Off, the
	 *  default, a road base that carries its own Distant LOD mesh is refused,
	 *  and so is anything under `Landscape\Roads\HighwayOverpass\` or
	 *  `...\Bridge\` -- see `lodgenIsRaisedRoadModel`. On is the way back to
	 *  ROADS1, which painted them. */
	bool roadRaised = false;

	/*! Paint `Landscape\Sidewalks\` with the roads. OFF is the default, and
	 *  the number that decided it was measured on chunk (-8,8) downtown, which
	 *  carries 17,801 projected sidewalk texels (the brief asked for at least
	 *  5,000), 15,696 of them more than two texels from any flat road so the
	 *  two families cannot be confused:
	 *
	 *    on those pure sidewalk texels vanilla's sheet sits BELOW its own
	 *    displaced-mask floor in brightness (AUC 0.518, floor top 0.620,
	 *    clearance -0.102): Bethesda paints no pale pavement ribbon there.
	 *    Ours cleared the same floor by +0.284 and read 128.4 mean luminance
	 *    against vanilla's 86.5 -- 42 units, 49 percent too bright, the worst
	 *    family error in the sheet, against 18.5 for the tile as a whole.
	 *
	 *    the same tile says the flat ROAD family is right: vanilla's clearance
	 *    +0.100, ours +0.101, mean error 16.7 -- better than the tile's own.
	 *
	 *  So the roads stay and the pavements go. On is the way back, and
	 *  `--roads-legacy` includes it. */
	bool roadSidewalks = false;

	/*! THE FAR TERRAIN RECEIVES AMBIENT OCCLUSION FROM THE PLACED OBJECTS
	 *  (lane GROUND1, bungo 2026-09-11 15:4x: "Okay, so the AO can be acurate
	 *  from objects").
	 *
	 *  OFF by default, and off is the previous bake's BYTES, not an argument
	 *  about a float: the object term is a SEPARATE eight-direction horizon
	 *  march whose visibility multiplies the terrain one, and it returns
	 *  exactly 1.0f where no object stands within the march's reach, so
	 *  `vis * 1.0f` is the same float and rounds to the same byte. A region
	 *  with no LOD-bearing placement is byte-identical with this on.
	 *
	 *  WHAT OCCLUDES: the base record's level-0 distant-LOD mesh (MNAM slot 0),
	 *  rasterised top-down as max-Z into a world-aligned 128-unit lattice. A
	 *  base with no LOD mesh at all is NOT drawn in the far ring, so it does not
	 *  darken the far ground either -- it is refused BY NAME into the census
	 *  rather than silently skipped.
	 *
	 *  WHERE IT LANDS: the AO byte of both composites -- the stock per-chunk
	 *  `_data` sheet's R and the pyramid tile's mask B (which is the same
	 *  `ao8` the tile also writes into `_data`'s R). Coarser pyramid levels
	 *  inherit it through the existing box filter. It does NOT reach the .lodl
	 *  AO plane: that plane is computed from the container's own stored height
	 *  word by the one function that serves both the writer and `--refresh-ao`,
	 *  and adding objects there would break that byte-identity rule silently.
	 *  The combination is refused in words in `src/nifcli.cpp`.
	 *
	 *  It does NOT reach the per-vertex terrain profile either (`.bto` vertex
	 *  colour B / UV2.x): that writer runs only under `--terrain-identity`,
	 *  whose whole purpose is reproducing Bethesda's own bytes.
	 *
	 *  REACH: 1458 world units. The march is
	 *  `for (float dist = 128; dist <= 2048; dist *= 1.5)`, so the steps are
	 *  128, 192, 288, 432, 648, 972, 1458 and 1458*1.5 = 2187 stops the loop.
	 *  1458 u is 0.356 of a cell, so the incremental ledger's dependency map
	 *  widens by one cell in every direction when this is on
	 *  (docs/LODGEN_LEDGER_FORMAT.md). */
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

	/*! THE SLAB LATTICE (lane SLAB1, 2026-09-18). The object march reads the
	 *  lowest object surface over a square as well as the highest, so a deck
	 *  1,000 units up is a CEILING -- sky still reaches the ground under it
	 *  from the sides -- instead of a solid block from the ground to its top.
	 *
	 *  Default = the new law. It is a sub-toggle inside `--terrain-object-ao`
	 *  and NOT a dial: there is nothing to tune. It exists for two reasons and
	 *  both are gates, not taste: it makes "red on the old formula" runnable on
	 *  one exe (tests/spells/lodgen_slab.sh), and 0 is the exact way back to
	 *  the sheet bytes of the bakes bungo has already looked at (hotfix 7c).
	 *
	 *  0 reproduces the max-Z reading bit for bit -- see
	 *  `lodgenObjectSkyVis`, where the wall branch is the whole of the old
	 *  loop and the ceiling term is never even added. */
	bool terrainObjectAoSlab = true;

	/*! Write the object height lattice itself to a file, so the term can be
	 *  audited against something other than its own output.
	 *
	 *  Format, little-endian, 24 bytes of header then `gw*gh` float32 rows
	 *  south to north, west to east: magic 'OBJH', int32 gx0, gy0, gw, gh,
	 *  float32 cell (128). A square with no object holds -1e30f.
	 *
	 *  SLAB1 appends a SECOND `gw*gh` float32 plane after the first: the LOWEST
	 *  object surface over each square, +1e30f where there is none. This is a
	 *  DEBUG FILE and not a shipped format -- nothing reads it back, the bake
	 *  never opens it, and a reader tells the two versions apart by file length
	 *  (24 + n*4 vs 24 + n*8). No shipped format changed.
	 *
	 *  It exists because the gate that asks "is the ground under a building
	 *  darker than the same ground elsewhere" needs the building's footprint
	 *  from somewhere OTHER than the darkening map, or it is circular. */
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
 *  two together are the rule lane ROADS1 measured. */
bool lodgenIsRoadModel( const QString & modelPath );

/*! THE RAISED-ROAD TEST, on the model path alone: `landscape` / `roads` /
 *  (`highwayoverpass` | `bridge`) / at least one more component.
 *
 *  The rule the road pass actually applies is the UNION of this and the base
 *  record's own `Has Distant LOD` (EsmLodBase::hasLod, set from its MNAM rows),
 *  and the reason is the general one: a base that carries a distant LOD mesh is
 *  DRAWN as an object at distance, so painting it into the ground as well would
 *  draw it twice -- once in the air where it stands and once flattened on the
 *  soil underneath.
 *
 *  Over the 470 road and sidewalk STAT bases in the shipped Commonwealth, lane
 *  ROADS2 measured MNAM presence and header bit 15 to be the same set, and that
 *  set to be exactly the raised families: HighwayOverpass 54 of 74, Bridge 6 of
 *  8, River 11 of 15, Park 11 of 35, Plaza 1 of 1, and ZERO across the flat
 *  folders -- (root) 167, Alley 25, City 69, Country 22, Dirt 19, GlowingSea 7,
 *  LexingtonGreen 4, Raised 6 (raised CURBS, which are ground-level), Sanctuary
 *  18. The path clause is here because 20 HighwayOverpass and 2 Bridge bases
 *  ship with no MNAM at all: bit 15 is 100% specific to raised pieces and 73%
 *  sensitive (lane FLAGSCAN1), and the two named folders close the rest. */
bool lodgenIsRaisedRoadModel( const QString & modelPath );

/*! `Landscape\\Sidewalks\\` -- the pavement and kerb family, component
 *  equality like the two tests above (a leading `meshes` dropped, then
 *  `landscape` / `sidewalks` / at least one more). `lodgenIsRoadModel` accepts
 *  it, so the paint has always included it; `LodgenCoverOptions::roadSidewalks`
 *  decides whether it is kept, and the measurement that set that default to
 *  off is written out there. */
bool lodgenIsSidewalkModel( const QString & modelPath );

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
	int refusedRaised = 0;      //!< road placements refused as raised (lodgenIsRaisedRoadModel or hasLod)
	int raisedBases = 0;        //!< distinct raised bases behind those refusals
	int blendTexels = 0;        //!< texels a partially transparent road shape composited into
	int refusedSidewalk = 0;    //!< placements refused as `Landscape\Sidewalks\` (roadSidewalks off)
	int sidewalkBases = 0;      //!< distinct sidewalk bases behind those refusals
	int groundShapes = 0;       //!< road shapes whose material is under materials/Landscape/Ground/
	int groundTexels = 0;       //!< texels such a shape wrote (0 when roadGroundPaint is 0)
	QStringList refusals;       //!< "<reason> <name>", deduplicated, capped at 16
	void addRefusal( const char * why, const QString & name );
	void add( const LodgenRoadCensus & o );
	//! ONE physical line, `key=value` tokens, never parsed by field position.
	QString line() const;
};

/* ---------------------------------------------------------------------------
 * THE MASK LAW -- one home for "what roughness and metallic does this material
 * have", shared by the object path's gloss composition and the far-terrain
 * mask sheet (CONSTITUTION 10: what is shared lives in the shared code).
 *
 * bungo's rulings, 2026-09-11 09:3x and 09:4x, verbatim: "if PBRM is used to
 * bake it, it gets roughness, if a vanilla legacy material, its gloss gets
 * inverted into roughness" and "then also add metallic map, but that should
 * only get derived from PBRM". So there are exactly three answers and each
 * NAMES itself in the census, because a silent fallback is the thing that
 * cannot be audited afterwards.
 * --------------------------------------------------------------------------- */

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

/*! THE LEGACY GLOSS, as this tree has always composed it: `smoothness` times
 *  the `_s` map's GREEN channel (lodgen.cpp's arrays pass), or the normal map's
 *  alpha where there is no `_s` map. One function so the arrays pass and the
 *  terrain resolver cannot drift: the terrain's roughness is literally
 *  `1 - lodgenLegacyGloss(...)` and must invert the same number the object
 *  sheets store. */
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
	/*! THE VANILLA-COLOUR FILL (lane SEAM1, docs/LODGEN_TERRAIN_VT.md 2.6): blend
	 *  the ground no LAND record paints toward Bethesda's own LOD diffuse for
	 *  those cells, tone-matched on the overlap. Read at bake time only. OFF by
	 *  default; a bake without it is byte-identical to one before it existed. */
	bool vanillaFill = false;
	/*! Where the ground-cover byte lives (bungo's open question, 2026-09-11
	 *  09:5x: the mask's A, mirroring the object family's subsurface slot, or
	 *  the colour sheet's A, the object family's `coverage` slot).
	 *
	 *  FALSE -- the mask -- is what ships, because the colour sheet's alpha is
	 *  the one slot `.lodm` 2.1 defines as OPACITY and a family-conformant
	 *  consumer alpha-tests it: ground cover there would punch holes in the
	 *  ground wherever grass is thin. True is the exact way back to the other
	 *  arm and is one flag, one sheet-format pair and no second code path. */
	bool coverInColor = false;
	/*! HALF-RESOLUTION AUX SHEETS (lane VTNORMAL1, `--vt-half-aux`, panel row
	 *  "Half-resolution normal, mask, height and emissive tiles"). The colour
	 *  sheet stays at the chosen texel density; msn, mask, height and emissive
	 *  store half the texels a side (their mip 0 is dropped, descriptor byte 6
	 *  says so). OFF by default and byte-identical when off. */
	bool halfAux = false;
	/* THE TERRAIN HORIZON SHEET (lane HORIZON1, 2026-09-18), `.lodt` role 7,
	 * REMOVED 2026-09-19 by lane HORIZONOUT on bungo's "horizon goes bye bye
	 * now, we're back to identity". No bake writes a role-7 sheet any more and
	 * there is no switch that turns one on. The ROLE stays defined in
	 * src/io/lodvfile.h and its reader-side rules stay enforced, so a .lodt
	 * baked by `release/NifSkope.before_horizonout.exe` still opens. The
	 * measured reason is in docs/LODGEN_TERRAIN_VT.md's history paragraph. */
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

/* ================= THE INPUT LEDGER (.lodb) -- lane INCR1, 2026-09-12 =======
 *
 * An incremental bake is only worth having if it is BYTE-IDENTICAL to the full
 * bake it replaces, and the only way to be sure of that is to record, per
 * chunk, a digest of every input that chunk's outputs actually read.  The
 * dependency map that decides what goes in is in
 * `docs/LODGEN_LEDGER_FORMAT.md` section 2 and it was written BEFORE this code.
 *
 * THE ONE THING TO KNOW ABOUT THE DIGEST: it is CONSERVATIVE BY CONSTRUCTION.
 * Every doubt is resolved toward including more, so the ledger can say "dirty"
 * about a chunk that did not need rebaking (costing time) but can never say
 * "clean" about one that did (costing correctness).  A reader who finds it
 * firing too often should widen nothing and narrow carefully.
 */

//! One chunk's row in the ledger.
struct LodgenLedgerEntry
{
	int dim = 4, cx = 0, cy = 0;
	QString inputs;                 //!< sha1 hex over the chunk's inputs, incl. the one-cell ring
	QStringList outFiles;           //!< paths relative to the ledger's directory
	QStringList outDigests;         //!< sha1 hex, parallel to outFiles
};

/*! A whole ledger: the run-level identity plus one row per chunk.
 *
 *  v2 (lane BAKEREC1, 2026-09-17) added everything below `chunks`, because the
 *  same file is now the BAKE RECORD as well as the ledger. `src/lodbfile.h`
 *  says why there is one file and not two, and names the three lines of it that
 *  are deliberately NOT deterministic. */
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
};

/*! The digest of everything chunk (cx,cy) at `dim` reads.
 *
 *  Covers the chunk's own cells AND the ONE-CELL RING around them, because the
 *  terrain ring (LODGEN_TERRAIN_RING_CELLS) and the AO skirt (aoSkirtCells)
 *  both reach exactly that far and the land-guide macro gradient reaches less.
 *  Per cell: the LAND heights, the vertex colours, the splat layers and their
 *  LTEX ids; then every LOD-bearing REFR, its base, its LOD model paths and the
 *  BYTES of every model file those paths resolve to through the resource stack.
 *
 *  Deterministic: no timestamp, no path outside the game's own naming, no
 *  iteration over a hash.  Two runs on an unchanged tree produce the same hex.
 */
QString lodgenChunkInputDigest( const EsmWorld & world, int dim, int cx, int cy,
	const QString & dataRoot );

//! sha1 hex of a file's bytes; empty when it cannot be read.
QString lodgenFileDigest( const QString & path );

/*! Write / read the record. v2 is PLAIN TEXT (UTF-8, LF, `key<TAB>fields`); the
 *  reader recognises the v1 binary container by its magic and refuses it by
 *  name rather than mis-parsing it. Implemented in `src/lodbfile.cpp`. */
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
