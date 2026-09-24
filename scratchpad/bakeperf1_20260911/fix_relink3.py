"""BAKEPERF1 relink 3, two changes.

(1) A HEADLESS RUN MUST NEVER PUT A WINDOW ON HIS DESKTOP.

    2026-09-11 14:2x: six Windows "Application Error" dialogs reached bungo
    while this lane was bisecting a crash in `-no-gui lodgen`. A headless run
    that pops a dialog is a regression whatever else it does (CONSTITUTION 6:
    every window on the second monitor, never SetForegroundWindow -- a modal
    error box obeys neither). `initModelLayer()` is the first thing the
    `-no-gui` path does, so the error mode is set there, for this process and
    for anything it starts.

(2) EVERY WORKER GETS THE WHOLE TEXTURE BUDGET.

    Dividing 512 MB by the worker count was this lane's invention. It is the
    ONE behavioural difference between the one-thread path and the many-thread
    path beyond scheduling, and the crash boundary lands exactly where the
    division hits its 64 MB floor: 2 workers (256 MB) clean, 4 (128 MB) clean,
    9 (64 MB) heap corruption. A 64 MB cache holds three 2048-square landscape
    diffuses, so it thrashes -- which is both a slowdown and the condition
    under which the LRU's eviction runs on almost every lookup.

    The cost is memory: 16 workers x 512 MB of texture cache is 8 GB worst
    case, against 31 GB on this machine. The bake census line prints the peak
    working set so that number is never a guess.
"""
P = 'src/nifcli.cpp'
s = open(P, encoding='utf-8', newline='').read()

old = """bool initModelLayer()
{
	QCoreApplication::setOrganizationName( "NifTools" );"""
new = """bool initModelLayer()
{
#ifdef Q_OS_WIN32
	/* NO CRASH DIALOG FROM A HEADLESS RUN (2026-09-11, lane BAKEPERF1).
	 *
	 * Six Windows "Application Error" boxes reached bungo's desktop while this
	 * lane was bisecting a fault in a `-no-gui lodgen` bake. A headless run has
	 * no business showing a window at all, and a modal error box obeys neither
	 * the second-monitor rule nor the never-foreground one. The mode is
	 * inherited by anything this process starts, so a driver script gets it
	 * too. A crash still fails the run and still sets the exit code -- what
	 * goes away is the dialog. */
	SetErrorMode( SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX
		| SEM_NOALIGNMENTFAULTEXCEPT | SEM_NOOPENFILEERRORBOX );
#endif
	QCoreApplication::setOrganizationName( "NifTools" );"""
assert s.count(old) == 1, ('init', s.count(old))
s = s.replace(old, new)

if '#include <windows.h>' not in s:
    anchor = '#include "nifcli.h"\n'
    assert s.count(anchor) == 1
    s = s.replace(anchor, anchor + '\n#ifdef Q_OS_WIN32\n#include <windows.h>\n#endif\n')

open(P, 'w', encoding='utf-8', newline='').write(s)

P = 'src/lodgenchunkpass.cpp'
s = open(P, encoding='utf-8', newline='').read()
old = """	const qint64 budgetEach = qMax( qint64( 64 ) << 20, opts.texBudgetBytes / threads );
"""
assert s.count(old) == 1, ('budget', s.count(old))
s = s.replace(old, '')
old2 = """		// one worker holds the whole budget, so the serial path decodes and
		// evicts exactly what it did before
		w->caches = lodgenCreateBakeCaches( threads == 1 ? opts.texBudgetBytes : budgetEach );
"""
new2 = """		/* EVERY worker gets the WHOLE budget, not a share of it. Dividing it
		 * was this lane's own idea and it is the one behavioural difference
		 * between one thread and many beyond scheduling: at nine workers the
		 * share hit its 64 MB floor, which holds three landscape diffuses and
		 * thrashes. The cost is memory -- 16 x 512 MB worst case against 31 GB
		 * -- and the census line prints the peak so it is never a guess. */
		w->caches = lodgenCreateBakeCaches( opts.texBudgetBytes );
"""
assert s.count(old2) == 1, ('budget2', s.count(old2))
s = s.replace(old2, new2)
open(P, 'w', encoding='utf-8', newline='').write(s)

for f in ('src/nifcli.cpp', 'src/lodgenchunkpass.cpp'):
    b = open(f, 'rb').read()
    print('%s CR=%d LF=%d' % (f, b.count(b'\r'), b.count(b'\n')))
print('relink 3 patch applied')
