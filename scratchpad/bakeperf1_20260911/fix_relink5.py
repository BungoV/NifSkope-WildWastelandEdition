"""BAKEPERF1 relink 5: the generic fan-out gets a PERSISTENT pool.

MEASURED, Sanctuary 9 chunks, chunk queue serial both times:

    rung exe              textures 5.2 s
    new exe, relink 4     textures 8.0 s      <- 54 percent SLOWER

`lodgenParallelFor` created a fresh QThread per call and joined it at the end.
The BC writers call it once per MIP of every sheet, so a 512-square sheet with
five fan-out-sized mips costs seventy-five thread creations, several sheets a
chunk, nine chunks -- thousands of creations, each hundreds of microseconds,
for block rows that take less than that to encode. The fan-out was a NET LOSS
and the stage-time table is what showed it.

A persistent QThreadPool reuses its threads (expiry disabled), so the cost of a
fan-out is a queue push and a semaphore instead of a CreateThread. Nested use
is still suppressed by `tlsInWorker`, so a chunk worker never pushes into the
pool and it can never saturate against itself.
"""
P = 'src/lodgenparallel.cpp'
s = open(P, encoding='utf-8', newline='').read()

old = """/*! One worker of a parallel-for. A plain QThread rather than QThreadPool: the
 *  pool is a process-wide singleton whose size other code may have opinions
 *  about, and a bake wants to own its own workers for the length of one pass. */
class ForThread final : public QThread
{
public:
	ForThread( int n, std::atomic<int> * next, const std::function<void( int )> * f )
		: total( n ), cursor( next ), fn( f ) {}

protected:
	void run() override
	{
		tlsInWorker = true;
		for ( ;; ) {
			const int i = cursor->fetch_add( 1 );
			if ( i >= total )
				break;
			( *fn )( i );
		}
		tlsInWorker = false;
	}

private:
	int total;
	std::atomic<int> * cursor;
	const std::function<void( int )> * fn;
};

} // namespace"""
new = """/*! The fan-out's own pool, PERSISTENT.
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

} // namespace"""
assert s.count(old) == 1, ('thread', s.count(old))
s = s.replace(old, new)

old2 = """	const int workers = qMin( t, n );
	std::atomic<int> next{ 0 };
	QVector<ForThread *> threads;
	threads.reserve( workers - 1 );
	for ( int w = 0; w < workers - 1; w++ ) {
		ForThread * th = new ForThread( n, &next, &f );
		threads.append( th );
		th->start();
	}
	// the calling thread takes a share as well, so `--threads 2` really is two
	{
		const bool saved = tlsInWorker;
		tlsInWorker = true;
		for ( ;; ) {
			const int i = next.fetch_add( 1 );
			if ( i >= n )
				break;
			f( i );
		}
		tlsInWorker = saved;
	}
	for ( ForThread * th : threads ) {
		th->wait();
		delete th;
	}
}"""
new2 = """	const int workers = qMin( t, n );
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
}"""
assert s.count(old2) == 1, ('loop', s.count(old2))
s = s.replace(old2, new2)

# LodgenWorkerScope is defined below the anonymous namespace; move the two
# definitions above it by declaring the class methods before first use is not
# needed -- the class is declared in the header, which is already included.
old3 = """#include <QThread>
#include <QVector>
#include <QWaitCondition>"""
new3 = """#include <QRunnable>
#include <QSemaphore>
#include <QThread>
#include <QThreadPool>
#include <QVector>
#include <QWaitCondition>"""
assert s.count(old3) == 1, ('inc', s.count(old3))
s = s.replace(old3, new3)
open(P, 'w', encoding='utf-8', newline='').write(s)
b = open(P, 'rb').read()
print('%s CR=%d LF=%d' % (P, b.count(b'\r'), b.count(b'\n')))
print('relink 5 patch applied')

# ---- the size floor goes from 8 to 32 -------------------------------------
# Measured with the per-call threads: the small mips were most of the churn.
# A mip with fewer than 32 block rows is a few hundred microseconds of work
# spread over sixteen dispatches; it stays on the calling thread.
s2 = open('src/lodgenparallel.cpp', encoding='utf-8', newline='').read()
oldf = "	if ( n < 8 || t <= 1 || tlsInWorker ) {"
newf = "	if ( n < 32 || t <= 1 || tlsInWorker ) {"
assert s2.count(oldf) == 1, ('floor', s2.count(oldf))
s2 = s2.replace(oldf, newf)
open('src/lodgenparallel.cpp', 'w', encoding='utf-8', newline='').write(s2)

s3 = open('src/lodgenparallel.h', encoding='utf-8', newline='').read()
oldh = "the size floor (8: a four-row mip is not worth a thread, and the small mips"
newh = "the size floor (32: a small mip is a few hundred microseconds of work"
assert s3.count(oldh) == 1, ('floorh', s3.count(oldh))
s3 = s3.replace(oldh, newh)
oldh2 = " *  were most of the churn), or when the caller is ALREADY inside a generator"
newh2 = " *  spread over sixteen dispatches), or when the caller is ALREADY inside a generator"
assert s3.count(oldh2) == 1, ('floorh2', s3.count(oldh2))
s3 = s3.replace(oldh2, newh2)
open('src/lodgenparallel.h', 'w', encoding='utf-8', newline='').write(s3)
print('size floor raised to 32')
