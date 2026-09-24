#!/usr/bin/env python
"""GENSMALL1: `lodgen_native.sh` learns the `native-casters:` line.

Two checks with floors on both sides:
  - the line exists and its four per-source counts SUM to the instance count
    (the partition law bungo's 14:4x ruling states), read out of the line's own
    AGREE/DISAGREE word so a reader and the gate cannot disagree;
  - the counts MOVE: the Sanctuary region has trees, so `tree` must be > 0 and
    strictly less than the instance total -- a constant or an all-in-one-bin
    census would fail both halves.
"""
import sys

p = 'tests/spells/lodgen_native.sh'
b = open(p, 'rb').read()
before = (len(b), b.count(b'\r'), b.count(b'\n'))

anchor = b'grep -E "^native: |^native-ladder: |^native-occluders: " "$W/bake_native.log" \n'
if b.count(anchor) != 1:
    sys.stderr.write('anchor appears %d times\n' % b.count(anchor))
    sys.exit(1)

new = (b'grep -E "^native: |^native-ladder: |^native-occluders: |^native-casters: " "$W/bake_native.log" \n'
       b'# the per-source caster counts (bungo 2026-09-11 14:4x). Two floors: the four\n'
       b'# bins must PARTITION the instance table, and they must not all sit in one bin.\n'
       b'CAST="$(grep -m1 "^native-casters: " "$W/bake_native.log")"\n'
       b'if [ -n "$CAST" ]; then note "the native-casters: line was printed"\n'
       b'else bad "the native-casters: line was printed"; fi\n'
       b'case "$CAST" in\n'
       b'\t*"== AGREE;"*) note "the four caster bins sum to the instance count (the line says AGREE)" ;;\n'
       b'\t*) bad "the four caster bins sum to the instance count: $CAST" ;;\n'
       b'esac\n'
       b'CTREE="$(printf \'%s\' "$CAST" | sed -n \'s/.*tree \\([0-9][0-9]*\\),.*/\\1/p\')"\n'
       b'CMESH="$(printf \'%s\' "$CAST" | sed -n \'s/.*, mesh \\([0-9][0-9]*\\),.*/\\1/p\')"\n'
       b'if [ -n "$CTREE" ] && [ "$CTREE" -gt 0 ] 2>/dev/null; then\n'
       b'\tnote "tree casters MOVE off zero on a region that has trees ($CTREE)"\n'
       b'else bad "tree casters MOVE off zero on a region that has trees (read \'$CTREE\')"; fi\n'
       b'if [ -n "$CMESH" ] && [ "$CMESH" -gt 0 ] 2>/dev/null; then\n'
       b'\tnote "mesh casters MOVE off zero, so the census is not all in one bin ($CMESH)"\n'
       b'else bad "mesh casters MOVE off zero, so the census is not all in one bin (read \'$CMESH\')"; fi\n')

b = b.replace(anchor, new)
open(p, 'wb').write(b)
after = (len(b), b.count(b'\r'), b.count(b'\n'))
print('%s  bytes %d -> %d   CR %d -> %d   LF %d -> %d'
      % (p, before[0], after[0], before[1], after[1], before[2], after[2]))
