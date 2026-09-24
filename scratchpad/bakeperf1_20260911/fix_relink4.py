"""BAKEPERF1 relink 4: the chunk fan-out becomes OPT-IN, and NIF parsing is
serialised so the opt-in path can be tested.

THE SYMBOLISED STACK (relinked without -Wl,-s for one diagnostic run, gdb on
the 25-chunk region at 16 threads) says where it is. Every one of the nine
worker threads was inside `lodgenLoadModel` -- parsing a road model into a
temporary NifModel -- and the faulting one was in `NifItem::deleteChildItems()`
underneath `BaseModel::~BaseModel()`:

    #0-#4  NifItem::deleteChildItems()          <- SIGSEGV, four levels deep
    #5     BaseModel::~BaseModel()
    #6     (anonymous)::lodgenLoadModel(...)
    #7     (anonymous)::LodgenRoadSet::addPlacement(...)
    #8     (anonymous)::LodgenRoadSet::gather(...)
    #9     lodgenBakeTerrainTextures(...)
    #10    (anonymous)::runJob(...)
    #11    (anonymous)::ChunkThread::run()

while the others sat in `BaseModel::getItemInternal`, `NifExpr::partition` under
`NifModel::updateArraySizeImpl`, `Transform::Transform`, `NifModel::get<>`.
That is the NifModel / NifItem / nif.xml layer, not this lane's code and not
the generator's caches.

So:

  1. `--chunk-threads N` is the chunk fan-out's own number and it DEFAULTS TO 1.
     A bake therefore behaves exactly as it did before this lane -- one world,
     one cache set, one chunk at a time -- unless somebody asks for more. A
     serial bake that works beats a parallel one that faults.
  2. `--threads N` keeps its meaning as the general fan-out budget (the BC
     encoders and anything added later) and still defaults to the machine.
  3. `lodgenLoadModel` takes a mutex for the whole life of its temporary
     NifModel, so NIF PARSING is serialised even when chunks are not. Whether
     that is enough to make `--chunk-threads 16` safe is a measurement, not a
     claim, and it is taken after this link.
"""
BS = chr(92)

# ---- 1. the thread budget gets a second number ---------------------------
P = 'src/lodgenparallel.h'
s = open(P, encoding='utf-8', newline='').read()
old = """//! The resolved count, always >= 1.
int lodgenThreadCount();
"""
new = """//! The resolved count, always >= 1.
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
"""
assert s.count(old) == 1, ('h', s.count(old))
s = s.replace(old, new)
open(P, 'w', encoding='utf-8', newline='').write(s)

P = 'src/lodgenparallel.cpp'
s = open(P, encoding='utf-8', newline='').read()
old = """int g_threads = 0;                          //!< 0 = the machine
"""
new = """int g_threads = 0;                          //!< 0 = the machine
int g_chunkThreads = 1;                     //!< ONE until the parser is safe
"""
assert s.count(old) == 1, ('c0', s.count(old))
s = s.replace(old, new)
old = """bool lodgenInWorker()
{
	return tlsInWorker;
}
"""
new = """void lodgenSetChunkThreadCount( int n )
{
	g_chunkThreads = n;
}

int lodgenChunkThreadCount()
{
	int n = g_chunkThreads;
	if ( n <= 0 )
		n = QThread::idealThreadCount();
	return qMax( 1, n );
}

bool lodgenInWorker()
{
	return tlsInWorker;
}
"""
assert s.count(old) == 1, ('c1', s.count(old))
s = s.replace(old, new)
open(P, 'w', encoding='utf-8', newline='').write(s)

# ---- 2. the chunk pass reads the chunk number ----------------------------
P = 'src/lodgenchunkpass.cpp'
s = open(P, encoding='utf-8', newline='').read()
old = "	const int threads = qBound( 1, lodgenThreadCount(), int( jobs.size() ) );"
new = "	const int threads = qBound( 1, lodgenChunkThreadCount(), int( jobs.size() ) );"
assert s.count(old) == 1, ('p', s.count(old))
s = s.replace(old, new)
open(P, 'w', encoding='utf-8', newline='').write(s)

# ---- 3. the census names both numbers ------------------------------------
P = 'src/lodgenchunkpass.cpp'
s = open(P, encoding='utf-8', newline='').read()
old = """	return QStringLiteral( "bake census: threads %1, chunk jobs %2, chunk workers %3, %4" )
		.arg( lodgenThreadCount() ).arg( g_lastJobs ).arg( g_lastWorkers )
		.arg( lodgenPeakWorkingSetLine() );"""
new = """	return QStringLiteral( "bake census: threads %1, chunk threads %2, chunk jobs %3, "
		"chunk workers %4, %5" )
		.arg( lodgenThreadCount() ).arg( lodgenChunkThreadCount() )
		.arg( g_lastJobs ).arg( g_lastWorkers ).arg( lodgenPeakWorkingSetLine() );"""
