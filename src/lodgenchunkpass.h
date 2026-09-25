/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef LODGENCHUNKPASS_H
#define LODGENCHUNKPASS_H

#include "lodgen.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

class EsmWorld;

/*! THE CHUNK QUEUE, run over the machine instead of over one core.
 *
 *  Both drivers -- the panel (`lodgenmanager.cpp`) and the command line
 *  (`nifcli.cpp`) -- had the same doubly-nested `for (cy) for (cx)` loop, each
 *  building a terrain chunk, its texture sheets and an object chunk. The loop
 *  lives here now (CONSTITUTION 10: what is shared lives in the shared code),
 *  and it fans the jobs over `lodgenThreadCount()` workers.
 *
 *  WHAT MAKES THE OUTPUT A FUNCTION OF THE INPUT AGAIN, not of who finished
 *  first:
 *
 *  * Every worker owns its OWN `EsmWorld` and its OWN `LodgenBakeCaches`.
 *    Neither can be shared: `EsmWorld`'s six lazy caches hand out references
 *    INTO the hash and `ESMFile` decompresses through one shared scratch
 *    buffer, and the bake caches' LRU `delete`s textures another worker may be
 *    reading. A mutex does not fix either. The caches only memoise
 *    deterministic lookups, so a cold cache per worker changes time, never a
 *    value.
 *  * Results RETIRE in job order, on the calling thread, through `retire`.
 *    Everything downstream that cares about order -- `writtenBto` (which the
 *    atlas, the texture arrays, the merge, the far-ring cut and the card
 *    arrays all consume in list order), the `[n] <name>` lines, the panel's
 *    preview splice and its progress bar -- is fed from there and never from a
 *    completion callback.
 *  * The FO4CS-native accumulator is journalled per worker and replayed in job
 *    order (see `lodgenNativeJournalBegin`).
 *  * `NifModel`'s constructor and destructor touch one process-wide map in
 *    `GameManager`; they are serialised under `lodgenNifModelMutex()`.
 *
 *  At `lodgenThreadCount() == 1` the pass runs every job on the calling thread,
 *  in order, on ONE world and ONE cache set -- which is the loop that was here
 *  before, and is what `--threads 1` means.
 */

struct LodgenChunkJob
{
	int dim = 4;
	int cx = 0;
	int cy = 0;
};

//! What one job produced. Handed to `retire` on the calling thread, in job order.
struct LodgenChunkOutcome
{
	int index = 0;                  //!< position in the job list
	int dim = 4, cx = 0, cy = 0;
	bool btrBuilt = false;          //!< the terrain chunk built
	bool btrNoLand = false;         //!< "no LAND" -- a skip, not a failure
	bool btrSaved = false;
	bool btoBuilt = false;
	bool btoSaved = false;
	QString btrError, btoError, texError;
	QString btrPath, btoPath;       //!< where the files went (empty when not written)
	QString manifestPath;
	QString btrPreviewPath, btoPreviewPath;   //!< only when previewDir is set
	/*! This job's recorded native-emitter calls. The pass replays and destroys
	 *  it immediately before `retire`, so a caller never sees a live one; it
	 *  is null at one thread, where the emitter is spoken to directly. */
	struct LodgenNativeJournal * nativeJournal = nullptr;
	int nativeEvents = 0;           //!< how many calls the journal held
};

struct LodgenChunkPassOptions
{
	//! What every worker's own EsmWorld loads; the same string the caller used.
	QString plugins;
	quint32 worldspace = 0x3CU;
	QString worldEdid;              //!< for the file stems

	bool wantBtr = false;           //!< build and write the .BTR terrain chunk
	bool wantBto = false;           //!< build and write the .BTO object chunk
	bool wantTex = false;           //!< bake the chunk's terrain texture sheets

	LodgenTerrainOptions terrain;
	LodgenObjectOptions object;
	LodgenCoverOptions cover;

