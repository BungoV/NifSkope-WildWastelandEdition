/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "lodgenchunkpass.h"

#include "esmdata.h"
#include "lodgenparallel.h"
#include "lodbfile.h"
#include "lodgenlayout.h"
#include "lodofile.h"
#include "model/nifmodel.h"
#include "nativeemit.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QHash>
#include <QSaveFile>
#include <QSet>
#include <QThread>

#include <atomic>
#include <memory>
#include <vector>

namespace
{

int g_lastWorkers = 0;
int g_lastJobs = 0;

/*! One worker's world. Everything in here is private to one thread for the
 *  length of the pass: the plugin reader with its six lazy caches (which hand
 *  out references INTO themselves), and the texture/grass-tint caches with
 *  their LRU (whose evictor deletes textures another worker could be reading).
 *  Neither survives being shared, with or without a mutex. */
struct Worker
{
	std::unique_ptr<EsmWorld> world;
	LodgenBakeCaches * caches = nullptr;
	qint64 msMeshes = 0, msTextures = 0;

	~Worker()
	{
		if ( caches )
			lodgenDestroyBakeCaches( caches );
	}
};

//! `<edid>.<dim>.<cx>.<cy>` -- the vanilla stem both drivers already built.
QString chunkStem( const QString & edid, int dim, int cx, int cy )
{
	return QString( "%1.%2.%3.%4" ).arg( edid ).arg( dim ).arg( cx ).arg( cy );
}

/*! Serialise a document exactly as BaseModel::saveToFile does before it opens
 *  a file -- through a QBuffer -- so the bytes handed to the writer thread are
 *  the bytes an inline save would have written. */
bool serialise( const NifModel & nif, QByteArray & out )
{
	QBuffer buf( &out );
	if ( !buf.open( QIODevice::WriteOnly ) )
		return false;
	const bool ok = nif.save( buf );
	buf.close();
	return ok;
}

/*! Write, through the queue when there is one. Without a queue it is
 *  QSaveFile with the direct-write fallback, which is what
 *  BaseModel::saveToFile does -- so the file lands the same way either way. */
bool writeBytes( const QString & path, const QByteArray & bytes, LodgenWriter * writer )
{
	if ( writer ) {
		writer->push( path, bytes );
		return true;            // the queue reports its failures at finish()
	}
	QSaveFile f( path );
	f.setDirectWriteFallback( true );
	if ( !f.open( QIODevice::WriteOnly ) )
		return false;
	if ( f.write( bytes ) != bytes.size() ) {
		f.cancelWriting();
		f.commit();
		return false;
	}
	return f.commit();
}

/*! Build one job into its outcome. Runs on a worker thread, or -- at one
 *  thread -- on the caller's, which is the loop this replaced. */
void runJob( const LodgenChunkJob & job, int index, const LodgenChunkPassOptions & opts,
	Worker & w, LodgenWriter * writer, bool journalNative, LodgenChunkOutcome & out )
{
	out.index = index;
	out.dim = job.dim;
	out.cx = job.cx;
	out.cy = job.cy;
	const QString stem = chunkStem( opts.worldEdid, job.dim, job.cx, job.cy );
	/* THE RING IS THE JOB'S, not the option block's. A panel run over "all
	 * rings" mixes 4/8/16/32 in one queue and the builders read `dim` off
	 * their own options, so each job takes its own copy. */
	LodgenTerrainOptions terrain = opts.terrain;
	terrain.dim = job.dim;
	LodgenObjectOptions object = opts.object;
	object.dim = job.dim;

	/* THE NATIVE ORDERING. One journal PER JOB, opened on this thread and
	 * closed before the job returns, so the driver can replay it at the moment
	 * this job retires -- which puts the accumulator's arrivals in exactly the
	 * sequence a serial run would have made. At one thread no journal is
	 * opened at all and the emitter is spoken to directly, as before. */
	LodgenNativeJournal * journal = journalNative ? lodgenNativeJournalBegin() : nullptr;

	if ( opts.wantBtr ) {
		NifModel nif;
		QString cerr;
		QElapsedTimer tMesh;
		tMesh.start();
		out.btrBuilt = lodgenBuildTerrainChunk( &nif, *w.world, job.cx, job.cy, terrain, &cerr );
		w.msMeshes += tMesh.elapsed();
		if ( !out.btrBuilt ) {
			out.btrError = cerr;
			out.btrNoLand = cerr.startsWith( QLatin1String( "no LAND" ) );
		} else {
			const QString path = opts.meshDir + QStringLiteral( "/" ) + stem + QStringLiteral( ".BTR" );
			QByteArray bytes;
			if ( serialise( nif, bytes ) ) {
				/* The preview copy carries the chunk's WORLD translation on
				 * its root -- what the panel used to put on the live document
				 * before saving it and then undo. Written here so that no
				 * NifModel ever crosses a thread. */
				if ( !opts.previewDir.isEmpty() ) {
					const QModelIndex root = nif.getBlockIndex( 0 );
					nif.set<Vector3>( root, "Translation",
						Vector3( float( job.cx ) * 4096.0f, float( job.cy ) * 4096.0f, 0.0f ) );
					QByteArray pv;
					if ( serialise( nif, pv ) ) {
						const QString pp = opts.previewDir + QStringLiteral( "/lodgen_preview_" )
							+ stem + QStringLiteral( "_btr.nif" );
						if ( writeBytes( pp, pv, writer ) )
							out.btrPreviewPath = pp;
					}
					nif.set<Vector3>( root, "Translation", Vector3() );
				}
				out.btrSaved = writeBytes( path, bytes, writer );
				if ( out.btrSaved )
					out.btrPath = path;
			}
			if ( opts.wantTex ) {
				QElapsedTimer tTex;
				tTex.start();
				QString terr;
				if ( !lodgenBakeTerrainTextures( *w.world, job.cx, job.cy, job.dim, opts.texDataRoot,
					opts.texDir, opts.cover, w.caches, &terr ) )
					out.texError = terr;
				w.msTextures += tTex.elapsed();
			}
		}
	}

	if ( opts.wantBto ) {
		NifModel nif;
		QString manifest, cerr;
		QElapsedTimer tObj;
		tObj.start();
		out.btoBuilt = lodgenBuildObjectChunk( &nif, *w.world, job.cx, job.cy, object, &manifest, &cerr );
		w.msMeshes += tObj.elapsed();
		if ( !out.btoBuilt ) {
			out.btoError = cerr;
		} else {
			/* The scratch directory when the `.BTO` is scaffolding, `meshDir`
			 * when it is an output (lane BTOFREE1). One line, and it decides
			 * the manifest with it, because every post-pass opens the manifest
			 * from the chunk's own path. */
			const QString btoDir = opts.btoScratchDir.isEmpty() ? opts.meshDir : opts.btoScratchDir;
			const QString path = btoDir + QStringLiteral( "/" ) + stem + QStringLiteral( ".BTO" );
			QByteArray bytes;
			if ( serialise( nif, bytes ) ) {
				if ( !opts.previewDir.isEmpty() ) {
					// BTO shapes self-place: the preview copy IS the document
					const QString pp = opts.previewDir + QStringLiteral( "/lodgen_preview_" )
						+ stem + QStringLiteral( "_bto.nif" );
					if ( writeBytes( pp, bytes, writer ) )
						out.btoPreviewPath = pp;
				}
				out.btoSaved = writeBytes( path, bytes, writer );
				if ( out.btoSaved ) {
					out.btoPath = path;
					/* THE MANIFEST IS A SIDECAR, not chunk data, and is written
					 * whatever the identity flag says (lane DEFAULTS1,
					 * 2026-09-12): the texture arrays, the impostor card
					 * arrays, the shape merge and the far-ring cut all read it
					 * back, and gating it on identity took them with it. */
					{
						const QString mp = path + QStringLiteral( ".manifest.txt" );
						if ( writeBytes( mp, manifest.toUtf8(), writer ) )
							out.manifestPath = mp;
					}
				}
			}
		}
	}

	if ( journal ) {
		lodgenNativeJournalEnd();
		out.nativeJournal = journal;
		out.nativeEvents = lodgenNativeJournalSize( journal );
	}
}

/*! A worker thread: take the next job nobody has taken, build it, mark it
 *  done. Completion order is not job order; the driver retires from the
 *  results array by index. */
class ChunkThread final : public QThread
{
public:
	ChunkThread( Worker * w, const QVector<LodgenChunkJob> * jobs, const LodgenChunkPassOptions * o,
		LodgenWriter * writer, std::atomic<int> * cursor, LodgenChunkOutcome * results,
		std::atomic<int> * flags, const std::function<bool()> * cancel )
		: worker( w ), jobList( jobs ), options( o ), out( writer ),
		  next( cursor ), res( results ), doneFlags( flags ), cancelled( cancel ) {}

protected:
	void run() override
	{
		/* This thread IS one of the generator's workers: the BC encoders it
		 * reaches must not fan out again underneath it. */
		LodgenWorkerScope workerScope;
		for ( ;; ) {
			const int i = next->fetch_add( 1 );
			if ( i >= jobList->size() )
				break;
			if ( cancelled && *cancelled && ( *cancelled )() ) {
				doneFlags[i].store( 2 );      // Cancel still lands between chunks
				continue;
			}
			runJob( jobList->at( i ), i, *options, *worker, out, true, res[i] );
			doneFlags[i].store( 1 );
		}
	}

private:
	Worker * worker;
	const QVector<LodgenChunkJob> * jobList;
	const LodgenChunkPassOptions * options;
	LodgenWriter * out;
	std::atomic<int> * next;
	LodgenChunkOutcome * res;
	std::atomic<int> * doneFlags;
	const std::function<bool()> * cancelled;
};

} // namespace

