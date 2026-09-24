#!/usr/bin/env python3
"""RESUME3: the `bound by` census word said "asked" when nobody asked.

Caught by READING the field on a real bake instead of trusting it: a run with no
`--chunk-threads` on the command line printed

    bake census: threads 16, chunk threads 1 bound by asked, ...

because `g_chunkThreads` is initialised to 1 and `lodgenChunkThreadCount()` reads
any positive value as "the caller asked for it". Four words are needed, not
three: "default" is the shipped 1 that nobody chose. `fo4cs-census-field`: a
field that cannot tell one state from another is not a field.

    python boundby_default.py --check
    python boundby_default.py --apply
"""
import sys, os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PATH = os.path.join(ROOT, 'src', 'lodgenparallel.cpp')
NL = chr(10)
TAB = chr(9)

OLD_SET = (
'void lodgenSetChunkThreadCount( int n )' + NL +
'{' + NL +
TAB + 'g_chunkThreads = n;' + NL +
'}' + NL)

NEW_SET = (
'//! Set true by the CLI/panel only, so the census can tell the SHIPPED default' + NL +
'//! apart from a 1 somebody typed (lane RESUME3, 2026-09-11).' + NL +
'static bool g_chunkThreadsAsked = false;' + NL +
NL +
'void lodgenSetChunkThreadCount( int n )' + NL +
'{' + NL +
TAB + 'g_chunkThreads = n;' + NL +
TAB + 'g_chunkThreadsAsked = true;' + NL +
'}' + NL)

OLD_GET = (
TAB + 'int n = g_chunkThreads;' + NL +
TAB + 'if ( n > 0 ) {' + NL +
TAB + TAB + 'g_chunkBoundBy = QStringLiteral( "asked" );' + NL +
TAB + TAB + 'return n;' + NL +
TAB + '}' + NL)

NEW_GET = (
TAB + 'int n = g_chunkThreads;' + NL +
TAB + 'if ( n > 0 ) {' + NL +
TAB + TAB + 'g_chunkBoundBy = g_chunkThreadsAsked ? QStringLiteral( "asked" )' + NL +
TAB + TAB + TAB + ': QStringLiteral( "default" );' + NL +
TAB + TAB + 'return n;' + NL +
TAB + '}' + NL)


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else '--check'
    b = open(PATH, 'rb').read()
    cr0 = b.count(b'\r')
    ok = True
    nb = b
    for label, old, new in (('lodgenSetChunkThreadCount', OLD_SET, NEW_SET),
                            ('lodgenChunkThreadCount', OLD_GET, NEW_GET)):
        c = nb.count(old.encode('utf-8'))
        print('src/lodgenparallel.cpp  replace count=%d  %s' % (c, label))
        if c != 1:
            ok = False
            print('   REFUSE: matched %d times, not 1' % c)
            continue
        nb = nb.replace(old.encode('utf-8'), new.encode('utf-8'), 1)
    print('src/lodgenparallel.cpp  CR before=%d after=%d  bytes %d -> %d'
          % (cr0, nb.count(b'\r'), len(b), len(nb)))
    if nb.count(b'\r') != cr0:
        ok = False
        print('   REFUSE: line endings changed')
    if not ok:
        print(NL + 'RESULT REFUSED - nothing written')
        return 2
    if mode == '--apply':
        open(PATH, 'wb').write(nb)
        print('wrote src/lodgenparallel.cpp')
        print(NL + 'RESULT APPLIED - 2 edits')
    else:
        print(NL + 'RESULT CHECK OK - 2 edits, nothing written')
    return 0


if __name__ == '__main__':
    sys.exit(main())