	QString texDataRoot;            //!< the data root the TEXTURE bake takes
	QString meshDir;                //!< where the .BTR/.BTO land
	/*! WHERE THE `.BTO` IS BUILT WHEN IT IS NOT AN OUTPUT (lane BTOFREE1,
	 *  2026-09-16). Empty = today's behaviour exactly: the chunk and its
	 *  manifest sidecar are written into `meshDir` and stay there. Non-empty =
	 *  the FO4CS target, where the `.BTO` is scaffolding rather than a product:
	 *  the chunk AND its manifest are built here instead, every read-back (the
	 *  texture arrays, the card arrays, the shape merge, the far-ring cut)
	 *  works on them here, and the driver moves the manifests into `meshDir`
	 *  and removes this directory when the post-passes are done.
	 *
	 *  The manifest travels WITH the chunk on purpose: every one of those
	 *  passes opens `<btoPath>.manifest.txt` beside the file it is rewriting
	 *  (`lodgen.cpp` 4836 / 5132 / 12439 / 12869 / 13235 / 13487), so a
	 *  manifest left behind in `meshDir` would be the one they never amended.
	 *  `.BTR` is untouched by this and still lands in `meshDir`. */
	QString btoScratchDir;
	QString texDir;                 //!< where the sheets land (created if needed)
	/*! Where the panel's preview copies go; empty writes none. A preview copy
	 *  is the same document with the chunk's WORLD translation on its root
	 *  (the .BTR half only), saved so the main thread has nothing to do but
	 *  open it -- no NifModel ever crosses a thread. */
	QString previewDir;
	qint64 texBudgetBytes = qint64( 512 ) << 20;   //!< divided among the workers

	/*! THE INCREMENTAL NATIVE CACHE (lane INCR1, 2026-09-17).
	 *
	 *  `--incremental` hands this pass only the DIRTY chunks, and for the
	 *  stock target that is the whole story: a clean chunk's `.BTR`/`.BTO`
	 *  are already on disk and nothing has to be said about them. The FO4CS
	 *  target is not like that. `<ws>.lodo` and `<ws>.lodi` are AGGREGATED
	 *  from every chunk's arrivals, so a chunk that is skipped silently
	 *  deletes its placements from the pair. That is why `--native` was on
	 *  the whole-region refusal list, and why the ruled pipeline could not
	 *  be rebaked incrementally at all.
	 *
	 *  So a skipped chunk speaks from a cache instead of from a build.
	 *  `nativeAllJobs` is the FULL queue in the order a full bake would have
	 *  run it; `jobs` is the dirty subset of it, in the same order. The pass
	 *  walks a cursor over the full queue and calls `nativeReplayCached` for
	 *  every chunk it passes that is not the next dirty one -- so a cached
	 *  chunk speaks at EXACTLY the point its own job would have been retired.
	 *  That is the whole correctness argument: the accumulator hands out
	 *  arrival indices in arrival order (the ORDERING LEAK comment in
	 *  `nativeemit.h`), so out of order is a different `.lodo`.
	 *
	 *  `nativeJournalSink` is handed a BAKED chunk's journal immediately
	 *  before the pass replays it, which is the one moment the chunk's own
	 *  calls exist as data; the driver reduces it and writes the `.lodj`.
	 *  Setting it also forces journalling ON at one thread, where the
	 *  emitter would otherwise be spoken to directly and there would be
	 *  nothing to write.
	 *
	 *  All three unset is today's behaviour, call for call. */
	QVector<LodgenChunkJob> nativeAllJobs;
	std::function<void( const LodgenChunkJob & )> nativeReplayCached;
	std::function<void( const LodgenChunkOutcome &, LodgenNativeJournal * )> nativeJournalSink;
};

/*! Run the queue.
 *
 *  `retire` is called once per job, on the CALLING thread, in job order.
 *  `cancelled` is polled on the calling thread between retires and by each
 *  worker before it picks up a job -- so Cancel still lands between chunks,
 *  as it did.
 *
 *  `msMeshes` / `msTextures` accumulate the two stage times. With more than one
 *  worker they are WALL time for the pass, split between the two stages in
 *  proportion to the work each did; with one worker they are the same sums the
 *  serial loop produced.
 */