int lodgenLastPassWorkers() { return g_lastWorkers; }
int lodgenLastPassJobs() { return g_lastJobs; }

/* THE `.BTO` DISPOSITION (lane BTOFREE1, 2026-09-16). Statics because the
 * census line is one shared formatter with no argument list, exactly as the
 * thread counts above it are. `g_btoState` is a TRI-STATE and its default
 * accuses its own plumbing: a run that never entered the object pass reads
 * `n/a`, not `0 dropped`, so "nothing was dropped" and "nothing was asked"
 * can never be read as the same sentence (CONSTITUTION 4, the three rules of
 * 2026-09-04 21:33). */
static QString g_btoWhere;                 // the scratch directory, or empty
static int     g_btoState   = 0;           // 0 n/a, 1 kept in the mod folder, 2 dropped
static int     g_btoChunks  = 0;           // how many .BTO files the run built
static int     g_btoDropped = 0;           // how many of them were removed
static qint64  g_btoFreed   = 0;           // their total size in bytes

void lodgenSetBtoDisposition( const QString & scratchDir, int built, int dropped, qint64 bytesFreed )
{
	g_btoWhere   = scratchDir;
	g_btoState   = scratchDir.isEmpty() ? 1 : 2;
	g_btoChunks  = built;
	g_btoDropped = dropped;
	g_btoFreed   = bytesFreed;
}

void lodgenClearBtoDisposition()
{
	g_btoWhere.clear();
	g_btoState = 0;
	g_btoChunks = g_btoDropped = 0;
	g_btoFreed = 0;
}

LodgenBtoScratchResult lodgenDropBtoScratch( const QStringList & writtenBto,
	const QString & scratchDir, const QString & finalDir )
{
	LodgenBtoScratchResult r;
	if ( scratchDir.isEmpty() )
		return r;
	for ( const QString & p : writtenBto ) {
		const QFileInfo fi( p );
		if ( !fi.exists() )
			continue;
		r.built++;
		r.freed += fi.size();
		/* THE SIDECAR TRAVELS WITH THE CHUNK AND STAYS BEHIND IT. Five passes
		 * open `<chunk>.BTO.manifest.txt` beside the chunk they are reading
		 * (lodgen.cpp), so it has to be in the scratch folder while they run;
		 * bungo's call is to keep the manifest, so it has to be in the mod
		 * folder when they are done. Both are true because it moves here. */
		const QString man = p + QStringLiteral( ".manifest.txt" );
		if ( QFileInfo::exists( man ) ) {
			const QString dst = finalDir + QStringLiteral( "/" ) + fi.fileName()
				+ QStringLiteral( ".manifest.txt" );
			QFile::remove( dst );
			if ( QFile::rename( man, dst ) ) {
				r.manifests++;
				/* `finalDir` is the FO4CS worldspace folder from today (lane
				 * LAYOUT1, 2026-09-16); the clause reads the root back from
				 * the sidecar's own landing place. */
				lodgenNoteLayoutFile( dst );
			} else
				r.warnings.append( QStringLiteral( "warning: %1 could not be moved out of "
					"the scratch folder" ).arg( QFileInfo( man ).fileName() ) );
		}
		if ( QFile::remove( p ) )
			r.dropped++;
	}
	if ( !QDir( scratchDir ).removeRecursively() )
		r.warnings.append( QStringLiteral( "warning: the .BTO scratch folder %1 could not be "
			"removed; the bake itself is complete" ).arg( scratchDir ) );
	lodgenSetBtoDisposition( scratchDir, r.built, r.dropped, r.freed );
	return r;
}

QString lodgenBtoDispositionLine()
{
	if ( g_btoState == 0 )
		return QStringLiteral( "bto n/a (no object pass ran)" );
	if ( g_btoState == 1 )
		return QStringLiteral( "bto built in the mod folder, %1 chunk(s), 0 dropped, 0 bytes freed" )
			.arg( g_btoChunks );
	return QStringLiteral( "bto built in scratch %1, %2 chunk(s), %3 dropped, %4 bytes freed" )
		.arg( g_btoWhere ).arg( g_btoChunks ).arg( g_btoDropped ).arg( g_btoFreed );
}

QString lodgenBakeCensusLine()
{
	/* `bound by` (lane RESUME3, 2026-09-11): which limit decided the chunk
	 * worker count -- "asked" when --chunk-threads named a number, "cores" when
	 * the machine did, "memory" when free RAM did. NIFPARSE1 computed the word
	 * and nothing read it; a run capped by memory used to read as a slow run. */
	return QStringLiteral( "bake census: threads %1, chunk threads %2 bound by %3, "
		"chunk jobs %4, chunk workers %5, %6" )
		.arg( lodgenThreadCount() ).arg( lodgenChunkThreadCount() )
		.arg( lodgenChunkThreadBoundBy() )
		.arg( g_lastJobs ).arg( g_lastWorkers ).arg( lodgenPeakWorkingSetLine() )
		+ QStringLiteral( ", " ) + lodgenBtoDispositionLine()
		/* `layout` (lane LAYOUT1, 2026-09-16, bungo's 19:3x ruling): the one
		 * root every FO4CS-target file went under, read back from the paths the
		 * writers actually opened and never from the setting that produced
		 * them. `n/a` on a run that wrote none -- a stock bake says that. */
		+ QStringLiteral( ", " ) + lodgenLayoutCensusLine();
}

