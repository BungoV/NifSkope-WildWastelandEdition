#!/usr/bin/env python3
"""RESUME3, THE FIX THE VERDICT NAMES: the headless CLI's message handler is
written from worker threads and it writes through a SHARED QTextStream.

THE MEASUREMENT THAT NAMES IT (report section 1). Four symbolised faults, all
under `ChunkThread::run -> runJob -> lodgenBakeTerrainTextures ->
LodgenRoadSet::gather -> addPlacement -> lodgenLoadModel`. Two of the four are
INSIDE `cliMessageHandler`, reached from
`GameResources::get_file` -> `qWarning()`; the other two are an innocent
`QList<Vector3>` / `QList<Color4>` reallocation in `lodgenLoadModel` that simply
hit the already-corrupt heap first. `err()` is a function-local
`static QTextStream` (`src/nifcli.cpp:100-103`) with no lock anywhere, and
QTextStream keeps a QString write buffer it grows in place -- sixteen workers
missing the same `.bgsm` append to one buffer at once and the heap goes.

The model layer is NOT the fault: `parsestress` at 16 threads, 20 runs,
10,240 loads, 0 digest mismatches, 0 faults, with both of its floors fired.

THE FIX. The handler stops touching the Qt stream and writes the line with the
CRT's own `fputs`, which locks the `FILE *` -- under one mutex so the text and
its newline cannot interleave. Nothing else in the file changes: the main
thread's `out()` / `err()` are untouched, and a single-threaded run prints
exactly the same bytes in exactly the same order.

    python fix_clilog.py --check
    python fix_clilog.py --apply
"""
import sys, os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PATH = os.path.join(ROOT, 'src', 'nifcli.cpp')
NL = chr(10)
TAB = chr(9)

OLD = (
"//! Silence Qt's chatter; the CLI's own output is the product." + NL +
"void cliMessageHandler( QtMsgType type, const QMessageLogContext &, const QString & msg )" + NL +
"{" + NL +
TAB + "if ( type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg )" + NL +
TAB + TAB + "err() << msg << Qt::endl;" + NL +
"}" + NL
).encode('utf-8')

NEW = (
"/* Silence Qt's chatter; the CLI's own output is the product." + NL +
" *" + NL +
" * THIS HANDLER IS CALLED FROM WORKER THREADS, AND IT USED TO WRITE THROUGH" + NL +
" * err() -- a function-local static QTextStream with no lock (lane RESUME3," + NL +
" * 2026-09-11). QTextStream is not reentrant: it grows one QString write" + NL +
" * buffer in place. A `-no-gui lodgen --chunk-threads 16` bake reaches here" + NL +
" * from every worker at once through qWarning() in" + NL +
" * GameResources::get_file -- one warning per missing .bgsm, and the road" + NL +
" * pass misses the same material on every placement -- and the heap went:" + NL +
" * 0xC0000374 on 3 of 5 bare Sanctuary runs, with two of four symbolised" + NL +
" * faults taken INSIDE this function and the other two in an innocent" + NL +
" * QList reallocation that reached the corrupted heap first." + NL +
" *" + NL +
" * The CRT locks the FILE *, so fputs from many threads is safe; the mutex is" + NL +
" * only so a line and its newline cannot be split. On one thread the bytes" + NL +
" * and their order are exactly what err() produced -- that is the way back." + NL +
" */" + NL +
"void cliMessageHandler( QtMsgType type, const QMessageLogContext &, const QString & msg )" + NL +
"{" + NL +
TAB + "if ( type != QtWarningMsg && type != QtCriticalMsg && type != QtFatalMsg )" + NL +
TAB + TAB + "return;" + NL +
TAB + "static QMutex cliLogMutex;" + NL +
TAB + "const QByteArray line = msg.toLocal8Bit();" + NL +
TAB + "QMutexLocker lock( &cliLogMutex );" + NL +
TAB + "std::fputs( line.constData(), stderr );" + NL +
TAB + "std::fputc( '" + chr(92) + "n', stderr );" + NL +
"}" + NL
).encode('utf-8')

# the include the mutex needs, placed beside the other Qt includes
INC_OLD = '#include <QCoreApplication>' + NL
INC_NEW = ('#include <QCoreApplication>' + NL
           + '#include <QMutex>' + NL
           + '#include <cstdio>' + NL)


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else '--check'
    b = open(PATH, 'rb').read()
    cr0, lf0, n0 = b.count(b'\r'), b.count(b'\n'), len(b)
    edits = [('the handler', OLD, NEW), ('the includes', INC_OLD.encode(), INC_NEW.encode())]
    ok = True
    nb = b
    for label, old, new in edits:
        c = nb.count(old)
        print('src/nifcli.cpp  replace count=%d  %s' % (c, label))
        if c != 1:
            ok = False
            print('   REFUSE: the anchor matched %d times, not 1' % c)
            continue
        nb = nb.replace(old, new, 1)
    cr1 = nb.count(b'\r')
    print('src/nifcli.cpp  CR before=%d after=%d   LF %d -> %d   bytes %d -> %d'
          % (cr0, cr1, lf0, nb.count(b'\n'), n0, len(nb)))
    if cr1 != cr0:
        ok = False
        print('   REFUSE: line endings changed')
    if not ok:
        print(NL + 'RESULT REFUSED - nothing written')
        return 2
    if mode == '--apply':
        open(PATH, 'wb').write(nb)
        print('wrote src/nifcli.cpp')
        print(NL + 'RESULT APPLIED - 2 edits')
    else:
        print(NL + 'RESULT CHECK OK - 2 edits, both anchors matched once, nothing written')
    return 0


if __name__ == '__main__':
    sys.exit(main())