assert s.count(old) == 1, ('cen', s.count(old))
s = s.replace(old, new)
open(P, 'w', encoding='utf-8', newline='').write(s)

# ---- 4. NIF parsing is serialised ----------------------------------------
P = 'src/lodgen.cpp'
s = open(P, encoding='utf-8', newline='').read()
old = """const QVector<LodSrcShape> & lodgenLoadModel( const QString & dataRoot,
	const QString & meshPath, QHash<QString, QVector<LodSrcShape>> & cache )
{
	const QString key = meshPath.toLower();
	auto it = cache.constFind( key );
	if ( it != cache.constEnd() )
		return *it;

	QVector<LodSrcShape> shapes;"""
new = """const QVector<LodSrcShape> & lodgenLoadModel( const QString & dataRoot,
	const QString & meshPath, QHash<QString, QVector<LodSrcShape>> & cache )
{
	const QString key = meshPath.toLower();
	auto it = cache.constFind( key );
	if ( it != cache.constEnd() )
		return *it;

	/* NIF PARSING IS SERIALISED (lane BAKEPERF1, 2026-09-11).
	 *
	 * The NifModel / NifItem / nif.xml layer is not safe for two threads
	 * parsing two documents at once in this tree: with the chunk queue fanned
	 * out, five runs out of five faulted inside `NifItem::deleteChildItems()`
	 * under `BaseModel::~BaseModel()` here, while every other worker sat in
	 * `BaseModel::getItemInternal` / `NifExpr::partition` / `NifModel::get<>`.
	 * The lock covers the WHOLE life of the temporary document, construction
	 * to destruction, because the fault was in the destructor.
	 *
	 * Uncontended and free when the queue runs one chunk at a time, which is
	 * the default (`lodgenChunkThreadCount()`). It is a containment, not a
	 * fix: the layer itself is still unsafe and the cache above means each
	 * model is parsed once per chunk regardless. */
	static QMutex parseMutex;
	QMutexLocker parseLock( &parseMutex );

	QVector<LodSrcShape> shapes;"""
assert s.count(old) == 1, ('load', s.count(old))
s = s.replace(old, new)
if '#include <QMutex>' not in s:
    anchor = '#include "lodgenparallel.h"\n'
    assert s.count(anchor) == 1
    s = s.replace(anchor, anchor + '\n#include <QMutex>\n#include <QMutexLocker>\n')
open(P, 'w', encoding='utf-8', newline='').write(s)

# ---- 5. the CLI switch ----------------------------------------------------
P = 'src/nifcli.cpp'
s = open(P, encoding='utf-8', newline='').read()
old = '\t\telse if ( t == QLatin1String( "--threads" ) ) lodgenSetThreadCount( next().toInt() );\n'
assert s.count(old) == 1, ('cli', s.count(old))
new = old + ('\t\t/* The CHUNK queue\'s own number, default 1. See lodgenparallel.h:\n'
             '\t\t * building NifModels on worker threads is not safe in this tree\n'
             '\t\t * yet, so the chunk fan-out is opt-in and the default bake is\n'
             '\t\t * the serial one that has always run. */\n'
             '\t\telse if ( t == QLatin1String( "--chunk-threads" ) ) lodgenSetChunkThreadCount( next().toInt() );\n')
s = s.replace(old, new)

anchor = '\t\t  << "  lodgen ... --terrain-region ... [--threads N]' + BS + 'n"\n'
assert s.count(anchor) == 1, ('usage', s.count(anchor))
added = (
    '\t\t  << "  lodgen ... --terrain-region ... [--chunk-threads N]' + BS + 'n"\n'
    '\t\t  << "                                          how many chunks the queue builds' + BS + 'n"\n'
    '\t\t  << "                                          at once. DEFAULT 1: building NIF' + BS + 'n"\n'
    '\t\t  << "                                          documents on worker threads is not' + BS + 'n"\n'
    '\t\t  << "                                          safe in this tree yet (it faults in' + BS + 'n"\n'
    '\t\t  << "                                          the parser), so the fan-out is' + BS + 'n"\n'
    '\t\t  << "                                          opt-in and unproven' + BS + 'n"\n'
)
s = s.replace(anchor, added + anchor)
open(P, 'w', encoding='utf-8', newline='').write(s)

for f in ('src/lodgenparallel.h', 'src/lodgenparallel.cpp', 'src/lodgenchunkpass.cpp',
          'src/lodgen.cpp', 'src/nifcli.cpp'):
    b = open(f, 'rb').read()
    print('%s CR=%d LF=%d' % (f, b.count(b'\r'), b.count(b'\n')))
print('relink 4 patch applied')
