/***** BEGIN LICENSE BLOCK *****

BSD License - see nifskope.h

***** END LICENCE BLOCK *****/

#ifndef LODGENPARALLEL_H
#define LODGENPARALLEL_H

#include <QString>
#include <QtGlobal>

#include <functional>

/*! THE GENERATOR'S THREAD BUDGET, and the one way back.
 *
 *  Everything the LOD generator fans out goes through here, so there is ONE
 *  number to turn down and one place that says what it means.
 *
 *  `lodgenSetThreadCount( 1 )` (`--threads 1` on the command line) is the exact
 *  way back: `lodgenParallelFor` degenerates into the plain `for` loop the code
 *  had before this file existed, on the calling thread, and the chunk pass
 *  builds one chunk at a time in queue order. Nothing about the arithmetic
 *  depends on the count -- that is what the byte-identity gate proves.
 *
 *  0 or negative means "the machine": QThread::idealThreadCount().
 */
void lodgenSetThreadCount( int n );

//! The resolved count, always >= 1.
int lodgenThreadCount();

/*! THE CHUNK FAN-OUT'S OWN NUMBER, and it defaults to ONE.
 *
 *  Separate from `lodgenThreadCount()` because the two have different
 *  evidence behind them. The generic fan-out (the BC encoders) is pure
 *  arithmetic over disjoint output and is proven byte-identical. The CHUNK
 *  fan-out builds NifModels on worker threads, and the NifModel / NifItem /
 *  nif.xml layer is not safe for that: measured 2026-09-11, five runs out of
 *  five at nine workers faulted in `NifItem::deleteChildItems()` under
 *  `BaseModel::~BaseModel()` with every other worker inside the same parser
 *  (lane BAKEPERF1 §3.4). Until that is fixed the chunk queue runs one chunk
 *  at a time unless somebody asks otherwise, and the ask is `--chunk-threads`.
 *
 *  0 or negative means "the machine". */
void lodgenSetChunkThreadCount( int n );
int lodgenChunkThreadCount();

/*! WHAT THE MACHINE'S MEMORY CAN HOLD, in chunk workers.
 *
 *  Each chunk worker owns its OWN plugin reader and its OWN texture cache,
 *  so N workers is N x (world + budget) of fixed overhead before one chunk
 *  of state. `lodgenChunkThreadCount()` never returns more than this.
 *
 *  `texBudgetBytes` is the per-worker texture budget the pass will use.
 *  The world figure is a measured constant, not an estimate; the census
 *  line prints the cap and what bound it, so a run that was held back by
 *  memory rather than by cores says so in words.
 */
int lodgenChunkThreadMemoryCap( qint64 texBudgetBytes );

//! Which bound decided the worker count: "cores", "memory" or "asked".
QString lodgenChunkThreadBoundBy();

//! True while the caller is running inside one of the generator's own workers.
bool lodgenInWorker();

/*! Marks the calling thread as one of the generator's own workers for this
 *  scope, so any `lodgenParallelFor` underneath it runs SERIALLY.
 *
 *  A chunk fan-out already has every core; a second fan-out inside it does not
 *  find more, and creating its threads per mip of every sheet corrupted the
 *  heap on four runs out of four before this existed (lane BAKEPERF1,
 *  2026-09-11 -- bisected to the terrain texture stage). */
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
 *  giving it threads rather than to pretend. Measured, lane PERF1 2026-09-17,
 *  nine-chunk FO4CS bake, sixteen logical cores, seconds in the model-loading
 *  stage against `--threads`: 24.2 at 1, 18.8 at 2, 19.1 at 4, 24.0 at 6,
 *  27.1 at 8, 33.9 at 16 -- it is allocation-bound, not compute-bound, and past
 *  four workers it is SLOWER THAN SERIAL. The ladder stage over the same models
 *  in the same bake went 36.3, 19.7, 12.6, 10.4, 9.2, 8.3, so one number for
 *  both stages would have to throw one of them away.
 */
void lodgenParallelFor( int n, const std::function<void( int )> & f, int cap = 0 );

/*! Peak working set of THIS process, in bytes; 0 where the platform cannot say.
 *
 *  Printed at the end of a bake so a run that would not fit in the machine is
 *  visible as a number instead of as a swap storm.
 */
quint64 lodgenPeakWorkingSet();

//! "peak working set: N.N GB (N bytes)", the words the panel and the CLI share.
QString lodgenPeakWorkingSetLine();

/*! A bounded write queue on a thread of its own, so a worker hands over bytes
 *  and goes back to building instead of waiting on the disk.
 *
 *  BOUNDED on purpose: a 3,060-chunk bake would otherwise hold every chunk it
 *  has built and not yet written. `push` blocks once the queue holds
 *  `maxBytes`. `finish()` drains -- a run that returned 0 with a file still
 *  unwritten is worse than a slow run, so nothing may exit before it.
 */
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
