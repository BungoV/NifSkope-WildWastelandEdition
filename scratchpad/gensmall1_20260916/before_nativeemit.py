#!/usr/bin/env python
"""Recover the BEFORE byte/CR/LF counts for src/nativeemit.cpp.

The file is untracked, so git holds no earlier copy; the before state is
reconstructed by reverse-applying this lane's three edits and then CHECKED
against the arithmetic below.  A reconstruction that does not land exactly on
that number is refused rather than reported.
"""
import sys

# 57,196 is the pre-lane size; the 59,204 a patch script printed as its own
# "before" was an INTERMEDIATE -- the caster-counting block (2,008 B, 47 lines)
# had already gone in by hand.  57,196 + 2,008 = 59,204 exactly, which is what
# makes this reconstruction a measurement rather than a story.
LAUNCH_BYTES = 57196

b = open('src/nativeemit.cpp', 'rb').read()
after = (len(b), b.count(b'\r'), b.count(b'\n'))

BS_N = b'\\' + b'n'

# ---- edit 3 (reverse): the castLine block and its join into the report
lines = b.split(b'\n')
# the counting block: from the comment opener to the closing brace of the walk
i = next(k for k, l in enumerate(lines) if b'THE PER-SOURCE CASTER COUNTS (bungo' in l)
j = next(k for k, l in enumerate(lines) if k > i and l == b'\t}')
del lines[i:j + 1]
# the castLine build, on its own prefix
i = next(k for k, l in enumerate(lines) if b'THE PER-SOURCE CASTER COUNTS, on their own prefix' in l)
j = next(k for k, l in enumerate(lines) if k > i and l.endswith(b'.arg( castMeshSlots ).arg( lib.meshes.size() );'))
del lines[i:j + 1]
# the join
i = next(k for k, l in enumerate(lines) if l.strip() == b"+ QChar( '" + b'\\' + b"n' ) + castLine")
del lines[i]
b = b'\n'.join(lines)

# ---- edit 2 (reverse): the sidecar, v4/26 back to v3/25
rev = [
    (b'\t\t/* Report version 4.  Every new column is inserted BEFORE `model`,\n'
     b'\t\t * because the model path is the only token that may hold a space and\n'
     b'\t\t * must stay the line\'s remainder.  v3 added the thirteen ladder\n'
     b'\t\t * columns; v4 adds `casterInstances`, the per-source caster count at\n'
     b'\t\t * mesh granularity (bungo 2026-09-11 14:4x). */\n'
     b'\t\toutBytes += "# lodgen native mesh report 4 ws ";\n',
     b'\t\t/* Report version 2 (v3 format): thirteen ladder columns, inserted BEFORE\n'
     b'\t\t * `model` because the model path is the only token that may hold a\n'
     b'\t\t * space and must stay the line\'s remainder. */\n'
     b'\t\toutBytes += "# lodgen native mesh report 3 ws ";\n'),
    (b'\t\t\t"refNoCut refFlat refSilhouette errExact errBounded weldedVerts uvConflicts boundaryCoarsest "\n'
     b'\t\t\t"casterInstances model' + BS_N + b'";\n',
     b'\t\t\t"refNoCut refFlat refSilhouette errExact errBounded weldedVerts uvConflicts boundaryCoarsest model'
     + BS_N + b'";\n'),
    (b'%18 %19 %20 %21 %22 %23 %24 %25 %26' + BS_N + b'" )\n',
     b'%18 %19 %20 %21 %22 %23 %24 %25' + BS_N + b'" )\n'),
    (b'\t\t\t\t.arg( i < meshCasters.size() ? meshCasters[i] : quint64( 0 ) )\n', b''),
]
for old, new in rev:
    n = b.count(old)
    if n != 1:
        sys.stderr.write('reverse anchor appears %d times: %r\n' % (n, old[:60]))
        sys.exit(1)
    b = b.replace(old, new)

before = (len(b), b.count(b'\r'), b.count(b'\n'))
print('reconstructed before: bytes %d  CR %d  LF %d' % before)
print('after (on disk):      bytes %d  CR %d  LF %d' % after)
if before[0] != LAUNCH_BYTES:
    sys.stderr.write('REFUSED: reconstruction is %d B, the launch measurement was %d B\n'
                     % (before[0], LAUNCH_BYTES))
    sys.exit(2)
print('AGREES: %d + 2008 (the counting block) = %d, the size the sidecar patch'
      ' printed as its own before' % (LAUNCH_BYTES, LAUNCH_BYTES + 2008))
