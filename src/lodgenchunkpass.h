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

//! THE CHUNK QUEUE, run over the machine instead of over one core.

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
	//! WHERE THE `.BTO` IS BUILT WHEN IT IS NOT AN OUTPUT. Empty = today's behaviour exactly: the chunk and its
	//! manifest sidecar are written into `meshDir` and stay there.
	QString btoScratchDir;
	QString texDir;                 //!< where the sheets land (created if needed)
	//! Where the panel's preview copies go; empty writes none.
	QString previewDir;
	qint64 texBudgetBytes = qint64( 512 ) << 20;   //!< divided among the workers

	//! THE INCREMENTAL NATIVE CACHE:
	//! `--incremental` hands this pass only the DIRTY chunks, and for the stock target that is the whole story:
	//! a clean chunk's `.BTR`/`.BTO` are already on disk and nothing has to be said about them.
	QVector<LodgenChunkJob> nativeAllJobs;
	std::function<void( const LodgenChunkJob & )> nativeReplayCached;
	std::function<void( const LodgenChunkOutcome &, LodgenNativeJournal * )> nativeJournalSink;

	/* RESUME. Each retired chunk's outcome and raw native journal are kept in `resumeDir`
	 * under `resumeKey`; a rerun with the same key retires those chunks from there (their files still on disk)
	 * instead of building them. Empty = off. */
	QString resumeDir;
	QString resumeKey;
};

//! Run the queue:
//! `retire` is called once per job, on the CALLING thread, in job order.
bool lodgenRunChunkPass( const QVector<LodgenChunkJob> & jobs,
	const LodgenChunkPassOptions & opts,
	const std::function<void( const LodgenChunkOutcome & )> & retire,
	const std::function<bool()> & cancelled,
	qint64 * msMeshes, qint64 * msTextures, QString * error );

//! Census: how many workers the last pass actually used, and how many jobs it ran.
int lodgenLastPassWorkers();
int lodgenLastPassJobs();

//! THE BAKE CENSUS LINE -- one formatter, so the panel and the command line cannot word it differently (the
//! same rule `lodgenStageTimeLine` follows).
QString lodgenBakeCensusLine();

//! THE `.BTO` DISPOSITION, said out loud in the census.
void lodgenSetBtoDisposition( const QString & scratchDir, int built, int dropped, qint64 bytesFreed );

//! WHAT THE TEARDOWN OF A `.BTO` SCRATCH FOLDER DID:
//! Counted from the files on disk, never predicted from the job list: a census field states what is there or it
//! states nothing.
struct LodgenBtoScratchResult
{
	int built = 0;              //!< `.BTO` files the run actually produced
	int dropped = 0;            //!< how many of them were removed again
	int manifests = 0;          //!< sidecars moved to the folder they belong in
	qint64 freed = 0;           //!< the bytes those chunks occupied
	QStringList warnings;
};

//! Move every chunk's manifest sidecar from `scratchDir` into `finalDir`, delete the chunks, remove the folder,
//! and record the disposition for the census line.
LodgenBtoScratchResult lodgenDropBtoScratch( const QStringList & writtenBto,
	const QString & scratchDir, const QString & finalDir );
void lodgenClearBtoDisposition();
QString lodgenBtoDispositionLine();

