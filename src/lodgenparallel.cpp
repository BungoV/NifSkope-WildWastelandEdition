/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#include "lodgenparallel.h"

#include <QByteArray>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QSaveFile>
#include <QStringList>
#include <QRunnable>
#include <QSemaphore>
#include <QThread>
#include <QThreadPool>
#include <QVector>
#include <QWaitCondition>

#include <atomic>
#include <cstring>
#include <deque>

#ifdef Q_OS_WIN32
#include <windows.h>
#include <psapi.h>
#endif

namespace
{

int g_threads = 0;                          //!< 0 = the machine
int g_chunkThreads = 1;                     //!< ONE: safe at 16, but slower
qint64 g_chunkTexBudgetBytes = qint64( 512 ) << 20;   //!< what the cap is computed against
thread_local bool tlsInWorker = false;

/*! The fan-out's own pool, PERSISTENT.
 *
 *  It used to create a fresh QThread per call and join it. The BC writers call
 *  the fan-out once per MIP of every sheet, so that was thousands of thread
 *  creations for block rows that encode in less time than a CreateThread takes
 *  -- measured as a 54 percent REGRESSION in the texture stage (5.2 s -> 8.0 s
 *  on the nine-chunk region) before this pool existed.
 *
 *  Its own pool rather than the global one, because the global instance's size
 *  is other code's business. Expiry disabled so the threads survive between
 *  calls. Nothing nested ever reaches it: a caller already inside a generator
 *  worker takes the serial path, so the pool cannot saturate against itself. */
QThreadPool & forPool()
{
	static QThreadPool pool;
	static const bool armed = []() {
		pool.setMaxThreadCount( qMax( 2, QThread::idealThreadCount() ) );
		pool.setExpiryTimeout( -1 );
		return true;
	}();
	Q_UNUSED( armed )
	return pool;
}

//! One share of a parallel-for, run on a pooled thread.
class ForTask final : public QRunnable
{
public:
	ForTask( int n, std::atomic<int> * next, const std::function<void( int )> * f,
		QSemaphore * done )
		: total( n ), cursor( next ), fn( f ) , finished( done )
	{
		setAutoDelete( true );
	}

	void run() override
	{
		{
			LodgenWorkerScope scope;
			for ( ;; ) {
				const int i = cursor->fetch_add( 1 );
				if ( i >= total )
					break;
				( *fn )( i );
			}
		}
		finished->release();
	}

private:
	int total;
	std::atomic<int> * cursor;
	const std::function<void( int )> * fn;
	QSemaphore * finished;
};

} // namespace

void lodgenSetThreadCount( int n )
{
	g_threads = n;
}

int lodgenThreadCount()
{
	int n = g_threads;
	if ( n <= 0 )
		n = QThread::idealThreadCount();
	return qMax( 1, n );
}

//! Set true by the CLI/panel only, so the census can tell the SHIPPED default
//! apart from a 1 somebody typed (lane RESUME3, 2026-09-11).
static bool g_chunkThreadsAsked = false;

void lodgenSetChunkThreadCount( int n )
{
	g_chunkThreads = n;
	g_chunkThreadsAsked = true;
}

//! The per-worker plugin reader, MEASURED: a load-plus-one-chunk run peaks
//! at 336 MB and the marginal cost of a second world is ~250 MB (BAKEPERF1,
//! 2026-09-11 section 3.10). Not an estimate, and not a round number chosen
//! to make the arithmetic come out.
static const qint64 kWorkerWorldBytes = qint64( 250 ) << 20;

//! Which bound decided the count, for the census line.
static QString g_chunkBoundBy = QStringLiteral( "cores" );

int lodgenChunkThreadMemoryCap( qint64 texBudgetBytes )
{
	const qint64 perWorker = kWorkerWorldBytes + qMax( qint64( 64 ) << 20, texBudgetBytes );
#ifdef Q_OS_WIN32
	MEMORYSTATUSEX	ms;
	memset( &ms, 0, sizeof( ms ) );
	ms.dwLength = sizeof( ms );
	if ( GlobalMemoryStatusEx( &ms ) ) {
		/* 60 percent of what is FREE, not of what is installed: the machine has
		 * a desktop and a browser on it, and the post-queue passes (the atlas,
		 * the arrays, the merge) still have to fit after the workers retire. */
		const qint64 usable = qint64( double( ms.ullAvailPhys ) * 0.60 );
		return qMax( 1, int( usable / perWorker ) );
	}
#else
	Q_UNUSED( perWorker )
#endif
	// no answer from the platform: do not invent one, do not cap
	return QThread::idealThreadCount();
}

QString lodgenChunkThreadBoundBy()
{
	return g_chunkBoundBy;
}

int lodgenChunkThreadCount()
{
	int n = g_chunkThreads;
	if ( n > 0 ) {
		g_chunkBoundBy = g_chunkThreadsAsked ? QStringLiteral( "asked" )
			: QStringLiteral( "default" );
		return n;
	}
	/* 0 or negative used to mean "the machine". It cannot: each chunk worker
	 * owns its own plugin reader AND its own texture cache, so sixteen of
	 * them is ~12 GB of FIXED overhead before one chunk of state -- 21.9 GB
	 * measured on twenty-five chunks, 70 percent of this machine. The count
	 * is the smaller of the cores and what free memory can hold, and the
	 * census says WHICH bound it was, so a run held back by memory reads as
	 * held back by memory rather than as mysteriously slow.
	 * (lane NIFPARSE1, 2026-09-11.) */
	const int cores = QThread::idealThreadCount();
	const int cap = lodgenChunkThreadMemoryCap( g_chunkTexBudgetBytes );
	g_chunkBoundBy = ( cap < cores ) ? QStringLiteral( "memory" ) : QStringLiteral( "cores" );
	return qMax( 1, qMin( cores, cap ) );
}

