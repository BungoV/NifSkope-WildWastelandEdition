/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef NATIVEEMIT_H
#define NATIVEEMIT_H

#include "lodofile.h"
#include "lodgenaggregate.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <vector>

class EsmWorld;

/*! The FO4CS-native emitter: takes what lodgenBuildObjectChunk already
 *  computes per placement -- the base, the (ref, part) key, the DRAWN
 *  transform (the tree yaw folded in), the slot it picked, the tree test and
 *  its hash, the alpha and emit facts -- and writes `<WS>.lodo` + `<WS>.lodi`
 *  for the worldspace (docs/LODGEN_NATIVE_LODO_LODI.md). It never stitches:
 *  the library is built once from every LOD model the worldspace's FULL base
 *  census names, so `baseId` and mesh ids are worldspace-stable in every bake,
 *  including a one-chunk one (spec 8, 9.3).
 *
 *  It is a process-wide accumulator, like the resource stack, so the hook-up
 *  inside the chunk builder is one call per placement and one per lit vertex,
 *  and the region driver calls begin / write / end around its chunk loop
 *  (scratchpad/native0_20260910/HOOKUP_CHANGE_NEEDED.md). Inactive (no
 *  begin) every call is a no-op and the stock bake is untouched -- the
 *  standing gate of every native lane. */

//! One shape of a LOD model, as the loader hands it over: geometry plus the material facts.
struct NativeSrcShape
{
	LodoSrcShape geom;          //!< model-space positions, unit normals/tangents, UVs, triangles (sway filled by the emitter)
	QString tex0, tex1, tex7, matName;
	/*! The BGEM base map, carried BESIDE `tex0` and never into it (lane
	 *  CELLVIEW3). `materialKey()` and `shapeEmits()` read `tex0/tex1/tex7/
	 *  matName` and are not touched, so the far-LOD bake's bytes are unchanged
	 *  BY CONSTRUCTION -- `tests/spells/lodgen_native_baseline --check` is the
	 *  refuter. Only the cell viewer reads this. */
	QString effectTex0;
	//! The shape named a material and NOTHING resolved: drawn neutral and counted, not magenta.
	bool matUnreadable = false;
	bool hasAlpha = false;
	quint8 alphaThreshold = 128;
	bool ownEmit = false;
	float emitColor[3] = { 0.0f, 0.0f, 0.0f };
	float emitMult = 1.0f;
	float smoothness = 1.0f, specMult = 1.0f;
};

//! The loader the emitter uses for every model of the census (lodgen.cpp's
//! lodgenLoadModel behind a lambda; the resource stack applies).
typedef bool ( *NativeModelLoader )( void * user, const QString & model, std::vector<NativeSrcShape> * out );

//! One placement, as the chunk builder saw it.
struct NativePlacement
{
	quint32 baseForm = 0;
	quint32 refForm = 0;
	int scolPart = -1;
	float pos[3] = { 0.0f, 0.0f, 0.0f };        //!< world units, the pre-quantisation float position
	float rot[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };   //!< row-major, world = R * local; the DRAWN rotation (tree yaw included)
	float scale = 1.0f;
	int slot = 0;                       //!< the MNAM slot the chunk builder drew
	QString model;                      //!< the model it drew
	bool isTree = false;
	bool mirrorU = false;               //!< (treeHash >> 8) & 1
	quint32 treeHash = 0;               //!< qRound(pos[0]) * 2654435761 ^ qRound(pos[1]) * 40503, 0 for a non-tree
	bool hasAlpha = false;              //!< any drawn shape alpha-tested
	bool emits = false;                 //!< any drawn shape own-emits with a non-black colour
	int objectIndex = -1;               //!< the chunk's identity index, so lighting can find this record
	int chunkX = 0, chunkY = 0, dim = 4;    //!< the stock chunk it came from
};

