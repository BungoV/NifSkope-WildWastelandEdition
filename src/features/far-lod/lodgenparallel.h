/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef LODGENPARALLEL_H
#define LODGENPARALLEL_H

#include <QElapsedTimer>
#include <QString>
#include <QtGlobal>

#include <atomic>
#include <functional>

//! THE GENERATOR'S THREAD BUDGET, and the one way back:
//! Everything the LOD generator fans out goes through here, so there is ONE number to turn down and one place
//! that says what it means.
void lodgenSetThreadCount( int n );

//! The resolved count, always >= 1.
int lodgenThreadCount();

//! THE CHUNK FAN-OUT'S OWN NUMBER, and it defaults to ONE:
//! Separate from `lodgenThreadCount` because the two have different evidence behind them. The generic fan-out
//! (the BC encoders) is pure arithmetic over disjoint output and is proven byte-identical.
void lodgenSetChunkThreadCount( int n );
int lodgenChunkThreadCount();

//! WHAT THE MACHINE'S MEMORY CAN HOLD, in chunk workers:
//! Each chunk worker owns its OWN plugin reader and its OWN texture cache, so N workers is N x (world + budget)
//! of fixed overhead before one chunk of state. `lodgenChunkThreadCount()` never returns more than this.
int lodgenChunkThreadMemoryCap( qint64 texBudgetBytes );

//! Which bound decided the worker count: "cores", "memory" or "asked".
QString lodgenChunkThreadBoundBy();

//! True while the caller is running inside one of the generator's own workers.
bool lodgenInWorker();

//! Marks the calling thread as one of the generator's own workers for this scope, so any `lodgenParallelFor`
//! underneath it runs SERIALLY.
class LodgenWorkerScope
{
public:
	LodgenWorkerScope();
	~LodgenWorkerScope();
private:
	bool saved;
};

/*! Call f(i) for every i in [0,n).
 *
 *  Serial -- same thread, ascending i -- when the count is 1, when n is below
 *  the size floor (32: a small mip is a few hundred microseconds of work
 *  spread over sixteen dispatches), or when the caller is ALREADY inside a generator
 *  worker (a chunk fan-out has the cores already; a nested fan-out underneath
 *  it would only oversubscribe them).
 *  f must write disjoint outputs; nothing here orders the writes.
 *
 *  `cap` (0 = no cap) is a CEILING ON WORKERS FOR THIS FAN-OUT ONLY, never a
 *  floor and never a way past `--threads`: the count used is
 *  `min( lodgenThreadCount(), cap )`. It exists because not every stage scales
 *  the same way, and the honest answer to a stage that does not is to stop
 *  giving it threads rather than to pretend. Measured,
 *  nine-chunk FO4CS bake, sixteen logical cores, seconds in the model-loading
 *  stage against `--threads`: 24.2 at 1, 18.8 at 2, 19.1 at 4, 24.0 at 6,
 *  27.1 at 8, 33.9 at 16 -- it is allocation-bound, not compute-bound, and past
 *  four workers it is SLOWER THAN SERIAL. The ladder stage over the same models
 *  in the same bake went 36.3, 19.7, 12.6, 10.4, 9.2, 8.3, so one number for
 *  both stages would have to throw one of them away.
 */
void lodgenParallelFor( int n, const std::function<void( int )> & f, int cap = 0, int floor = 32 );

/*! as lodgenParallelFor, with no size floor, on a pool of its own whose threads are not
 *  generator workers, so a lodgenParallelFor inside f still fans out (a few big items keep every core busy).
 *  Serial where lodgenParallelFor is; WW_LODGEN_BAKESPEED_RED=1 is exactly lodgenParallelFor. */
void lodgenParallelForOuter( int n, const std::function<void( int )> & f );

/*! Peak working set of THIS process, in bytes; 0 where the platform cannot say.
 *
 *  Printed at the end of a bake so a run that would not fit in the machine is
 *  visible as a number instead of as a swap storm.
 */
quint64 lodgenPeakWorkingSet();

//! "peak working set: N.N GB (N bytes)", the words the panel and the CLI share.
QString lodgenPeakWorkingSetLine();

//! with WW_LODGEN_MEMLOG set, one "mem: <phase> | private, peak private, ws" line on stderr.
//! the red: WW_LODGEN_BAKESPEED_RED=1 runs the stages it made parallel serially, as before
bool lodgenBakeSpeedRed();
bool lodgenMemLogOn();
void lodgenMemNote( const QString & phase, const QString & extra = QString() );

/*! the CPU block encoders' share, for WW_LODGEN_MEMLOG. Each BC1/BC3/BC4/BC5/BC7 block loop adds
 *  its wall time here; loops on several threads at once add up, so the total reads as CPU seconds. */
extern std::atomic<qint64> g_lodgenBcNs;
struct LodgenBcTimer
{
	QElapsedTimer t;
	LodgenBcTimer() { t.start(); }
	~LodgenBcTimer() { g_lodgenBcNs += t.nsecsElapsed(); }
};

/*! drop the pages of this process's MAPPED FILES (the plugins, the archives) from its working
 *  set. They stay in the file cache, so a later read is a soft fault; private memory is not touched. Returns
 *  the mapped bytes visited. A no-op under WW_LODGEN_BAKEMEM_RED=1. */
qint64 lodgenTrimMappedWorkingSet();

//! While alive, a thread calls lodgenTrimMappedWorkingSet() every `seconds`.
class LodgenMappedTrim
{
public:
	explicit LodgenMappedTrim( int seconds = 5 );
	~LodgenMappedTrim();
private:
	struct Impl;
	Impl * d;
};

//! A bounded write queue on a thread of its own, so a worker hands over bytes and goes back to building instead
//! of waiting on the disk.
class LodgenWriter
{
public:
	explicit LodgenWriter( qint64 maxBytes = qint64( 256 ) << 20 );
	~LodgenWriter();
	//! Queue `bytes` to be written at `path`. Blocks while the queue is full.
	void push( const QString & path, const QByteArray & bytes );
	//! Drain the queue and stop the thread. Returns false if any write failed.
	bool finish();
	//! The paths that failed, once finish() has run.
	QStringList failures() const;
	//! How many files went through the queue.
	int written() const;

private:
	struct Impl;
	Impl * d;
};

#endif // LODGENPARALLEL_H