bool lodgenInWorker()
{
	return tlsInWorker;
}

LodgenWorkerScope::LodgenWorkerScope() : saved( tlsInWorker )
{
	tlsInWorker = true;
}

LodgenWorkerScope::~LodgenWorkerScope()
{
	tlsInWorker = saved;
}

void lodgenParallelFor( int n, const std::function<void( int )> & f, int cap )
{
	if ( n <= 0 )
		return;
	/* The cap only ever takes threads away (see the header's measurement), so
	 * `--threads 1` is still one thread and `--threads 0` on a two-core machine
	 * is still two. */
	const int t = cap > 0 ? qMin( lodgenThreadCount(), cap ) : lodgenThreadCount();
	// THE SIZE FLOOR: below eight items the threads cost more than the work
	if ( n < 32 || t <= 1 || tlsInWorker ) {
		// THE WAY BACK: the same loop, the same thread, ascending i
		for ( int i = 0; i < n; i++ )
			f( i );
		return;
	}
	const int workers = qMin( t, n );
	std::atomic<int> next{ 0 };
	QSemaphore done;
	for ( int w = 0; w < workers - 1; w++ )
		forPool().start( new ForTask( n, &next, &f, &done ) );
	// the calling thread takes a share as well, so `--threads 2` really is two
	{
		LodgenWorkerScope scope;
		for ( ;; ) {
			const int i = next.fetch_add( 1 );
			if ( i >= n )
				break;
			f( i );
		}
	}
	// nothing returns until every share has finished with `f` and with `next`
	done.acquire( workers - 1 );
}

quint64 lodgenPeakWorkingSet()
{
#ifdef Q_OS_WIN32
	PROCESS_MEMORY_COUNTERS pmc;
	memset( &pmc, 0, sizeof( pmc ) );
	pmc.cb = sizeof( pmc );
	if ( GetProcessMemoryInfo( GetCurrentProcess(), &pmc, sizeof( pmc ) ) )
		return quint64( pmc.PeakWorkingSetSize );
#endif
	return 0;
}

QString lodgenPeakWorkingSetLine()
{
	const quint64 b = lodgenPeakWorkingSet();
	if ( !b )
		return QStringLiteral( "peak working set: not available on this platform" );
	return QStringLiteral( "peak working set: %1 GB (%2 bytes)" )
		.arg( double( b ) / 1073741824.0, 0, 'f', 2 ).arg( b );
}

/* ---------------------------------------------------------------- writer */

struct LodgenWriter::Impl
{
	struct Item
	{
		QString path;
		QByteArray bytes;
	};

	QMutex mutex;
	QWaitCondition roomFree, workReady;
	std::deque<Item> queue;
	qint64 queued = 0;
	qint64 maxBytes = qint64( 256 ) << 20;
	bool stopping = false;
	QStringList failed;
	int count = 0;

	class Thread final : public QThread
	{
	public:
		explicit Thread( Impl * i ) : impl( i ) {}

	protected:
		void run() override
		{
			tlsInWorker = true;
			for ( ;; ) {
				Item item;
				{
					QMutexLocker lock( &impl->mutex );
					while ( impl->queue.empty() && !impl->stopping )
						impl->workReady.wait( &impl->mutex );
					if ( impl->queue.empty() && impl->stopping )
						break;
					item = std::move( impl->queue.front() );
					impl->queue.pop_front();
				}
				/* QSaveFile with the direct-write fallback, exactly as
				 * BaseModel::saveToFile does it, so a file that goes
				 * through the queue lands the same way as one written
				 * inline -- including under MO2's overlay. */
				QSaveFile f( item.path );
				f.setDirectWriteFallback( true );
				bool ok = f.open( QIODevice::WriteOnly );
				if ( ok ) {
					if ( f.write( item.bytes ) != item.bytes.size() ) {
						f.cancelWriting();
						ok = false;
					}
					ok = f.commit() && ok;
				}
				{
					QMutexLocker lock( &impl->mutex );
					impl->queued -= item.bytes.size();
					impl->count++;
					if ( !ok )
						impl->failed.append( item.path );
					impl->roomFree.wakeAll();
				}
			}
			tlsInWorker = false;
		}

	private:
		Impl * impl;
	};

	Thread * thread = nullptr;
};

LodgenWriter::LodgenWriter( qint64 maxBytes ) : d( new Impl )
{
	d->maxBytes = qMax( qint64( 1 ) << 20, maxBytes );
	d->thread = new Impl::Thread( d );
	d->thread->start();
}

LodgenWriter::~LodgenWriter()
{
	finish();
	delete d->thread;
	delete d;
}

void LodgenWriter::push( const QString & path, const QByteArray & bytes )
{
	QMutexLocker lock( &d->mutex );
	// bounded: a queue that never blocks holds the whole bake in memory
	while ( d->queued > 0 && d->queued + bytes.size() > d->maxBytes )
		d->roomFree.wait( &d->mutex );
	d->queue.push_back( Impl::Item{ path, bytes } );
	d->queued += bytes.size();
	d->workReady.wakeAll();
}

bool LodgenWriter::finish()
{
	if ( !d->thread )
		return d->failed.isEmpty();
	{
		QMutexLocker lock( &d->mutex );
		d->stopping = true;
		d->workReady.wakeAll();
	}
	d->thread->wait();          // THE DRAIN: nothing exits with a file unwritten
	QMutexLocker lock( &d->mutex );
	return d->failed.isEmpty();
}

QStringList LodgenWriter::failures() const
{
	QMutexLocker lock( &d->mutex );
	return d->failed;
}

int LodgenWriter::written() const
{
	QMutexLocker lock( &d->mutex );
	return d->count;
}
