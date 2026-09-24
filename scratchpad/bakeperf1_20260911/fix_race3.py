"""BAKEPERF1, fix 3: the nested fan-out, which is what corrupted the heap.

BISECTED, three runs a variant, at 16 threads on the 9-chunk region:

    full          crash crash crash     (0xC0000374, heap corruption)
    no --native   crash crash ok
    no --tex-dir  ok ok ok
    --no-ao       crash crash crash
    meshes only   ok ok ok

Dropping the terrain TEXTURE bake is the only thing that makes it clean, and
the only shared machinery in that stage is the DDS writers -- which is exactly
where this lane put `lodgenParallelFor`. `lodgenParallelFor` suppresses a
nested fan-out when the caller is already inside one of ITS OWN workers, but a
chunk worker is a `ChunkThread` and never set that flag. So every one of 16
chunk workers fanned the BC encoding out again, 15 fresh QThreads per MIP of
every sheet: tens of thousands of thread creations and destructions, nested,
from non-main threads.

Two changes:

  1. `LodgenWorkerScope` -- an RAII marker that says "this thread is already one
     of the generator's workers". `ChunkThread::run()` opens one, so the BC
     encoders underneath a chunk fan-out run serially. The cores are already
     busy; a nested fan-out could only oversubscribe them.

  2. A SIZE FLOOR in `lodgenParallelFor`: below 8 items it runs serially. A
     4x4-block mip is not worth a thread, and the small mips were most of the
     churn.
"""
import os

BS = chr(92)

# ---- 1. the header gets the scope ----------------------------------------
P = 'src/lodgenparallel.h'
s = open(P, encoding='utf-8', newline='').read()
old = """//! True while the caller is running inside one of the generator's own workers.
bool lodgenInWorker();
"""
new = """//! True while the caller is running inside one of the generator's own workers.
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
"""
assert s.count(old) == 1, ('h', s.count(old))
s = s.replace(old, new)

old2 = """ *  Serial -- same thread, ascending i -- when the count is 1, when n is 1, or
 *  when the caller is ALREADY inside a generator worker (a chunk fan-out has
 *  the cores already; a nested fan-out underneath it would only oversubscribe).
 *  f must write disjoint outputs; nothing here orders the writes.
 */"""
new2 = """ *  Serial -- same thread, ascending i -- when the count is 1, when n is below
 *  the size floor (8: a four-row mip is not worth a thread, and the small mips
 *  were most of the churn), or when the caller is ALREADY inside a generator
 *  worker (a chunk fan-out has the cores already; a nested fan-out underneath
 *  it would only oversubscribe them).
 *  f must write disjoint outputs; nothing here orders the writes.
 */"""
assert s.count(old2) == 1, ('h2', s.count(old2))
s = s.replace(old2, new2)
open(P, 'w', encoding='utf-8', newline='').write(s)

# ---- 2. the implementation ------------------------------------------------
P = 'src/lodgenparallel.cpp'
s = open(P, encoding='utf-8', newline='').read()
old = """bool lodgenInWorker()
{
	return tlsInWorker;
}
"""
new = """bool lodgenInWorker()
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
"""
assert s.count(old) == 1, ('c', s.count(old))
s = s.replace(old, new)

old2 = """	const int t = lodgenThreadCount();
	if ( n == 1 || t <= 1 || tlsInWorker ) {"""
new2 = """	const int t = lodgenThreadCount();
	// THE SIZE FLOOR: below eight items the threads cost more than the work
	if ( n < 8 || t <= 1 || tlsInWorker ) {"""
assert s.count(old2) == 1, ('c2', s.count(old2))
s = s.replace(old2, new2)
open(P, 'w', encoding='utf-8', newline='').write(s)

# ---- 3. the chunk worker declares itself ----------------------------------
P = 'src/lodgenchunkpass.cpp'
s = open(P, encoding='utf-8', newline='').read()
old = """	void run() override
	{
		for ( ;; ) {
			const int i = next->fetch_add( 1 );"""
new = """	void run() override
	{
		/* This thread IS one of the generator's workers: the BC encoders it
		 * reaches must not fan out again underneath it. */
		LodgenWorkerScope workerScope;
		for ( ;; ) {
			const int i = next->fetch_add( 1 );"""
assert s.count(old) == 1, ('p', s.count(old))
s = s.replace(old, new)
open(P, 'w', encoding='utf-8', newline='').write(s)

for f in ('src/lodgenparallel.h', 'src/lodgenparallel.cpp', 'src/lodgenchunkpass.cpp'):
    b = open(f, 'rb').read()
    print('%s CR=%d LF=%d' % (f, b.count(b'\r'), b.count(b'\n')))
print('fix 3 applied')