bool lodgenRunChunkPass( const QVector<LodgenChunkJob> & jobs,
	const LodgenChunkPassOptions & opts,
	const std::function<void( const LodgenChunkOutcome & )> & retire,
	const std::function<bool()> & cancelled,
	qint64 * msMeshes, qint64 * msTextures, QString * error );

//! Census: how many workers the last pass actually used, and how many jobs it ran.
int lodgenLastPassWorkers();
int lodgenLastPassJobs();

/*! THE BAKE CENSUS LINE -- one formatter, so the panel and the command line
 *  cannot word it differently (the same rule `lodgenStageTimeLine` follows).
 *
 *      bake census: threads 16, chunk jobs 25, chunk workers 16,
 *                   peak working set 4.10 GB (4404019200 bytes)
 *
 *  `threads` is the budget that was asked for, `chunk workers` is what the
 *  queue could actually use (a 4-job queue cannot use 16), so the two
 *  disagreeing is information rather than a contradiction. A run that never
 *  entered the chunk pass reads `chunk workers 0` -- a default that accuses
 *  its own plumbing rather than flattering it. */
QString lodgenBakeCensusLine();

/*! THE `.BTO` DISPOSITION, said out loud in the census (lane BTOFREE1,
 *  2026-09-16). Under the FO4CS target the `.BTO` chunks are scaffolding and
 *  are removed when the post-passes have read them; a bake that hides that
 *  would be a bake whose output nobody can account for.
 *
 *      bto built in scratch E:/.../lodgen_bto_scratch, 9 chunk(s), 9 dropped,
 *      4485434 bytes freed
 *      bto built in the mod folder, 9 chunk(s), 0 dropped, 0 bytes freed
 *      bto n/a (no object pass ran)
 *
 *  `lodgenSetBtoDisposition` with an EMPTY scratch directory says "kept";
 *  with one, "dropped". Never called at all = `n/a`, which is a default that
 *  accuses its own plumbing rather than reporting a comfortable zero. */
void lodgenSetBtoDisposition( const QString & scratchDir, int built, int dropped, qint64 bytesFreed );

/*! WHAT THE TEARDOWN OF A `.BTO` SCRATCH FOLDER DID (lane BTOFREE1).
 *
 *  Counted from the files on disk, never predicted from the job list: a census
 *  field states what is there or it states nothing. `warnings` carries the
 *  human sentences for anything that could not be moved or removed -- the bake
 *  itself is complete either way, so none of them is an error. */
struct LodgenBtoScratchResult
{
	int built = 0;              //!< `.BTO` files the run actually produced
	int dropped = 0;            //!< how many of them were removed again
	int manifests = 0;          //!< sidecars moved to the folder they belong in
	qint64 freed = 0;           //!< the bytes those chunks occupied
	QStringList warnings;
};

/*! Move every chunk's manifest sidecar from `scratchDir` into `finalDir`,
 *  delete the chunks, remove the folder, and record the disposition for the
 *  census line. One implementation for the panel and the command line, because
 *  `lodgen_byte_gate.sh` compares what the two of them leave on disk. */
LodgenBtoScratchResult lodgenDropBtoScratch( const QStringList & writtenBto,
	const QString & scratchDir, const QString & finalDir );
void lodgenClearBtoDisposition();
QString lodgenBtoDispositionLine();

/* ===== THE BAKE RECORD AND INCREMENTAL REGENERATION ========================
 *
 * Lane INCR1 (2026-09-12, cache 2026-09-17) wrote this inside `cmdLodgen`;
 * lane INCRGATE1 (2026-09-24) moved it here, unchanged in behaviour, so the
 * LOD Generation panel's "Rebake only what changed" row runs the SAME diff,
 * the SAME refusals, the SAME per-chunk `.lodj` cache and writes the SAME
 * record as `-no-gui lodgen --incremental`. The command line keeps its words
 * byte for byte: `tests/spells/lodgen_incremental.sh` greps them.
 *
 * The contract is `docs/LODGEN_LEDGER_FORMAT.md`; the record's own text format
 * is `docs/LODGEN_BAKE_RECORD.md` (the reader and writer live in lodgen.cpp). */