/*! Arm the emitter for one worldspace. `outDir` receives `<WS>.lodo` and
 *  `<WS>.lodi`; `meshReportPath`, when given, receives one line per library
 *  mesh (the GPU-cache-order and silhouette numbers -- `--native-mesh-report`). */
/*! v3: `buildLadder` and `buildOccluders` are the two EXACT ways back
 *  (`--native-no-ladder`, `--native-no-occluders`). With the ladder off the
 *  library is one level per mesh, every error 0 and the header's LADDER flag
 *  clear, which is what v2 described; with the occluders off the `.lodi`'s box
 *  table is empty and the census says so in words. Both default ON. */
void lodgenNativeBegin( const EsmWorld * world, const QString & outDir, NativeModelLoader loader, void * user,
	const QString & meshReportPath = QString(), bool buildLadder = true, bool buildOccluders = true );
bool lodgenNativeActive();
/*! v4/v5 knobs. STICKY and held outside the emitter state, so they may be set
 *  before or after `lodgenNativeBegin` -- both callers set them while parsing.
 *  `libraryNear` false is `--library mnam`, the exact way back to the v3 choice
 *  of level 0; `placementAo` false is `--native-no-placement-ao`, the exact way
 *  back to a `.lodi` with no AO blob and no version 5. */
void lodgenNativeLadderOptions( bool libraryNear, bool ladderFoliage, float silhouetteMin, bool placementAo );
//! v6 (2026-09-18): the per-instance vertex-AO stream; false is `--native-no-vertex-ao`, the .lodi stays v5.
void lodgenNativeVertexAoOption( bool on );
/*! v7 (2026-09-18): the group table and the per-vertex sky stream, one switch
 *  because they are one version word. False is `--lodi-v6`, the exact way back
 *  to a version-6 `.lodi` -- no group table, no sky stream, a 256-byte header
 *  block, byte for byte what the bake wrote before this lane. Default ON. */
void lodgenNativeLodiV7Option( bool on );
/*! v9 (lane HORIZON3, 2026-09-19; the one piece of that lane which lane
 *  HORIZONOUT kept when the baked-horizon route was dropped the same day):
 *  mark the placements a player can scrap at a workshop,
 *  `LODI_INST_SCRAPPABLE`.
 *
 *  It is what lets the runtime far shadow map drop a workshop-owned caster, so
 *  it belongs to the IDENTITY route and not to the baked route it was written
 *  beside. FALSE IS THE EXACT WAY BACK: no bit is written and the `.lodi`
 *  stays at version 7, byte for byte. Default OFF, because it has not been
 *  flown and bungo's rule of 2026-09-17 is that an owed ruling never ships as
 *  a default. */
void lodgenNativeScrappableOption( bool scrappable );

/*! v7 grouping, THE PROXIMITY JOIN (bungo's ruling of 2026-09-19, measured by
 *  lane IDENTPROX). `legacy` restores the shipped rule exactly -- only a
 *  placement whose BASE model path carries an `architecture` component may
 *  join, and it joins when the two WORLD AXIS-ALIGNED boxes are within 16 u on
 *  every axis. False (the default) runs the ruled rule: every NON-TREE
 *  placement with a drawn LOD mesh joins when the two MESHES are within
 *  `gapWorld` world units. `gapWorld <= 0` leaves the default 64 alone.
 *
 *  The gap is a knob and the measure is not: lane IDENTPROX measured that a
 *  BOX gap of any size bridges a street (an elevated highway deck's box hangs
 *  over the buildings under it) while the mesh gap does not until 256 u. */
void lodgenNativeIdentityJoinOption( bool legacy, float gapWorld );
//! One placement (the same (ref, part) arriving from several rings is kept once, first wins).
void lodgenNativeAddPlacement( const NativePlacement & p );
//! One lit vertex of a placement: the emitter averages AO, sky and ground per record (0..1 each).
void lodgenNativeLighting( int chunkX, int chunkY, int dim, int objectIndex, float ao, float sky, float ground );
/*! One PLACEMENT-AO ray for a placement: cast from just above its drawn top,
 *  straight up, against the assembled chunk and the heightfield -- the same
 *  scene and the same reach the chunk's own vertices are cast against. */
