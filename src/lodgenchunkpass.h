/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef LODGENCHUNKPASS_H
#define LODGENCHUNKPASS_H

#include "lodgen.h"

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

#endif // LODGENCHUNKPASS_H
