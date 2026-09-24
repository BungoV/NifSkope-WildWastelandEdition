#!/usr/bin/env python
"""GENSMALL1: the `native-casters:` census line, on its own prefix.

The house rule is stated in nativeemit.cpp itself: "The v3 rows go on their OWN
lines rather than into the sentence above: the census line is already at the
edge of readable, and a reader greps one prefix a subject."  So the per-source
caster counts get `native-casters:`, beside `native-ladder:` and
`native-occluders:`, and not a 36th clause of `native:`.
"""
import sys

p = 'src/nativeemit.cpp'
b = open(p, 'rb').read()
before = (len(b), b.count(b'\r'), b.count(b'\n'))

anchor = b'\t\tQString occLine = QString( "native-occluders: '
if b.count(anchor) != 1:
    sys.stderr.write('anchor appears %d times\n' % b.count(anchor))
    sys.exit(1)

add = (
    b'\t\t/* THE PER-SOURCE CASTER COUNTS, on their own prefix.  The four are a\n'
    b'\t\t * PARTITION of the instance table (bungo 2026-09-11 14:4x: "every\n'
    b'\t\t * placement has exactly ONE shadow representation at a time"), so the\n'
    b'\t\t * line states the sum law and whether it holds, rather than leaving a\n'
    b'\t\t * reader to add four numbers up and guess.  `none` is a fault word:\n'
    b'\t\t * an instance with no representation of any kind casts nothing. */\n'
    b'\t\tQString castLine = QString( "native-casters: per source, of %1 instances: "\n'
    b'\t\t\t"tree %2, card %3, mesh %4, none %5; sum %6 == instances %7 == %8; "\n'
    b'\t\t\t"mesh-slot casters %9 over %10 meshes (each instance once per distinct mesh its base names); "\n'
    b'\t\t\t"terrain march: runtime, FO4CS measures it" )\n'
    b'\t\t\t.arg( stats.instances ).arg( castTree ).arg( castCard ).arg( castMesh ).arg( castNone )\n'
    b'\t\t\t.arg( castTree + castCard + castMesh + castNone ).arg( stats.instances )\n'
    b'\t\t\t.arg( ( castTree + castCard + castMesh + castNone ) == quint64( stats.instances )\n'
    b'\t\t\t\t? QStringLiteral( "AGREE" ) : QStringLiteral( "DISAGREE" ) )\n'
    b'\t\t\t.arg( castMeshSlots ).arg( lib.meshes.size() );\n'
)
b = b.replace(anchor, add + anchor)

# and join it into the report, between the occluders and the aggregates
NL = b"QChar( '" + b'\\' + b"n' )"
emit = b'\t\t*report = line + ' + NL + b' + ladderLine + ' + NL + b' + occLine\n'
if b.count(emit) != 1:
    sys.stderr.write('report join appears %d times\n' % b.count(emit))
    sys.exit(2)
b = b.replace(emit, b'\t\t*report = line + ' + NL + b' + ladderLine + ' + NL + b' + occLine\n'
              b'\t\t\t+ ' + NL + b' + castLine\n')

open(p, 'wb').write(b)
after = (len(b), b.count(b'\r'), b.count(b'\n'))
print('%s  bytes %d -> %d   CR %d -> %d   LF %d -> %d   (emitted after %r)'
      % (p, before[0], after[0], before[1], after[1], before[2], after[2], emit.strip()))
