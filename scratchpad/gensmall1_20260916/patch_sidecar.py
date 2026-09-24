#!/usr/bin/env python
"""GENSMALL1: the mesh-report sidecar gains `casterInstances` (report v3 -> v4).

Written with Write, NOT a heredoc: a quoted heredoc still collapses `\\\\` to `\\`
on the way into python, so a C string's literal backslash-n cannot be matched
from one (`nifskope-ww-lodgen`, the editing traps).
"""
import sys

p = 'src/nativeemit.cpp'
b = open(p, 'rb').read()
before = (len(b), b.count(b'\r'), b.count(b'\n'))

BS_N = b'\\' + b'n'   # the two characters a C source spells a newline with

edits = [
    (b'\t\t/* Report version 2 (v3 format): thirteen ladder columns, inserted BEFORE\n'
     b'\t\t * `model` because the model path is the only token that may hold a\n'
     b'\t\t * space and must stay the line\'s remainder. */\n'
     b'\t\toutBytes += "# lodgen native mesh report 3 ws ";\n',
     b'\t\t/* Report version 4.  Every new column is inserted BEFORE `model`,\n'
     b'\t\t * because the model path is the only token that may hold a space and\n'
     b'\t\t * must stay the line\'s remainder.  v3 added the thirteen ladder\n'
     b'\t\t * columns; v4 adds `casterInstances`, the per-source caster count at\n'
     b'\t\t * mesh granularity (bungo 2026-09-11 14:4x). */\n'
     b'\t\toutBytes += "# lodgen native mesh report 4 ws ";\n'),

    (b'\t\t\t"refNoCut refFlat refSilhouette errExact errBounded weldedVerts uvConflicts boundaryCoarsest model'
     + BS_N + b'";\n',
     b'\t\t\t"refNoCut refFlat refSilhouette errExact errBounded weldedVerts uvConflicts boundaryCoarsest "\n'
     b'\t\t\t"casterInstances model' + BS_N + b'";\n'),

    (b'%18 %19 %20 %21 %22 %23 %24 %25' + BS_N + b'" )\n',
     b'%18 %19 %20 %21 %22 %23 %24 %25 %26' + BS_N + b'" )\n'),

    (b'\t\t\t\t.arg( ms.weldedVertices ).arg( ms.weldUvConflicts ).arg( ms.boundaryCoarsest )\n'
     b'\t\t\t\t.arg( meshStatPath[i] ).toUtf8();\n',
     b'\t\t\t\t.arg( ms.weldedVertices ).arg( ms.weldUvConflicts ).arg( ms.boundaryCoarsest )\n'
     b'\t\t\t\t.arg( i < meshCasters.size() ? meshCasters[i] : quint64( 0 ) )\n'
     b'\t\t\t\t.arg( meshStatPath[i] ).toUtf8();\n'),
]

for old, new in edits:
    n = b.count(old)
    if n != 1:
        sys.stderr.write('anchor appears %d times, refusing: %r\n' % (n, old[:60]))
        sys.exit(1)
    b = b.replace(old, new)

open(p, 'wb').write(b)
after = (len(b), b.count(b'\r'), b.count(b'\n'))
print('%s  bytes %d -> %d   CR %d -> %d   LF %d -> %d'
      % (p, before[0], after[0], before[1], after[1], before[2], after[2]))