//! `<out>/FO4CSLOD/<ws>/<ws>.lodb` under the FO4CS target, `<out>/<ws>.lodb` without it.
QString lodbRecordPath( const QString & outDir, const QString & ws, bool fo4csTarget );
//! the record an incremental run diffs against: the FO4CS spot first, then the stock one; empty = none
QString lodbFindRecord( const QString & dir, const QString & ws );
//! sha1 over the argument vector minus the flags that cannot reach a tracked output (the format doc, section 3)
QString lodgenSwitchDigestOf( const QStringList & argv );

/*! THE IDENTITY WORD (lane INCRGATE1, 2026-09-24).
 *
 *  The switch digest is the TYPED argument vector, so a bare bake on two exes
 *  whose DEFAULTS differ carries one digest, and `--incremental` could not see
 *  the flip: lanes DEFAULTS1 (2026-09-12) and DEFAULTS2 (2026-09-23) both
 *  moved a default under an unchanged digest and recorded it as an open
 *  finding. The identity word closes it. It is `gen<N>:<sha1>` over one
 *  `key=value` line per EFFECTIVE setting -- the value the bake actually used,
 *  whether it was typed or defaulted -- plus the generator revision below.
 *  The record's `switches` line is then sha1( argv digest, identity word ), so
 *  a default that flips moves the word, moves `switches`, and the incremental
 *  run refuses with the switches reason instead of keeping yesterday's chunks.
 *
 *  Paths, thread counts and progress hooks are NOT in the dump, for the reason
 *  they are not in the argv digest: none of them can reach a tracked byte.
 *
 *  The REVISION is the manual half: bump it when a change moves output bytes
 *  with no setting moving (a new rule, a bug fix in a writer). The dump cannot
 *  see a constant that lives inside lodgen.cpp, and this number is how a lane
 *  says so. */
constexpr int kLodgenGeneratorRevision = 1;

/*! What the front end adds to the pass's own options: the whole-region steps,
 *  the far-ring cut, the pyramid and the native modules. Every field is set by
 *  the caller; the initialisers only keep a forgotten one deterministic. */
struct LodgenIdentityExtras
{
	bool atlas = false, arrays = false, merge = true, atlasBc1 = false;
	bool keepBto = false, texFromVt = false;
	LodgenSimplifyOptions simplify;
	LodgenVtOptions vt;
	int vtBtr = 0;
	bool nativeLadder = false, nativeOccluders = true, libraryNear = false, ladderFoliage = false;
	float silhouetteMin = 0.0f;
	bool placementAo = false, vertexAo = false, lodiV7 = false, scrappable = false;
	bool identityJoinLegacy = false;
	float identityJoinGap = 0.0f;
	bool aggregate = false;
	int aggMin = 0, aggTile = 0, aggViews = 0;
	QStringList more;           //!< further `key=value` lines a front end owns (sorted before use)
};

//! one `key=value` line per effective setting, in a fixed order; the first line is `generator=<N>`
QStringList lodgenIdentityDump( const LodgenChunkPassOptions & pass, const LodgenIdentityExtras & x );
//! `gen<N>:<sha1 hex>` over the dump
QString lodgenIdentityWord( const QStringList & dump );
//! the record's `switches`: sha1 over the argv digest and the identity word
QString lodgenSwitchesWithIdentity( const QString & argvDigest, const QString & identityWord );

//! Why an incremental run did not start.
enum class LodgenIncrementalVerdict
{
	Go,                 //!< run `jobs` (filtered when `incremental` is set)
	NoRecord,           //!< nothing to diff against (the command line refuses; the panel bakes whole)
	Shape,              //!< the record covers another worldspace, chunk size or region
	Switches,           //!< the switches or the identity word moved
	RegionProducts,     //!< atlas / arrays / impostors: one product out of the whole region
	NativeNoCache,      //!< the FO4CS pair without the per-chunk cache
};