void lodgenNativePlacementAo( int chunkX, int chunkY, int dim, int objectIndex, float ao );
//! Build the library from the full census, write both files, print the census line.
bool lodgenNativeWrite( QString * report, QString * error );

/*! WHERE THE SECONDS WENT INSIDE THE LIBRARY BUILD (lane PERF1, 2026-09-17).
 *
 *  The four `stage times:` figures put the whole of `lodgenNativeWrite` into
 *  `meshes`, and on the ruled FO4CS target that ONE call is most of the bake:
 *  measured on the exe of 2026-09-17 07:33, a warm nine-chunk Sanctuary bake is
 *  82.0 s wall of which `meshes` is 69.3 s, and a SIXTEEN-chunk bake of the same
 *  origin moves `meshes` by 3.4 s -- because the library is a function of the
 *  worldspace's full base census and not of the region. A user reading his own
 *  bake could see that it was slow and not where.
 *
 *  Returns `library: census A s, models B s, ladder C s, bases D s, lodo write
 *  E s, instances F s, lodi write G s` for the LAST `lodgenNativeWrite` of this
 *  process, or an EMPTY string when no native pair was written -- a stock bake
 *  keeps the `stage times:` line it always had, to the byte.
 *
 *  It is appended to the existing `stage times:` census line rather than given a
 *  line of its own, so the record gains no SIXTH volatile field: that line is
 *  already declared volatile and already masked WHOLE by both normalisers
 *  (`lodbNormalise` in src/lodbfile.cpp and `normalise` in
 *  tests/spells/lodb_read.py). `ww-volatile-field-law` step 4 -- the count stays
 *  at five and all of it keeps moving together. */
QString lodgenNativeLibrarySplit();

/*! THE LIBRARY-REUSE OFFER (lane PERF1, step 5, `--incremental` only).
 *
 *  The three hexes are the ones the PREVIOUS bake's record wrote
 *  (`loadOrderHashHex`, `pluginCorpusHashHex`, `objectCorpusHashHex`), in the
 *  record's own 16-digit lower-case form. `lodgenNativeWrite` recomputes each
 *  from the world it is about to bake and keeps the previous `.lodo` only when
 *  all three agree, the file is there and it reads back with a full payload
 *  check. Anything else REBUILDS and the census says which test refused.
 *
 *  The switch digest is NOT passed: the incremental driver refuses the whole
 *  run when it moves, so a write that reaches here already has it equal.
 *
 *  The offer is CONSUMED by the next `lodgenNativeWrite` -- arm it once per
 *  write you mean it for. A bake that never arms it is the bake this tree
 *  always did, to the byte. */
struct NativeReuseOffer
{
	bool armed = false;
	QString loadOrderHex;
	QString pluginCorpusHex;
	QString objectCorpusHex;
};
void lodgenNativeOfferLibraryReuse( const NativeReuseOffer & offer );

/*! THE CARD LINK (lane CARDLINK1, 2026-09-24). Call after the card-arrays
 *  pass (`lodgenBuildCardArrays`, which appends `<array .lodm> <layer>` to
 *  every `C` line) and before `lodgenNativeWrite`, with the same chunk list
 *  and the same array file base (`<Objects dir>/<ws>.LodgenCards`). The write
 *  then gives every base in an array its `cardLayer` (set << 11 | layer),
 *  writes `cardCount` and `cardCorpusHash` (the proposed R19 contract, docs
 *  LODGEN_NATIVE_LODO_LODI.md 4.13) and sets `LODI_INST_FORCE_CARD` on each
 *  placement whose ring has no mesh or whose chunk stood it on its card.
 *
 *  Refuses, by name: a missing manifest or array file, a malformed `C` line,
 *  more than 32 arrays, a layer id that is not a formID, a base in two layers,
 *  a `C` line whose layer is not its base's. No `C` line naming an array is
 *  not a refusal: nothing is linked and the bake writes no card. Never
 *  calling this is the exact way back. */