bool lodgenRunChunkPass( const QVector<LodgenChunkJob> & jobs,
	const LodgenChunkPassOptions & opts,
	const std::function<void( const LodgenChunkOutcome & )> & retire,
	const std::function<bool()> & cancelled,
	qint64 * msMeshes, qint64 * msTextures, QString * error )
{
	g_lastJobs = jobs.size();
	g_lastWorkers = 0;
	if ( jobs.isEmpty() ) {
		/* NOTHING DIRTY. For the stock target that really is nothing to do.
		 * For the FO4CS pair it is the opposite: every chunk of the region
		 * has to speak from its cache, or the rewritten `.lodo`/`.lodi` would
		 * be EMPTY -- the worst possible shape of this bug, because both
		 * files would still be written and still be valid. */
		if ( opts.nativeReplayCached ) {
			for ( const LodgenChunkJob & cj : opts.nativeAllJobs )
				opts.nativeReplayCached( cj );
		}
		return true;
	}

	const int threads = qBound( 1, lodgenChunkThreadCount(), int( jobs.size() ) );
	g_lastWorkers = threads;

	/* WARM the process-wide lazy singletons on THIS thread before anything
	 * fans out -- the resource-stack index and the mesh archive index are
	 * "build once on first use" statics, and first use is the only moment
	 * they can race. */
	lodgenWarmSharedIndices();

	QElapsedTimer wall;
	wall.start();

	std::vector<std::unique_ptr<Worker>> workers;
	workers.reserve( size_t( threads ) );
	for ( int i = 0; i < threads; i++ ) {
		auto w = std::make_unique<Worker>();
		w->world = std::make_unique<EsmWorld>();
		QString werr;
		if ( !w->world->load( opts.plugins, opts.worldspace, &werr ) ) {
			if ( error )
				*error = QStringLiteral( "worker %1: %2" ).arg( i ).arg( werr );
			return false;
		}
		/* EVERY worker gets the WHOLE budget, not a share of it. Dividing it
		 * was this lane's own idea and it is the one behavioural difference
		 * between one thread and many beyond scheduling: at nine workers the
		 * share hit its 64 MB floor, which holds three landscape diffuses and
		 * thrashes. The cost is memory -- 16 x 512 MB worst case against 31 GB
		 * -- and the census line prints the peak so it is never a guess. */
		w->caches = lodgenCreateBakeCaches( opts.texBudgetBytes );
		workers.push_back( std::move( w ) );
	}

	std::vector<LodgenChunkOutcome> results( size_t( jobs.size() ) );

	/* EMPTY-FOLDER HYGIENE (lane LAYOUT1, 2026-09-16). `meshDir` is the
	 * LEGACY chunk folder, `meshes/terrain/<ws>/`. Under the FO4CS target
	 * nothing lands in it any more -- the `.BTO` is scaffolding in a scratch
	 * folder, its manifest moved under FO4CSLOD/<ws>/ with the rest -- so the
	 * folder is created only when a file is actually going to be written
	 * into it. A bake that fills nothing leaves nothing behind. */
	if ( opts.wantBtr || opts.btoScratchDir.isEmpty() )
		QDir().mkpath( opts.meshDir );
	if ( opts.wantTex && !opts.texDir.isEmpty() )
		QDir().mkpath( opts.texDir );

	bool ok = true;

	/* THE CURSOR OVER THE FULL QUEUE (lane INCR1). `jobs` is a subsequence
	 * of `opts.nativeAllJobs` in the same order, so one forward cursor puts
	 * every skipped chunk back in its own place; no search, no sorting, and
	 * a queue that is not a subsequence simply drains at the tail rather
	 * than replaying anything twice. */
	int nativeCursor = 0;
	const bool haveNativeCache = !opts.nativeAllJobs.isEmpty()
						&& bool( opts.nativeReplayCached );
	auto sameChunk = []( const LodgenChunkJob & a, const LodgenChunkJob & b ) {
		return a.dim == b.dim && a.cx == b.cx && a.cy == b.cy;
	};
	auto replayCachedBefore = [&]( const LodgenChunkJob & job ) {
		if ( !haveNativeCache )
			return;
		while ( nativeCursor < opts.nativeAllJobs.size()
				&& !sameChunk( opts.nativeAllJobs.at( nativeCursor ), job ) ) {
			opts.nativeReplayCached( opts.nativeAllJobs.at( nativeCursor ) );
			nativeCursor++;
		}
		if ( nativeCursor < opts.nativeAllJobs.size() )
			nativeCursor++;         // this chunk speaks for itself
	};
	auto replayCachedTail = [&]() {
		while ( haveNativeCache && nativeCursor < opts.nativeAllJobs.size() ) {
			opts.nativeReplayCached( opts.nativeAllJobs.at( nativeCursor ) );
			nativeCursor++;
		}
	};

	if ( threads == 1 ) {
		/* THE WAY BACK (`--threads 1`): one world, one cache set, one job at a
		 * time, in order, written inline, speaking to the native accumulator
		 * directly. This is the loop both drivers had. */
		Worker & w = *workers[0];
		/* Journalling at ONE thread exists only for the cache: without a sink
		 * the serial loop speaks to the emitter directly, which is what it has
		 * always done and the reason `--threads 1` is the way back. With a
		 * sink the calls have to exist as data for one moment so they can be
		 * reduced and written, and they are replayed immediately after, on
		 * this same thread, in this same place. */
		const bool journalNative = bool( opts.nativeJournalSink );
		for ( int i = 0; i < jobs.size(); i++ ) {
			if ( cancelled && cancelled() )
				break;
			replayCachedBefore( jobs.at( i ) );
			runJob( jobs.at( i ), i, opts, w, nullptr, journalNative, results[size_t( i )] );
			LodgenChunkOutcome & r1 = results[size_t( i )];
			if ( r1.nativeJournal ) {
				opts.nativeJournalSink( r1, r1.nativeJournal );
				lodgenNativeJournalReplay( r1.nativeJournal );
				lodgenNativeJournalDestroy( r1.nativeJournal );
				r1.nativeJournal = nullptr;
			}
			retire( r1 );
		}
		replayCachedTail();
		if ( msMeshes )
			*msMeshes += w.msMeshes;
		if ( msTextures )
			*msTextures += w.msTextures;
		return ok;
	}

	std::vector<std::atomic<int>> flags( size_t( jobs.size() ) );
	for ( auto & f : flags )
		f.store( 0 );
	std::atomic<int> cursor{ 0 };

	/* The writer thread with its bounded queue: a worker hands over bytes and
	 * goes back to building. Nothing returns before finish() has drained it. */
	LodgenWriter writer;

	QVector<ChunkThread *> ths;
	for ( int i = 0; i < threads; i++ ) {
		ChunkThread * t = new ChunkThread( workers[size_t( i )].get(), &jobs, &opts, &writer,
			&cursor, results.data(), flags.data(), &cancelled );
		ths.append( t );
		t->start();
	}

	/* RETIRE IN JOB ORDER, on the calling thread. Everything downstream that
	 * cares about order -- `writtenBto` (consumed in list order by the atlas,
	 * the texture arrays, the merge, the far-ring cut and the card arrays),
	 * the `[n] <name>` lines, the panel's preview splice and its progress bar,
	 * and the native accumulator -- is fed from here, so completion order
	 * cannot reach an output file. The caller's `retire` is free to pump the
	 * event loop; the workers do not touch it. */
	for ( int i = 0; i < jobs.size(); i++ ) {
		while ( flags[size_t( i )].load() == 0 )
			QThread::msleep( 1 );
		if ( flags[size_t( i )].load() == 2 )
			continue;                   // cancelled before this job was picked up
		LodgenChunkOutcome & r = results[size_t( i )];
		replayCachedBefore( jobs.at( i ) );
		if ( r.nativeJournal ) {
			if ( opts.nativeJournalSink )
				opts.nativeJournalSink( r, r.nativeJournal );
			lodgenNativeJournalReplay( r.nativeJournal );
			lodgenNativeJournalDestroy( r.nativeJournal );
			r.nativeJournal = nullptr;
		}
		retire( r );
	}
	replayCachedTail();

	for ( ChunkThread * t : ths ) {
		t->wait();
		delete t;
	}
	// a cancelled run can leave journals nobody retired
	for ( LodgenChunkOutcome & r : results ) {
		if ( r.nativeJournal ) {
			lodgenNativeJournalDestroy( r.nativeJournal );
			r.nativeJournal = nullptr;
		}
	}

	if ( !writer.finish() ) {
		ok = false;
		if ( error )
			*error = QStringLiteral( "%1 file(s) failed to write, first: %2" )
				.arg( writer.failures().size() ).arg( writer.failures().value( 0 ) );
	}

	qint64 sumMesh = 0, sumTex = 0;
	for ( auto & w : workers ) {
		sumMesh += w->msMeshes;
		sumTex += w->msTextures;
	}
	/* THE STAGE TIMES under a fan-out. The per-chunk sums add up to the CPU
	 * time every worker spent, which is not what "how long did the bake take"
	 * means. The pass's own WALL time is reported instead, split between the
	 * two stages in the proportion the work itself had. */
	const qint64 wallMs = wall.elapsed();
	const qint64 sum = sumMesh + sumTex;
	if ( msMeshes )
		*msMeshes += sum > 0 ? ( wallMs * sumMesh ) / sum : wallMs;
	if ( msTextures )
		*msTextures += sum > 0 ? ( wallMs * sumTex ) / sum : 0;

	return ok;
}


/* ===== THE BAKE RECORD AND INCREMENTAL REGENERATION ========================
 *
 * Moved out of `cmdLodgen` (src/nifcli.cpp) by lane INCRGATE1, 2026-09-24, so
 * the panel runs the same ledger. The comments that explain each rule moved
 * with it; the behaviour did not change, and `lodgen_incremental.sh` is the
 * gate that says so. */