/*! One bake's ledger state, from the diff to the record. The caller fills the
 *  first block; everything after it is the ledger's. It must outlive the chunk
 *  pass: the cache hooks armed on the pass capture it by reference. */
struct LodgenIncrementalRun
{
	// ---- the caller's ----
	QString fromDir;            //!< where the previous record is; empty = a full bake (the record is still written)
	bool    requireRecord = true; //!< false = no record yet is a whole bake, not a refusal (the panel)
	QString outDir;             //!< where this bake's outputs and record go
	QString nativeDir;          //!< the FO4CS target's root; empty = the stock target
	//! where the record goes when not `outDir` (`--fo4cs-one-root`, lane BAKE1); empty = `outDir`
	QString recordRoot;
	QString digestRoot;         //!< the loose root the per-chunk input digest reads
	quint32 worldspace = 0x3CU;
	int     dim = 4;
	int     region[4] = { 0, 0, 0, 0 };
	QString switches;           //!< lodgenSwitchesWithIdentity(...)
	bool    regionProducts = false;
	bool    nativeCache = true; //!< false = `--no-native-cache`: no `.lodj`, and `--native` refuses as before INCR1
	std::function<void( const QString & )> warn;  //!< a cache failure, as it happens
	// ---- the ledger's ----
	QString ledgerPath, prevRecordDir, lodjDir;
	LodgenLedger prev;
	QVector<LodgenChunkJob> allJobs;
	bool    incremental = false;
	QHash<QString, QStringList> producedFiles;
	int     lodjWritten = 0, lodjReplayed = 0, lodjFailed = 0;
	qint64  lodjPlacements = 0;
};

/*! THE DIFF. Sets `ledgerPath` and `allJobs`; with `fromDir` set it reads the
 *  previous record, runs the refusals in INCR1's order, and on Go replaces
 *  `jobs` with the dirty ones (widened by one cell, and every chunk with no
 *  `.lodj` added back) and puts the `incremental:` census line in `census`
 *  and up to eight per-chunk reasons in `reasons`. `detail` carries the read
 *  error of a NoRecord and the record path of a Shape refusal. */
LodgenIncrementalVerdict lodgenIncrementalBegin( LodgenIncrementalRun & run, const EsmWorld & world,
	QVector<LodgenChunkJob> & jobs, QString * census, QStringList * reasons, QString * detail );
//! the refusal, as the two lines the command line has always printed (reason, way forward)
QStringList lodgenIncrementalRefusal( LodgenIncrementalVerdict v, const LodgenIncrementalRun & run,
	const QString & detail );

//! arm the per-chunk `.lodj` cache on the pass (the FO4CS target with the cache on only)
void lodgenIncrementalArmCache( LodgenIncrementalRun & run, const QString & worldEdid,
	LodgenChunkPassOptions & pass );
//! the retire hook: what one retired chunk put on disk, for the record
void lodgenIncrementalNoteRetired( LodgenIncrementalRun & run, const LodgenChunkPassOptions & pass,
	const LodgenChunkOutcome & r );
//! the `native cache:` census line; empty when the cache was not armed
QString lodgenIncrementalCacheCensus( const LodgenIncrementalRun & run );
//! empty to go on; otherwise the refusal lines (a cache failure, or an arrival lit by two chunks)
QStringList lodgenIncrementalCacheRefusal( const LodgenIncrementalRun & run );
//! PERF1's library-reuse offer from the previous record's hashes (incremental runs only)
void lodgenIncrementalOfferReuse( const LodgenIncrementalRun & run );
/*! THE RECORD, written LAST (after every file is closed and every census line
 *  printed). `warnings` gets what could not be read; `readBack` the
 *  `bake-record:` line, parsed off the file that was just written. */
bool lodgenIncrementalWriteRecord( const LodgenIncrementalRun & run, const EsmWorld & world,
	const QStringList & switchTokens, const QStringList & resourceStack,
	QStringList * warnings, QString * readBack );

#endif // LODGENCHUNKPASS_H