bool lodgenNativeLinkCards( const QStringList & btoPaths, const QString & cardArrayBase, QString * error );

//! Disarm and forget everything.
void lodgenNativeEnd();

/*! AGGREGATE RING-3 IMPOSTORS (bungo 2026-09-11 08:3x, "1 sounds good").
 *
 *  Armed separately from the emitter and OFF by default, because it is a module
 *  and its off value has to be the exact way back: unarmed, `.lodi` is written
 *  at version 3 and is byte for byte the file the same bake wrote before this
 *  lane (CONSTITUTION 10).
 *
 *  `cards` maps a base form id to its card set as the caller already read it,
 *  so this module never opens a card sidecar -- lodgen.cpp's `lodgenCard` stays
 *  the tree's ONE reader of that file. A tree whose base is absent from the map
 *  is refused IN WORDS and keeps its per-tree card.
 *
 *  The aggregation runs inside `lodgenNativeWrite`, after the instance table is
 *  built, because only there are the instance INDICES known -- and the covered
 *  list is indices, not refs, so a runtime suppresses by one array read. The
 *  finished SHEETS are handed back through `lodgenNativeAggregateSets()` for the
 *  caller to write, because the DDS writer and the frame dilation live beside
 *  the rest of the texture bake. */
void lodgenNativeSetAggregate( const LodgenAggOptions & opts,
	const QHash<quint32, LodgenAggCard> & cards );
//! The sets the last `lodgenNativeWrite` built; empty when unarmed.
const QVector<LodgenAggSet> & lodgenNativeAggregateSets();
//! Its census, refusals included.
const LodgenAggStats & lodgenNativeAggregateStats();

/*! THE ORDERING LEAK, closed (lane BAKEPERF1, 2026-09-11).
 *
 *  The accumulator is a process-wide singleton and it is ORDER-SENSITIVE:
 *  `lodgenNativeAddPlacement` gives each new `(ref, part)` the NEXT index in
 *  `arrivals`, so what the library holds depends on the order the chunks
 *  spoke. Fanning the chunk queue over threads would make that order
 *  COMPLETION order, and the `.lodo`/`.lodi` pair would stop being a function
 *  of the input alone.
 *
 *  So a worker does not speak to the accumulator at all. It opens a JOURNAL on
 *  its own thread; every `lodgenNativeAddPlacement` and `lodgenNativeLighting`
 *  call made on that thread is RECORDED instead of applied, and the driver
 *  replays the journals on ONE thread in CHUNK-QUEUE order. The replay makes
 *  the same two calls with the same arguments in the same sequence a serial
 *  run would have made, so the pair is byte-identical by construction rather
 *  than by hope. A thread with no journal open behaves exactly as before. */
struct LodgenNativeJournal;
//! Start recording on THIS thread. Null when the emitter is not armed.
LodgenNativeJournal * lodgenNativeJournalBegin();
//! Stop recording on this thread; the journal stays the caller's.
void lodgenNativeJournalEnd();
//! Apply a journal's events to the accumulator, in the order recorded.
void lodgenNativeJournalReplay( LodgenNativeJournal * j );
//! How many events a journal holds -- a census number, so a silent journal shows.
int lodgenNativeJournalSize( const LodgenNativeJournal * j );
void lodgenNativeJournalDestroy( LodgenNativeJournal * j );