/* ===== THE BAKE RECORD AND INCREMENTAL REGENERATION ========================
 *
 * This was written inside `cmdLodgen` first;
 * it moved here, unchanged in behaviour, so the
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

//! THE IDENTITY WORD:
//! The switch digest is the TYPED argument vector, so a bare bake on two exes whose DEFAULTS differ carries one
//! digest, and `--incremental` could not see the flip: a default once moved under
//! an unchanged digest and recorded it as an open finding.
constexpr int kLodgenGeneratorRevision = 3;

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
	/*! IDENT1: the contact join and the building occluder. The
	 *  identity dump names them only when ON, so a bake with both off hashes the
	 *  exact words a bake from before the lane did. */
	bool identityJoinContact = false, occluderBuilding = false;
	//! the named-landmark list's digest (lodgenNativeLandmarksDigest), named only with the contact join
	QString landmarksDigest;
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
	//! where the record goes when not `outDir` (`--fo4cs-one-root`); empty = `outDir`
	QString recordRoot;
	QString digestRoot;         //!< the loose root the per-chunk input digest reads
	quint32 worldspace = 0x3CU;
	int     dim = 4;
	int     region[4] = { 0, 0, 0, 0 };
	QString switches;           //!< lodgenSwitchesWithIdentity(...)
	bool    regionProducts = false;
	bool    nativeCache = true; //!< false = `--no-native-cache`: no `.lodj`, and `--native` refuses as before INCR1
	std::function<void( const QString & )> warn;  //!< a cache failure, as it happens
	//! THE RAW CHUNK CACHE.
	bool    rawCache = false;
	// ---- the ledger's ----
	QString ledgerPath, prevRecordDir, lodjDir;
	LodgenLedger prev;
	QVector<LodgenChunkJob> allJobs;
	bool    incremental = false;
	QHash<QString, QStringList> producedFiles;
	int     lodjWritten = 0, lodjReplayed = 0, lodjFailed = 0;
	qint64  lodjPlacements = 0;
	// ---- whole-map incremental ----
	QString rawCacheDir;                //!< `<outDir>/lodgen_chunk_cache` when `rawCache`
	int     movedInputs = 0;            //!< chunks whose input digest moved (Begin)
	int     unknownChunks = 0;          //!< chunks the record never heard of (Begin)
	int     rawCacheLost = 0;           //!< clean chunks rebaked because their raw cache was missing or stale
	int     rawRestored = 0;            //!< clean chunks whose raw files were copied back this run
	/*! region products that no longer hash as the record says (or cannot be
	 *  vouched for: no product rows, a plugin or the load order moved); -1 =
	 *  not checked. Set by lodgenIncrementalCheckProducts. */
	int     productsLost = -1;
	bool    nullRun = false;            //!< nothing moved: no stage ran, the record is carried forward
	//! each chunk key's input digest, computed once and shared by the diff, the record and the raw cache
	mutable QHash<QString, QString> inputsMemo;
	//! raw copies made this run, keyed like producedFiles: {bto sha1 or "-", manifest sha1 or "-"}
	QHash<QString, QStringList> rawStored;
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

/* ---- whole-map incremental FO4CS bakes ---------- */

//! After Begin, on an incremental run: are the previous record's region products still the files it names, and
//! did the plugin list and the load order stay put?
void lodgenIncrementalCheckProducts( LodgenIncrementalRun & run, const EsmWorld & world,
	QStringList * reasons );
//! Right after the chunk pass returns (every file flushed, no post-pass run yet): copy each retired chunk's raw
//! `.BTO` and manifest into the raw cache.
bool lodgenIncrementalStoreRaw( LodgenIncrementalRun & run, const QString & worldEdid,
	const QHash<QString, QString> & retired, QString * error );
/*! The full job-order `.BTO` list for the post-passes: this run's own chunks
 *  where they were rebaked, the raw cache copied back into `btoDir` for every
 *  clean one. False + `error` when a cached file cannot be copied. */
bool lodgenIncrementalRestoreRaw( LodgenIncrementalRun & run, const QString & worldEdid,
	const QString & btoDir, const QHash<QString, QString> & retired, QStringList * writtenBto,
	QString * error );
//! After the record: the key files that make this run's raw copies valid for the next one.
void lodgenIncrementalSealRaw( const LodgenIncrementalRun & run, const QString & worldEdid );
//! a chunk's key as the record and the produced-file list spell it
QString lodgenIncrementalChunkKey( const LodgenIncrementalRun & run, int dim, int cx, int cy );

#endif // LODGENCHUNKPASS_H
