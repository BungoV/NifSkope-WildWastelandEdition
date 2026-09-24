#!/usr/bin/env python3
"""RESUME3: make NIFPARSE1's `bound by` census word READABLE.

F7 (`fixes.py`, src/lodgenparallel.*) computes a memory cap for
`--chunk-threads 0` and records WHICH bound decided it -- "cores", "memory" or
"asked" -- in `lodgenChunkThreadBoundBy()`. Nothing called it: the word was
written and never printed, which is the exact shape CONSTITUTION 4's first rule
of 2026-09-04 21:33 forbids ("no counter, census field or status line ships
without a test that it is WRITTEN and that it MOVES").

This adds it to the one line that already reports the chunk pass, so a run held
back by memory says so in words instead of looking slow. No arithmetic changes.

    python census_boundby.py --check
    python census_boundby.py --apply
"""
import sys, os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PATH = os.path.join(ROOT, 'src', 'lodgenchunkpass.cpp')

OLD = (
b'\treturn QStringLiteral( "bake census: threads %1, chunk threads %2, chunk jobs %3, "\n'
b'\t\t"chunk workers %4, %5" )\n'
b'\t\t.arg( lodgenThreadCount() ).arg( lodgenChunkThreadCount() )\n'
b'\t\t.arg( g_lastJobs ).arg( g_lastWorkers ).arg( lodgenPeakWorkingSetLine() );\n'
)

NEW = (
b'\t/* `bound by` (lane RESUME3, 2026-09-11): which limit decided the chunk\n'
b'\t * worker count -- "asked" when --chunk-threads named a number, "cores" when\n'
b'\t * the machine did, "memory" when free RAM did. NIFPARSE1 computed the word\n'
b'\t * and nothing read it; a run capped by memory used to read as a slow run. */\n'
b'\treturn QStringLiteral( "bake census: threads %1, chunk threads %2 bound by %3, "\n'
b'\t\t"chunk jobs %4, chunk workers %5, %6" )\n'
b'\t\t.arg( lodgenThreadCount() ).arg( lodgenChunkThreadCount() )\n'
b'\t\t.arg( lodgenChunkThreadBoundBy() )\n'
b'\t\t.arg( g_lastJobs ).arg( g_lastWorkers ).arg( lodgenPeakWorkingSetLine() );\n'
)


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else '--check'
    b = open(PATH, 'rb').read()
    cr0 = b.count(b'\r')
    c = b.count(OLD)
    print('src/lodgenchunkpass.cpp  replace count=%d  (lodgenBakeCensusLine)' % c)
    if c != 1:
        print('RESULT REFUSED - the anchor matched %d times, not 1' % c)
        return 3
    nb = b.replace(OLD, NEW)
    print('src/lodgenchunkpass.cpp  CR before=%d after=%d  LF %d -> %d'
          % (cr0, nb.count(b'\r'), b.count(b'\n'), nb.count(b'\n')))
    if nb.count(b'\r') != cr0:
        print('RESULT REFUSED - CR count moved')
        return 4
    if mode == '--apply':
        open(PATH, 'wb').write(nb)
        print('wrote src/lodgenchunkpass.cpp')
        print('RESULT APPLIED - 1 edit')
    else:
        print('RESULT CHECK OK - 1 edit, anchor matched once, nothing written')
    return 0


if __name__ == '__main__':
    sys.exit(main())