/*! WHERE THE RECORD GOES, composed in ONE place (lane BAKEREC1, 2026-09-17).
 *
 *  Under the FO4CS target it sits with the files it describes, at
 *  `<mod>/FO4CSLOD/<ws>/<ws>.lodb` -- lane LAYOUT1's ruling, written down in
 *  `docs/LODGEN_LEDGER_FORMAT.md` section 1 and in `src/lodgenlayout.h`.
 *  Without that target there is no FO4CSLOD root at all, so it keeps its
 *  version-1 home beside the `.BTR`/`.BTO` files it is a record OF.
 *
 *  THE STOCK TARGET STILL WRITES ONE, and that is a deliberate divergence from
 *  this lane's brief. `--incremental` has refused without a record since lane
 *  INCR1 shipped it; making the record FO4CS-only would have retired the
 *  incremental path for every stock bake, silently, on the way to adding a
 *  feature. The stock ENGINE output -- `.BTR`, `.BTO`, the chunk sheets -- is
 *  untouched either way, which is what the byte-identity gates actually pin;
 *  each of them already excuses the `.lodb` by name and compares it field by
 *  field instead. */
QString lodbRecordPath( const QString & outDir, const QString & ws, bool fo4csTarget )
{
	return fo4csTarget
		? ( lodgenFo4csWorldDir( outDir, ws ) + QChar( '/' ) + ws + QStringLiteral( ".lodb" ) )
		: ( outDir + QChar( '/' ) + ws + QStringLiteral( ".lodb" ) );
}

/*! The record an `--incremental` run must diff against, FOUND rather than
 *  assumed: the previous bake may have been a stock one or an FO4CS one, and
 *  this run has no way to know which. Both spots are looked at, the FO4CS one
 *  first. Empty when neither holds a file, which is the `NO_LEDGER` refusal. */
QString lodbFindRecord( const QString & dir, const QString & ws )
{
	const QString a = lodbRecordPath( dir, ws, true );
	if ( QFileInfo( a ).isFile() )
		return a;
	const QString b = lodbRecordPath( dir, ws, false );
	if ( QFileInfo( b ).isFile() )
		return b;
	return QString();
}

/*! Flags whose TOKEN AND VALUE are both dropped from the digest, because
 *  neither can make a TRACKED CHUNK OUTPUT stale. That is the whole test, and
 *  it is narrower than "cannot reach an output byte": the ledger tracks the
 *  per-chunk .BTO/.BTR/.DDS files it lists and nothing else, so a flag that
 *  writes a SEPARATE product into a SEPARATE directory is not its business.
 *
 *  A flag is here if it names WHERE files go, or HOW MANY threads carry them,
 *  or WHICH FILE an asset is read from -- and in that last case only because
 *  the ledger digests the asset's BYTES through the same lodgenReadAsset() the
 *  bake uses, so moving a mod folder still dirties every chunk whose assets
 *  changed under it.
 *
 *  --native and --native-mesh-report are here, and they were taken OFF for one
 *  afternoon on the reasoning that a flag changing what a run WRITES belongs in
 *  the digest. That reasoning cost tests/spells/lodgen_native.sh check 5 -- the
 *  stock bake is byte-identical with and without --native -- because the only
 *  file that then differed was the ledger recording the flag. The pair goes to
 *  its own --native directory and cannot touch a chunk; the run that WOULD be
 *  wrong (an incremental one) is refused outright a few lines below, which is a
 *  better answer than a switch digest that fires on the honest case too.
 *
 *  --threads is on the list, and that is a claim: BAKEPERF1's pass retires
 *  every job on the calling thread IN JOB ORDER, so the worker count cannot
 *  reach a byte. If that ever stops being true this line is the bug. */
static const char * const gLgSwitchSkip[] = {
	"--out-dir", "--tex-dir", "--data-root", "--incremental",
	"--threads", "--chunk-threads", "--preview-dir",
	"--resource", "--plugins-txt", "--mo2-profile", "--mo2-mods",
	"--native", "--native-mesh-report",
	nullptr
};

/*! Flags whose TOKEN stays in the digest and whose VALUE is dropped. The list
 *  exists because a flag can be both things at once: --vt makes the chunk
 *  sheets come from the virtual-texture pyramid instead of the stock per-chunk
 *  composite -- a different picture from the same inputs, so the flag must be
 *  digested -- while its argument is only a place to put the pyramid, exactly
 *  like --out-dir's.
 *
 *  Digesting that path made two identical commands write two different ledgers
 *  whenever they were pointed at different directories, which is what
 *  tests/spells/lodgen_roads.sh R1 does on purpose: it bakes --no-roads twice
 *  into roadOff/ and roadOff2/ and compares every byte. One of the two kinds of
 *  list would have been enough for either flag; neither was enough for both. */
static const char * const gLgSwitchSkipValue[] = {
	"--vt",
	nullptr
};

QString lodgenSwitchDigestOf( const QStringList & a )
{
	QCryptographicHash h( QCryptographicHash::Sha1 );
	for ( int i = 0; i < a.size(); i++ ) {
		bool skip = false;
		for ( const char * const * s = gLgSwitchSkip; *s; s++ ) {
			if ( a.at( i ) == QLatin1String( *s ) ) {
				skip = true;
				i++;                    /* and its value */
				break;
			}
		}
		if ( skip )
			continue;
		for ( const char * const * s = gLgSwitchSkipValue; *s; s++ ) {
			if ( a.at( i ) == QLatin1String( *s ) ) {
				h.addData( a.at( i ).toUtf8() );   /* the flag, never its path */
				h.addData( QByteArray( "\x1f", 1 ) );
				skip = true;
				i++;
				break;
			}
		}
		if ( skip )
			continue;
		h.addData( a.at( i ).toUtf8() );
		h.addData( QByteArray( "\x1f", 1 ) );
	}
	return QString::fromLatin1( h.result().toHex() );
}

namespace
{

QString chunkKey( int cx, int cy )
{
	return QString( "%1,%2" ).arg( cx ).arg( cy );
}

/* A run over several rings (`--dim all`, lane BAKE1) has a ring-4 and a
 * ring-8 chunk at the same corner. A chunk of the run's OWN ring keeps the
 * old key, so a one-ring record is unchanged to the byte; another ring's
 * chunk is `cx,cy@dim`. Only the record writer and the produced-file list use
 * it: --incremental refuses a many-ring run. */
QString chunkKeyAt( int dim, int runDim, int cx, int cy )
{
	return dim == runDim ? chunkKey( cx, cy )
		: QString( "%1,%2@%3" ).arg( cx ).arg( cy ).arg( dim );
}

} // namespace

