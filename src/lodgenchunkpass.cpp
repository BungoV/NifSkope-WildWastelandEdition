/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "lodgenchunkpass.h"

#include "esmdata.h"
#include "lodgenparallel.h"
#include "lodgenlayout.h"
#include "model/nifmodel.h"
#include "nativeemit.h"

#include <QBuffer>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QSaveFile>
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
