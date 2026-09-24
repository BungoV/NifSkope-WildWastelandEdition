"""IMPOSTORFIX3 -- section 11 appended to `ww-reference-card-diagnose`, then
the file copied to the SECOND skill tree, which is stale.

The repo copy carries IMPOSTORFIX2's sections 7..10; `E:/Projects/Claude/
.claude/skills/ww-reference-card-diagnose/SKILL.md` is still IMPOSTORFIX1's
9,163-byte version. Both are brought to the same bytes here.
"""
import sys, shutil

A = 'E:/Projects/NifskopeWildWastelandEdition/.claude/skills/ww-reference-card-diagnose/SKILL.md'
B = 'E:/Projects/Claude/.claude/skills/ww-reference-card-diagnose/SKILL.md'

TAIL = """
## 11. Make the simulation real, then publish both columns (IMPOSTORFIX3, 2026-09-19)

Sections 4 and 8 get you a candidate table: six fills x five subjects, scored
offline, a winner. That table is a PREDICTION. The lane that builds the winner
owes the same table with a second pair of columns, because the reference card
and the application are two implementations and the whole value of the method
is that their disagreement is a finding.

    subject   R3 sim  R2d8 sim  sim gain | R3 real  8-ring real  real gain
    blast_n4  0.4978   0.5646    +0.0668 | 0.5038     0.5736       +0.0698
    blast_n8  0.6639   0.7200    +0.0561 | 0.6754     0.7483       +0.0729
    maple_n4  0.3609   0.3685    +0.0076 | 0.3545     0.3674       +0.0129
    dead_n4   0.5826   0.6153    +0.0327 | 0.5721     0.6073       +0.0352
    rock_n4   0.7777   0.8331    +0.0554 | 0.7724     0.8305       +0.0581

Absolute IoUs within 0.006..0.011 and every gain the same sign and order: that
is the instrument passing its own audit. A simulated gain that does not appear
is not a disappointment, it is the most interesting result of the lane.

**The metric will not be the same on both sides, so say which.** The offline
fill used `scipy.ndimage.distance_transform_edt`, a EUCLIDEAN disc of radius 8.
The C++ grows by 8-connected ring passes, a CHEBYSHEV square of radius 8. The
two sets differ by the corners. Write the divergence into the code comment, and
show the answer does not turn on it -- here both 8 and 16 texels beat the
shipped fill on all five subjects, so the radius and therefore the metric is
not what is being decided.

**Two sheet sets need two fixture ROOTS.** `registerLooseSheets` walks UP from
a `.lodm` to the nearest ancestor holding a `textures/` tree, so `cards/` and
`cards_old/` side by side in one fixture resolve to the SAME DDS and the A/B
measures nothing -- while still printing two slightly different numbers,
because the harness's IoU has jitter. Build a separate root, check the two
`_oct_n.DDS` differ, and check the old root reproduces the previous lane's
published number to four digits before believing the new one.

**Count the views the repair LOSES and name them.** A mean over 24 views hides
its own minority. blast_n4 improved on 21 of 24 and lost three, worst
-0.0675 at azimuth 300 elevation 15; the rock improved on 21 of 24 and lost
three at elevation 45, all under 0.009. Put the worst LOSING view in the
picture strip, not the best winning one -- and if the close-up you chose at
random turns out to be one of the losers, publish that one.

**The band that contains the plane by construction.** A clause of the form
"the texels outside must lie in the band the covered texels occupy" cannot
convict a sheet whose outside is the CARD PLANE, because the band is the
object's full depth range and an object centred on its own card straddles the
plane. Measured: 0 frames of 16 failed on four of five subjects. Continuity
across the silhouette -- an uncovered texel touching an inked one carries a
height within SLACK of the mean of its inked neighbours -- fails on all five.
Probe a proposed clause against the OLD state before writing it into the gate;
a throwaway script that scores two candidate clauses side by side costs ten
minutes and is the only thing that tells them apart.
"""

a = open(A, 'rb').read()
cr0 = a.count(b'\r')
if b'## 11.' in a:
    print('section 11 already present -- refusing to append twice'); sys.exit(2)
if not a.endswith(b'\n'):
    a += b'\n'
out = a + TAIL.encode('utf-8')
if out.count(b'\r') != cr0:
    print('CR COUNT MOVED %d -> %d' % (cr0, out.count(b'\r'))); sys.exit(2)
open(A, 'wb').write(out)
print('repo tree   %s  %d -> %d bytes, CR %d' % (A, len(a), len(out), out.count(b'\r')))
b = open(B, 'rb').read()
print('second tree was %d bytes (stale: %s)'
      % (len(b), 'missing IMPOSTORFIX2 sections' if b'## 7.' not in b else 'current'))
shutil.copyfile(A, B)
print('second tree %s  now %d bytes' % (B, len(open(B, 'rb').read())))