QStringList lodgenIdentityDump( const LodgenChunkPassOptions & pass, const LodgenIdentityExtras & x )
{
	/* ONE LINE PER EFFECTIVE SETTING, in a fixed order, so the dump can be
	 * printed and two of them diffed by eye. Floats go through `%.9g`, which
	 * round-trips a float exactly; nothing is formatted by locale. Paths,
	 * thread counts, the preview folder and every progress hook are left out
	 * on purpose (the header says why). `wantBtr`/`wantBto`/`wantTex` are left
	 * to the front end: the command line has always digested `--tex-dir` as a
	 * PLACE, and the panel adds its own ticks through `more`. */
	QStringList d;
	auto b = []( bool v ) { return v ? QStringLiteral( "1" ) : QStringLiteral( "0" ); };
	auto f = []( double v ) { return QString::number( v, 'g', 9 ); };
	auto n = []( qint64 v ) { return QString::number( v ); };
	auto add = [&d]( const QString & k, const QString & v ) { d.append( k + QChar( '=' ) + v ); };
	auto cover = [&]( const QString & p, const LodgenCoverOptions & c ) {
		add( p + "cover", b( c.cover ) );
		add( p + "tintStrength", f( c.tintStrength ) );
		add( p + "coverFull", f( c.coverFull ) );
		add( p + "roads", b( c.roads ) );
		add( p + "roadCoverSuppress", f( c.roadCoverSuppress ) );
		add( p + "roadOpacity", f( c.roadOpacity ) );
		add( p + "roadComposite", n( c.roadComposite ) );
		add( p + "roadDetail", f( c.roadDetail ) );
		add( p + "roadGroundPaint", f( c.roadGroundPaint ) );
		add( p + "roadRaised", b( c.roadRaised ) );
		add( p + "roadSidewalks", b( c.roadSidewalks ) );
		add( p + "terrainObjectAo", b( c.terrainObjectAo ) );
		add( p + "terrainObjectAoStrength", f( c.terrainObjectAoStrength ) );
		add( p + "terrainObjectAoSlab", b( c.terrainObjectAoSlab ) );
	};

	add( "generator", n( kLodgenGeneratorRevision ) );

	const LodgenTerrainOptions & t = pass.terrain;
	add( "terrain.dim", n( t.dim ) );
	add( "terrain.water", b( t.water ) );
	add( "terrain.waterSubdiv", n( t.waterSubdiv ) );
	add( "terrain.waterChannels", b( t.waterChannels ) );
	add( "terrain.waterCullBuried", b( t.waterCullBuried ) );
	add( "terrain.shoreDenser", b( t.shoreDenser ) );
	add( "terrain.shoreDensity", n( t.shoreDensity ) );
	add( "terrain.targetTrisPerCell", n( t.targetTrisPerCell ) );
	add( "terrain.geomorph", b( t.geomorph ) );
	add( "terrain.terrainIdentity", b( t.terrainIdentity ) );
	// a format string written INTO the .BTR, not a place on disk
	add( "terrain.textureBase", t.textureBase );

	const LodgenObjectOptions & o = pass.object;
	add( "object.dim", n( o.dim ) );
	add( "object.identity", b( o.identity ) );
	add( "object.bakeAO", b( o.bakeAO ) );
	add( "object.cullBuried", b( o.cullBuried ) );
	add( "object.aoGrey", b( o.aoGrey ) );
	add( "object.aoSkirtCells", n( o.aoSkirtCells ) );
	add( "object.treeSway", b( o.treeSway ) );
	add( "object.objectChannels", b( o.objectChannels ) );
	add( "object.cullMargin", f( o.cullMargin ) );
	add( "object.cardAuxDiv", n( o.cardAuxDiv ) );
	add( "object.lodLevel", n( o.lodLevel ) );
	add( "object.slotFallback", b( o.slotFallback ) );
	add( "object.impostors", b( !o.impostorDir.isEmpty() ) );
	add( "object.impostorFromLevel", n( o.impostorFromLevel ) );
	add( "object.treesOnly", b( o.treesOnly ) );

	cover( QStringLiteral( "cover." ), pass.cover );

	/* THE PROCESS-WIDE LAND AND BLEND SETTINGS. These are the ones both
	 * default flips so far moved (DEFAULTS1: hex, warp, mip bias, guide;
	 * DEFAULTS2: the edge blend), and they are file statics inside lodgen.cpp,
	 * reachable only through their getters. */
	add( "land.tiling", f( lodgenLandTiling() ) );
	add( "land.sampleAverage", b( lodgenLandSampleAverage() ) );
	add( "land.detail", f( lodgenLandDetail() ) );
	add( "land.warpAmp", f( lodgenLandWarpAmp() ) );
	add( "land.warpLattice", f( lodgenLandWarpLattice() ) );
	add( "land.warpOctaves", n( lodgenLandWarpOctaves() ) );
	add( "land.mipBias", f( lodgenLandMipBias() ) );
	add( "land.hexSize", f( lodgenLandHexSize() ) );
	add( "land.guideRule", n( lodgenLandGuideRule() ) );
	add( "land.guideStrength", f( lodgenLandGuideStrength() ) );
	add( "land.guideScale", f( lodgenLandGuideScale() ) );
	add( "land.guideSlopeRef", f( lodgenLandGuideSlopeRef() ) );
	add( "land.erosion", f( lodgenErosion() ) );
	add( "land.erosionIterations", n( lodgenErosionIterations() ) );
	add( "land.erosionSeed", n( lodgenErosionSeed() ) );
	add( "land.detailSource", n( lodgenLandDetailSource() ) );
	add( "land.shade", f( lodgenLandShade() ) );
	add( "land.grade", f( lodgenLandGrade() ) );
	add( "land.sheetFormat", n( lodgenSheetFormat() ) );
	add( "blend.edges", n( lodgenBlendEdges() ) );
	add( "blend.margin", f( lodgenBlendMargin() ) );

	add( "run.atlas", b( x.atlas ) );
	add( "run.arrays", b( x.arrays ) );
	add( "run.merge", b( x.merge ) );
	add( "run.atlasBc1", b( x.atlasBc1 ) );
	add( "run.keepBto", b( x.keepBto ) );
	add( "run.texFromVt", b( x.texFromVt ) );
	add( "simplify.enabled", b( x.simplify.enabled ) );
	add( "simplify.ratio8", f( x.simplify.ratio8 ) );
	add( "simplify.ratio16", f( x.simplify.ratio16 ) );
	add( "simplify.ratio32", f( x.simplify.ratio32 ) );
	add( "simplify.errorWorld", f( x.simplify.errorWorld ) );
	add( "simplify.minTris", n( x.simplify.minTris ) );
	add( "vt.finestDim", n( x.vt.finestDim ) );
	add( "vt.content", n( x.vt.content ) );
	add( "vt.border", n( x.vt.border ) );
	add( "vt.mips", n( x.vt.mips ) );
	add( "vt.compression", n( x.vt.compression ) );
	add( "vt.height", b( x.vt.height ) );
	add( "vt.coverInColor", b( x.vt.coverInColor ) );
	add( "vt.halfAux", b( x.vt.halfAux ) );
	add( "vt.btr", n( x.vtBtr ) );
	cover( QStringLiteral( "vt.cover." ), x.vt.cover );
	add( "native.ladder", b( x.nativeLadder ) );
	add( "native.occluders", b( x.nativeOccluders ) );
	add( "native.libraryNear", b( x.libraryNear ) );
	add( "native.ladderFoliage", b( x.ladderFoliage ) );
	add( "native.silhouetteMin", f( x.silhouetteMin ) );
	add( "native.placementAo", b( x.placementAo ) );
	add( "native.vertexAo", b( x.vertexAo ) );
	add( "native.lodiV7", b( x.lodiV7 ) );
	add( "native.scrappable", b( x.scrappable ) );
	add( "native.identityJoinLegacy", b( x.identityJoinLegacy ) );
	add( "native.identityJoinGap", f( x.identityJoinGap ) );
	add( "native.aggregate", b( x.aggregate ) );
	add( "native.aggregateMin", n( x.aggMin ) );
	add( "native.aggregateTile", n( x.aggTile ) );
	add( "native.aggregateViews", n( x.aggViews ) );

	QStringList more = x.more;
	more.sort();
	d += more;
	return d;
}

QString lodgenIdentityWord( const QStringList & dump )
{
	const QByteArray h = QCryptographicHash::hash(
		dump.join( QChar( '\n' ) ).toUtf8(), QCryptographicHash::Sha1 );
	return QStringLiteral( "gen%1:%2" ).arg( kLodgenGeneratorRevision )
		.arg( QString::fromLatin1( h.toHex() ) );
}

QString lodgenSwitchesWithIdentity( const QString & argvDigest, const QString & identityWord )
{
	QCryptographicHash h( QCryptographicHash::Sha1 );
	h.addData( argvDigest.toUtf8() );
	h.addData( QByteArray( "\x1f", 1 ) );
	h.addData( identityWord.toUtf8() );
	return QString::fromLatin1( h.result().toHex() );
}

