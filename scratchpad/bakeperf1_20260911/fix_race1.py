"""BAKEPERF1, fix 1 after the first multi-thread run took an access violation.

`setupArrayPseudonyms()` (src/model/nifmodel.cpp) fills three file-scope QHashes
the FIRST time a NifModel is constructed, guarded only by `if
(!arrayPseudonyms.isEmpty()) return;`. In the GUI that first construction has
always happened on the main thread. Under the chunk fan-out sixteen workers
construct their first NifModel at once, every one of them sees the hash empty,
and sixteen of them insert into it together.

A QMutex at the top makes the guard mean what it says. It is taken once per
NifModel construction, which is already inside the recursive lock the resource
map takes, and a NifModel construction is microseconds against a chunk.
"""
P = 'src/model/nifmodel.cpp'
s = open(P, encoding='utf-8', newline='').read()

old = """void setupArrayPseudonyms()
{
	if ( !arrayPseudonyms.isEmpty() )
		return;
"""
new = """void setupArrayPseudonyms()
{
	/* THE FIRST NifModel USED TO BE BUILT ON THE MAIN THREAD, ALWAYS.
	 * The LOD generator's chunk pass (lodgenchunkpass.h) builds them on
	 * workers, so sixteen threads can reach this empty hash at once and all
	 * sixteen fill it. One mutex; taken once per NifModel construction, which
	 * is microseconds against a chunk. (lane BAKEPERF1, 2026-09-11) */
	static QMutex pseudonymGuard;
	QMutexLocker pseudonymLock( &pseudonymGuard );
	if ( !arrayPseudonyms.isEmpty() )
		return;
"""
assert s.count(old) == 1, s.count(old)
s = s.replace(old, new)

if '#include <QMutex>' not in s:
    anchor = '//! @file nifmodel.cpp The NIF data model.\n'
    assert s.count(anchor) == 1
    s = s.replace(anchor, anchor + '\n#include <QMutex>\n#include <QMutexLocker>\n')

open(P, 'w', encoding='utf-8', newline='').write(s)
print('nifmodel guarded')