/* ---- THE PER-CHUNK NATIVE CACHE (`.lodj`, lane INCR1, 2026-09-17) -------
 *
 * WHY IT EXISTS. `--native` was on `--incremental`'s whole-region refusal
 * list, which meant the incremental rebake refused every command the FO4CS
 * pipeline is made of. The pair is aggregated from `s.arrivals`, and a chunk
 * that is SKIPPED contributes nothing to it, so the skipped chunk's
 * contribution has to come from somewhere.
 *
 * WHY NOT OUT OF THE PREVIOUS `.lodi` (the route with no new file). Four
 * things the aggregate needs are provably NOT in that file:
 *   1. `NativePlacement::model` -- the path of the mesh the chunk actually
 *      DREW. Never written: `lodifile.cpp` uses `baseName` only in refusal
 *      text (:291, :294). Without it `models[foldPath(p.model)]` cannot be
 *      looked up, so the occluder box cannot be rebuilt.
 *   2. the model-space occluder box. The file stores the WORLD-space,
 *      0.999-shrunk box with the quantised scale already applied, and only
 *      for the at most LODI_OCCLUDERS_PER_CELL = 4 per cell that survived
 *      the writer's cull (`lodifile.cpp:466`). The rest are not in it.
 *   3. `NativePlacement::isTree` -- the aggregate's tree list keys on it
 *      and no `.lodi` instance flag carries it.
 *   4. the lighting ACCUMULATORS. The file carries the 8-bit mean, not the
 *      sum and the count, so two chunks' contributions cannot be recombined.
 * So the clean route is closed, and this is a NEW FILE: a stated divergence
 * for bungo, listed in the bake record's `out` rows and counted by the
 * layout census like every other output.
 *
 * WHAT IT HOLDS. Not the raw journal -- lighting is one event per VERTEX,
 * millions a region -- but the chunk's REDUCTION: every placement it
 * emitted, in emission order, and one row per lit placement with the exact
 * double sums and counts. Replayed in the chunk's own queue position the
 * arrivals come out bit for bit as a full bake's, because a chunk's events
 * are contiguous and its sums start from zero.
 *
 * THE ONE CASE THAT IS NOT EXACT, and it is guarded rather than hoped: an
 * arrival is keyed `(refForm, scolPart)` ACROSS chunks, so a placement two
 * chunks both light has a sum built from both, and `(prev + a1) + a2` is not
 * `prev + (a1 + a2)` in floating point. Those are COUNTED
 * (`lodgenNativeSharedArrivals()`) and an incremental `--native` run refuses
 * when the count is not zero. */

//! Reduce one chunk job's journal and write it. `path` is created; the
//! caller notes it as a layout file and as one of the chunk's outputs.
bool lodgenNativeJournalWriteCache( const LodgenNativeJournal * j, const QString & path,
	const QString & wsEdid, int dim, int cx, int cy, QString * error );

//! Put a cached chunk's contribution back, at the point in the queue order
//! where its job would have been replayed.
bool lodgenNativeReplayCache( const QString & path, QString * error );

//! Placements lit by more than one chunk. Zero is the only value for which a
//! cached replay is bit-exact; the driver refuses otherwise.
int lodgenNativeSharedArrivals();

//! Placements the last cache write or read carried, for the census.
int lodgenNativeCacheLastPlacements();

/*! Read a pair back with every check on, cross-check the pairing (identity,
 *  the two hashes, the load-order hash, every baseId, every base's
 *  mesh-or-card, every instance's drawKey against the library's own
 *  (mesh, material) rank), and print the census: `--native-verify`.
 *
 *  `world`, when given (`--native-verify-corpus`), makes it RECOMPUTE the
 *  object corpus hash, the plugin corpus hash and the load-order hash from the
 *  plugin and refuse on any mismatch, NAMING the file and the field -- the
 *  staleness check bungo asked for on 2026-09-11 10:0x ("Add it"). Without it
 *  the two files are still checked against each other. */
bool lodgenNativeVerify( const QString & lodoPath, const QString & lodiPath, QString * report, QString * error,
	const EsmWorld * world = nullptr );

#endif // NATIVEEMIT_H