LodgenIncrementalVerdict lodgenIncrementalBegin( LodgenIncrementalRun & run, const EsmWorld & world,
	QVector<LodgenChunkJob> & jobs, QString * census, QStringList * reasonsOut, QString * detail )
{
	const QString ws = world.worldspaceEdid();
	const bool fo4cs = !run.nativeDir.isEmpty();
	run.ledgerPath = lodbRecordPath( run.recordRoot.isEmpty() ? run.outDir : run.recordRoot, ws, fo4cs );
	run.lodjDir = fo4cs ? lodgenFo4csWorldDir( run.nativeDir, ws ) : QString();
	run.allJobs = jobs;
	run.incremental = false;
	run.prev = LodgenLedger();
	run.prevRecordDir.clear();
	run.producedFiles.clear();
	run.lodjWritten = run.lodjReplayed = run.lodjFailed = 0;
	run.lodjPlacements = 0;
	if ( run.fromDir.isEmpty() )
		return LodgenIncrementalVerdict::Go;

	/* The directory the PREVIOUS record's relative output paths resolve
	 * against: the record's own folder, which is the law the format states
	 * (`docs/LODGEN_BAKE_RECORD.md`) and which lets a mod folder be moved. */
	const QString found = lodbFindRecord( run.fromDir, ws );
	const QString srcLedger = found.isEmpty() ? lodbRecordPath( run.fromDir, ws, fo4cs ) : found;
	run.prevRecordDir = QFileInfo( srcLedger ).absolutePath();
	/* THE REFUSALS, in INCR1's order. Each is reported with the reason AND the
	 * way forward and nothing is baked: an incremental run that silently
	 * promoted itself to a full bake would lie to an operator watching a clock. */
	QString lerr;
	if ( !lodgenReadLedger( srcLedger, &run.prev, &lerr ) ) {
		run.prev = LodgenLedger();
		if ( !run.requireRecord ) {
			/* THE PANEL'S FIRST RUN. A row that stays ticked is a standing
			 * setting, not a request to diff, so with no record yet the whole
			 * region is baked and the record written; the next run diffs. */
			if ( census )
				*census = QString( "incremental: no bake record at %1 yet; all %2 chunk(s) baked "
								   "and the record written" ).arg( srcLedger ).arg( jobs.size() );
			return LodgenIncrementalVerdict::Go;
		}
		if ( detail )
			*detail = lerr;
		return LodgenIncrementalVerdict::NoRecord;
	}
	const LodgenLedger & prev = run.prev;
	if ( prev.worldspace != run.worldspace || prev.dim != run.dim
		 || prev.region[0] != run.region[0] || prev.region[1] != run.region[1]
		 || prev.region[2] != run.region[2] || prev.region[3] != run.region[3] ) {
		if ( detail )
			*detail = srcLedger;
		return LodgenIncrementalVerdict::Shape;
	}
	if ( prev.switches != run.switches )
		return LodgenIncrementalVerdict::Switches;
	/* WHICH POST-PASSES ARE WHOLE-REGION. The merge and the far-ring simplify
	 * rewrite one .BTO at a time with nothing carried between files, so a
	 * filtered list gives each rebaked chunk exactly the full-run treatment.
	 * The atlas, the arrays and the impostor card sets build ONE product out
	 * of every written .BTO, so a filtered list would build them from a
	 * fraction of the region. The merge is ON by default; refusing it would
	 * have made --incremental refuse every command anybody types. */
	if ( run.regionProducts )
		return LodgenIncrementalVerdict::RegionProducts;
	/* --native builds ONE .lodo/.lodi pair out of the placements the chunk
	 * pass hands it. Since lane INCR1 a skipped chunk speaks from its `.lodj`
	 * cache in its own queue position, so the arrival order -- which the
	 * library's mesh ids depend on -- is the order a full bake produces. With
	 * the cache OFF there is nothing to read back: the old refusal stands. */
	if ( fo4cs && !run.nativeCache )
		return LodgenIncrementalVerdict::NativeNoCache;

	/* Now the diff. A chunk is dirty when its input digest moved, when the
	 * ledger has never heard of it, or when an output it claims is missing or
	 * has been edited under us.
	 *
	 * THE LIST THE WIDENING IS SEEDED FROM (lane INCR1, 2026-09-17) is not the
	 * same list: a chunk dirty only because its own `.lodj` went missing
	 * changes nothing for its neighbours. Seeding the widening from it made
	 * deleting ONE cache file rebake the whole region
	 * (`scratchpad/incr1_20260917/s2_proof.txt` leg C). */
	const int d = run.dim;
	QSet<QString> dirty, dirtyWide;
	QHash<QString, const LodgenLedgerEntry *> byKey;
	for ( const LodgenLedgerEntry & e : prev.chunks )
		byKey.insert( chunkKey( e.cx, e.cy ), &e );
	int movedInputs = 0, unknown = 0, lostOutput = 0;
	QStringList reasons;
	for ( const LodgenChunkJob & j : run.allJobs ) {
		const QString key = chunkKey( j.cx, j.cy );
		const LodgenLedgerEntry * pe = byKey.value( key, nullptr );
		if ( !pe ) {
			dirty.insert( key );
			dirtyWide.insert( key );
			unknown++;
			continue;
		}
		const LodgenLedgerEntry & e = *pe;
		const QString now = lodgenChunkInputDigest( world, d, j.cx, j.cy, run.digestRoot );
		if ( now != e.inputs ) {
			dirty.insert( key );
			dirtyWide.insert( key );
			movedInputs++;
			if ( reasons.size() < 8 )
				reasons.append( QString( "  (%1,%2) inputs %3 -> %4" )
					.arg( j.cx ).arg( j.cy ).arg( e.inputs.left( 12 ), now.left( 12 ) ) );
			continue;
		}
		/* EVERY output, not the first lost one, because WHICH output was lost
		 * decides whether the neighbours are dragged in with it. */
		bool lostAny = false, lostReal = false;
		for ( int k = 0; k < e.outFiles.size(); k++ ) {
			const QString fp = run.prevRecordDir + "/" + e.outFiles[k];
			if ( lodgenFileDigest( fp ) == e.outDigests[k] )
				continue;
			const bool isCache = e.outFiles[k].endsWith( QLatin1String( ".lodj" ) );
			if ( !lostAny && reasons.size() < 8 )
				reasons.append( QString( "  (%1,%2) output %3 is missing or edited%4" )
					.arg( j.cx ).arg( j.cy ).arg( e.outFiles[k] )
					.arg( isCache ? QStringLiteral( " (a chunk cache: this chunk only)" ) : QString() ) );
			lostAny = true;
			if ( !isCache )
				lostReal = true;
		}
		if ( lostAny ) {
			dirty.insert( key );
			lostOutput++;
			if ( lostReal )
				dirtyWide.insert( key );
		}
	}
	/* THE WIDENING. A chunk is also dirty when a chunk within ONE CELL of it
	 * is: the terrain ring and the AO skirt each reach exactly one cell
	 * (docs/LODGEN_LEDGER_FORMAT.md section 2). It fires on a neighbour whose
	 * OUTPUT was lost, which no input digest can see. */
	QSet<QString> widened = dirty;
	for ( const LodgenChunkJob & j : run.allJobs ) {
		if ( widened.contains( chunkKey( j.cx, j.cy ) ) )
			continue;
		for ( const QString & dk : dirtyWide ) {
			const int dx = dk.section( QLatin1Char( ',' ), 0, 0 ).toInt();
			const int dy = dk.section( QLatin1Char( ',' ), 1, 1 ).toInt();
			if ( j.cx <= dx + d && dx <= j.cx + d && j.cy <= dy + d && dy <= j.cy + d ) {
				widened.insert( chunkKey( j.cx, j.cy ) );
				break;
			}
		}
	}
	const int spread = widened.size() - dirty.size();
	/* THE CACHE HAS TO BE THERE FOR EVERY CHUNK THIS RUN WILL SKIP. A record
	 * written before INCR1 lists no `.lodj` at all, so every chunk would look
	 * clean, nothing would be replayed, and the run would write a valid,
	 * self-consistent, EMPTY pair -- so this is checked against the DISK. */
	int lostCache = 0;
	if ( fo4cs ) {
		for ( const LodgenChunkJob & j : run.allJobs ) {
			const QString key = chunkKey( j.cx, j.cy );
			if ( widened.contains( key ) )
				continue;
			const QString cp = QString( "%1/%2.%3.%4.%5.lodj" )
				.arg( run.lodjDir, ws ).arg( j.dim ).arg( j.cx ).arg( j.cy );
			if ( !QFileInfo::exists( cp ) ) {
				widened.insert( key );
				lostCache++;
				if ( reasons.size() < 8 )
					reasons.append( QString( "  (%1,%2) has no native chunk cache" ).arg( j.cx ).arg( j.cy ) );
			}
		}
	}
	QVector<LodgenChunkJob> kept;
	for ( const LodgenChunkJob & j : run.allJobs )
		if ( widened.contains( chunkKey( j.cx, j.cy ) ) )
			kept.append( j );
	if ( census )
		*census = QString( "incremental: %1 of %2 chunks dirty "
						   "(%3 inputs moved, %4 not in the ledger, %5 output lost, "
						   "%6 by neighbour, %7 with no native chunk cache)" )
			.arg( kept.size() ).arg( run.allJobs.size() )
			.arg( movedInputs ).arg( unknown ).arg( lostOutput ).arg( spread )
			.arg( lostCache );
	if ( reasonsOut )
		*reasonsOut = reasons;
	jobs = kept;
	run.incremental = true;
	return LodgenIncrementalVerdict::Go;
}

QStringList lodgenIncrementalRefusal( LodgenIncrementalVerdict v, const LodgenIncrementalRun & run,
	const QString & detail )
{
	const LodgenLedger & p = run.prev;
	switch ( v ) {
	case LodgenIncrementalVerdict::NoRecord:
		return { QStringLiteral( "refused: --incremental has nothing to diff against -- " ) + detail,
			QStringLiteral( "  run the same command once WITHOUT --incremental; every bake writes "
				"the ledger, so the next run can be incremental." ) };
	case LodgenIncrementalVerdict::Shape:
		return { QString( "refused: %1 describes worldspace %2 dim %3 region %4 %5 %6 %7, and this "
				"run is a different shape." ).arg( detail ).arg( p.worldspace, 0, 16 ).arg( p.dim )
				.arg( p.region[0] ).arg( p.region[1] ).arg( p.region[2] ).arg( p.region[3] ),
			QStringLiteral( "  an incremental run must cover exactly the region its ledger covers; "
				"bake this region once without --incremental." ) };
	case LodgenIncrementalVerdict::Switches:
		return { QStringLiteral( "refused: the switches differ from the ones the ledger was written with, "
				"so EVERY chunk is dirty and an incremental run would be a full run with "
				"extra bookkeeping." ),
			QStringLiteral( "  bake without --incremental. (The digest covers the argument vector in "
				"order; even reordering flags fires it, which is deliberate -- it can "
				"only over-rebake, never under-rebake. Since 2026-09-24 it also covers the "
				"identity word -- the generator revision and every setting the bake used, "
				"typed or defaulted -- so a default that moved between two builds fires it too.)" ) };
	case LodgenIncrementalVerdict::RegionProducts:
		return { QStringLiteral( "refused: --atlas, --arrays and --impostors each build ONE region-wide "
				"product out of the whole written .BTO list, so a filtered chunk list "
				"would build them from a FRACTION of the region and not say so." ),
			QStringLiteral( "  bake without --incremental, or drop those flags from this run and do "
				"them in a separate full pass over the finished chunks. (The merge and "
				"the far-ring simplify are NOT on this list: both rewrite one .BTO at a "
				"time with nothing carried between files.)" ) };
	case LodgenIncrementalVerdict::NativeNoCache:
		return { QStringLiteral( "refused: --native builds ONE .lodo/.lodi pair for the whole region out of "
				"the placements the chunk pass hands it, so an incremental run would write "
				"a pair covering only the chunks it rebaked and say nothing about the rest." ),
			QStringLiteral( "  --no-native-cache is what turned the per-chunk cache off; drop it and "
				"the skipped chunks speak from their .lodj files. Or bake without "
				"--incremental." ) };
	case LodgenIncrementalVerdict::Go:
		break;
	}
	return {};
}

void lodgenIncrementalArmCache( LodgenIncrementalRun & run, const QString & worldEdid,
	LodgenChunkPassOptions & pass )
{
	/* THE PER-CHUNK NATIVE CACHE (lane INCR1, 2026-09-17). Written on every
	 * FO4CS bake the front end arms it for, full or incremental, because a
	 * cache only helps the run AFTER the one that made it. It is listed in the
	 * record's `out` rows like every other output, so editing one by hand makes
	 * its chunk dirty by the ordinary output check. */
	if ( run.nativeDir.isEmpty() || !run.nativeCache )
		return;
	LodgenIncrementalRun * rp = &run;
	const QString ws = worldEdid;
	auto lodjPath = [rp, ws]( int dm, int cx, int cy ) {
		return QString( "%1/%2.%3.%4.%5.lodj" ).arg( rp->lodjDir, ws ).arg( dm ).arg( cx ).arg( cy );
	};
	pass.nativeJournalSink = [rp, ws, lodjPath]( const LodgenChunkOutcome & r, LodgenNativeJournal * jr ) {
		const QString cp = lodjPath( r.dim, r.cx, r.cy );
		QString jerr;
		if ( !lodgenNativeJournalWriteCache( jr, cp, ws, r.dim, r.cx, r.cy, &jerr ) ) {
			if ( rp->warn )
				rp->warn( QString( "native cache (%1,%2): %3" ).arg( r.cx ).arg( r.cy ).arg( jerr ) );
			rp->lodjFailed++;
			return;
		}
		rp->lodjWritten++;
		/* the retire hook for this same job appends to this very list a moment
		 * later, on this same thread */
		rp->producedFiles[chunkKeyAt( r.dim, rp->dim, r.cx, r.cy )].append( cp );
	};
	if ( run.incremental ) {
		pass.nativeAllJobs = run.allJobs;
		pass.nativeReplayCached = [rp, lodjPath]( const LodgenChunkJob & j ) {
			QString jerr;
			if ( !lodgenNativeReplayCache( lodjPath( j.dim, j.cx, j.cy ), &jerr ) ) {
				if ( rp->warn )
					rp->warn( QStringLiteral( "native cache: " ) + jerr );
				rp->lodjFailed++;
				return;
			}
			rp->lodjReplayed++;
			rp->lodjPlacements += lodgenNativeCacheLastPlacements();
		};
	}
}

void lodgenIncrementalNoteRetired( LodgenIncrementalRun & run, const LodgenChunkPassOptions & pass,
	const LodgenChunkOutcome & r )
{
	QStringList & pf = run.producedFiles[chunkKeyAt( r.dim, run.dim, r.cx, r.cy )];
	const bool scratch = !pass.btoScratchDir.isEmpty();
	/* WITH A SCRATCH FOLDER the `.BTO` will not survive this run, so it is not
	 * a tracked output: digesting a file about to be deleted made every later
	 * --incremental run rebake the whole region. Its manifest DOES survive, at
	 * its final path under FO4CSLOD/<ws>/ (lane LAYOUT1) -- naming any other
	 * path cost the entry its digest once -- and the digest is taken at
	 * record-write time, after the move. */
	for ( const QString & p : { r.btrPath, r.btoPath, r.manifestPath } ) {
		if ( p.isEmpty() )
			continue;
		if ( scratch && p == r.btoPath )
			continue;
		if ( scratch && p == r.manifestPath ) {
			pf.append( lodgenFo4csWorldDir( run.outDir, pass.worldEdid ) + QStringLiteral( "/" )
				+ QFileInfo( r.btoPath ).fileName() + QStringLiteral( ".manifest.txt" ) );
			continue;
		}
		/* NO EXISTENCE CHECK (2026-09-18): at more than one chunk thread the
		 * bytes may still sit in the writer's queue when the job retires. */
		pf.append( p );
	}
	/* The colour/normal/data sheets, named from the same format string
	 * lodgenBakeTerrainTextures builds them with: the outcome carries only an
	 * error string for them, and without this deleting a sheet by hand would
	 * not make its chunk dirty. */
	if ( pass.wantTex && !pass.texDir.isEmpty() ) {
		const QString sheetBase = QString( "%1/%2.%3.%4.%5" )
			.arg( pass.texDir ).arg( pass.worldEdid ).arg( r.dim ).arg( r.cx ).arg( r.cy );
		for ( const QString & sfx : { QStringLiteral( ".DDS" ), QStringLiteral( "_msn.DDS" ),
				QStringLiteral( "_data.DDS" ) } )
			if ( QFileInfo::exists( sheetBase + sfx ) )
				pf.append( sheetBase + sfx );
	}
}

QString lodgenIncrementalCacheCensus( const LodgenIncrementalRun & run )
{
	if ( run.nativeDir.isEmpty() || !run.nativeCache )
		return QString();
	return QString( "native cache: %1 chunk(s) written to .lodj, "
					"%2 replayed from cache (%3 placement(s)), %4 failure(s), "
					"%5 arrival(s) lit by more than one chunk" )
		.arg( run.lodjWritten ).arg( run.lodjReplayed ).arg( run.lodjPlacements )
		.arg( run.lodjFailed ).arg( lodgenNativeSharedArrivals() );
}

QStringList lodgenIncrementalCacheRefusal( const LodgenIncrementalRun & run )
{
	if ( run.lodjFailed > 0 )
		return { QString( "error: %1 native chunk cache failure(s); the pair this run would write "
				"is missing whole chunks. Bake without --incremental." ).arg( run.lodjFailed ) };
	/* THE ONE CASE THE CACHE CANNOT REPRODUCE BIT FOR BIT: an arrival keyed
	 * across chunks has lighting sums built from both, and `(prev + a1) + a2`
	 * is not `prev + (a1 + a2)` in floating point. */
	if ( run.lodjReplayed > 0 && lodgenNativeSharedArrivals() > 0 )
		return { QString( "refused: %1 placement(s) were lit by more than one chunk, so a rebuilt "
				"chunk and a cached one would have to have their lighting sums "
				"added in an order this run cannot reproduce." ).arg( lodgenNativeSharedArrivals() ),
			QStringLiteral( "  bake without --incremental: a full bake adds them in the one "
				"order there is." ) };
	return {};
}

void lodgenIncrementalOfferReuse( const LodgenIncrementalRun & run )
{
	/* THE LIBRARY-REUSE OFFER (lane PERF1). Only an incremental bake offers,
	 * with the three hashes the PREVIOUS record wrote; lodgenNativeWrite keeps
	 * the previous `.lodo` only when all three agree and it reads back whole.
	 * The switch digest -- and with it the identity word -- is not passed: a
	 * run whose digest moved never reaches here. */
	if ( !run.incremental || run.prev.loadOrderHashHex.isEmpty()
		|| run.prev.pluginCorpusHashHex.isEmpty() || run.prev.objectCorpusHashHex.isEmpty() )
		return;
	NativeReuseOffer offer;
	offer.armed = true;
	offer.loadOrderHex    = run.prev.loadOrderHashHex;
	offer.pluginCorpusHex = run.prev.pluginCorpusHashHex;
	offer.objectCorpusHex = run.prev.objectCorpusHashHex;
	lodgenNativeOfferLibraryReuse( offer );
}

bool lodgenIncrementalWriteRecord( const LodgenIncrementalRun & run, const EsmWorld & world,
	const QStringList & switchTokens, const QStringList & resourceStack,
	QStringList * warnings, QString * readBack )
{
	/* THE BAKE RECORD (lanes INCR1 and BAKEREC1). Written by every bake that
	 * asks for it, LAST: after every file is closed -- the merge and the
	 * far-ring cut rewrite each .BTO, and digesting before them recorded
	 * hashes of files that no longer existed -- and after every census line,
	 * because the record carries the census verbatim. Deterministic except for
	 * the five things `src/lodbfile.h` names. */
	LodgenLedger led;
	led.worldspace = run.worldspace;
	led.worldEdid  = world.worldspaceEdid();
	led.dim        = run.dim;
	for ( int k = 0; k < 4; k++ )
		led.region[k] = run.region[k];
	led.switches  = run.switches;
	led.loadOrder = QString::number( world.loadOrderHash(), 16 );
	/* THE RELATIVE ROOT IS THE RECORD'S OWN FOLDER, so a mod folder can be
	 * moved; the incremental reader resolves them the same way. */
	const QString ledgerRoot = QFileInfo( run.ledgerPath ).absolutePath();
	QHash<QString, const LodgenLedgerEntry *> oldByKey;
	for ( const LodgenLedgerEntry & e : run.prev.chunks )
		oldByKey.insert( chunkKeyAt( e.dim, run.dim, e.cx, e.cy ), &e );
	for ( const LodgenChunkJob & j : run.allJobs ) {
		const QString key = chunkKeyAt( j.dim, run.dim, j.cx, j.cy );
		const bool rebaked = run.producedFiles.contains( key );
		if ( !rebaked && run.incremental ) {
			/* A chunk this run SKIPPED: its row is carried forward, which makes
			 * the record a record of what is on disk. */
			if ( const LodgenLedgerEntry * pe = oldByKey.value( key, nullptr ) )
				led.chunks.append( *pe );
			continue;
		}
		LodgenLedgerEntry e;
		e.dim = j.dim; e.cx = j.cx; e.cy = j.cy;
		e.inputs = lodgenChunkInputDigest( world, j.dim, j.cx, j.cy, run.digestRoot );
		for ( const QString & p : run.producedFiles.value( key ) ) {
			e.outFiles.append( QDir( ledgerRoot ).relativeFilePath( QDir( p ).absolutePath() ) );
			e.outDigests.append( lodgenFileDigest( p ) );
		}
		led.chunks.append( e );
	}

	led.fo4csTarget = !run.nativeDir.isEmpty();
	led.exeStamp    = lodbExeStamp();
	led.exeBytes    = lodbExeSize();
	led.bakedUtc    = QDateTime::currentDateTimeUtc().toString( Qt::ISODate );

	/* THE CORPUS HASHES, from the pair that was just written, so the record
	 * cannot disagree with it. No pair, no hash line. */
	auto hex16 = []( quint64 v ) { return QString( "%1" ).arg( v, 16, 16, QChar( '0' ) ); };
	led.loadOrderHashHex = hex16( world.loadOrderHash() );
	if ( led.fo4csTarget ) {
		const QString lodoPath = lodgenFo4csWorldDir( run.nativeDir, world.worldspaceEdid() )
			+ QChar( '/' ) + world.worldspaceEdid() + QStringLiteral( ".lodo" );
		LodoHeader lh;
		QString rerr;
		if ( lodoRead( lodoPath, &lh, nullptr, false, &rerr ) ) {
			led.pluginCorpusHashHex = hex16( lh.pluginCorpusHash );
			led.objectCorpusHashHex = hex16( lh.objectCorpusHash );
			led.modelCorpusHashHex  = hex16( lh.modelCorpusHash );
			led.cardCorpusHashHex   = hex16( lh.cardCorpusHash );
			led.loadOrderHashHex    = hex16( lh.loadOrderHash );
		} else if ( warnings ) {
			warnings->append( QString( "warning: the bake record cannot read back %1 for its corpus "
				"hashes (%2) -- the record will carry loadOrderHash only" ).arg( lodoPath, rerr ) );
		}
	}

	/* THE PLUGINS, IN LOAD ORDER, each with an FNV-1a 64 over its BYTES
	 * (bungo 2026-09-16: "each bake needs to know plugins used or what's
	 * different, to even attempt a partial rebake"). */
	{
		const QStringList paths = world.pluginList().split( QChar( ',' ), Qt::SkipEmptyParts );
		for ( int i = 0; i < paths.size(); i++ ) {
			const QFileInfo fi( paths.at( i ).trimmed() );
			LodbPlugin p;
			p.index = i;
			p.name  = fi.fileName().toLower();
			p.bytes = fi.size();
			p.path  = QDir::fromNativeSeparators( fi.absoluteFilePath() );
			if ( !lodbFileFnv1a64( p.path, &p.hash ) ) {
				p.hash = 0;
				if ( warnings )
					warnings->append( QString( "warning: the bake record cannot read %1 to hash it -- "
						"that plugin line carries a zero hash and cannot detect an edit" ).arg( p.path ) );
			}
			led.plugins.append( p );
		}
	}

	/* THE RESOURCE STACK, in the order it was given (last wins). Informational
	 * and masked by lodbNormalise(). */
	for ( const QString & r : resourceStack ) {
		const QFileInfo fi( r );
		LodbResource e;
		e.path = QDir::fromNativeSeparators( fi.absoluteFilePath() );
		if ( fi.isDir() ) {
			e.kind = QStringLiteral( "folder" );
		} else {
			e.kind = fi.suffix().toLower() == QLatin1String( "bsa" )
				? QStringLiteral( "bsa" ) : QStringLiteral( "ba2" );
			e.bytes = fi.size();
			e.mtimeIso = fi.lastModified().toUTC().toString( Qt::ISODate );
		}
		led.resources.append( e );
	}

	led.switchTokens = switchTokens;
	led.census = lodbCensusLines();

	QDir().mkpath( QFileInfo( run.ledgerPath ).absolutePath() );
	QString lwerr;
	if ( !lodgenWriteLedger( run.ledgerPath, led, &lwerr ) ) {
		if ( warnings )
			warnings->append( QStringLiteral( "warning: " ) + lwerr
				+ QStringLiteral( " -- the bake is fine, but the next --incremental run will refuse" ) );
		return false;
	}
	if ( led.fo4csTarget )
		lodgenNoteLayoutFile( run.ledgerPath );
	/* THE LINE IS READ BACK OFF THE FILE (CONSTITUTION 4): a writer that
	 * dropped a section says so here instead of being believed. */
	LodgenLedger back;
	QString rerr;
	if ( readBack ) {
		if ( !lodgenReadLedger( run.ledgerPath, &back, &rerr ) )
			*readBack = QString( "bake-record: %1 REFUSED ON READ-BACK -- %2" ).arg( run.ledgerPath, rerr );
		else
			*readBack = QString( "bake-record: %1, %2 plugin(s), %3 resource(s), "
								 "%4 switch token(s), %5 chunk(s), %6 census line(s), "
								 "end %7 file(s) %8 bytes, record %9 bytes" )
				.arg( run.ledgerPath ).arg( back.plugins.size() ).arg( back.resources.size() )
				.arg( back.switchTokens.size() ).arg( back.chunks.size() )
				.arg( back.census.size() ).arg( back.endFiles ).arg( back.endBytes )
				.arg( QFileInfo( run.ledgerPath ).size() );
	}
	return true;
}
