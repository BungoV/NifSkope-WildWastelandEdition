"""Give ww-simulate-before-build the audit ROADS3 owed it, and mark PENDING.md
superseded. Written as a file: prose apostrophes, and heredocs arrive CRLF.
Both trees get the skill paragraph, additively, because lane UINOTES1 is live in
the copy and nothing there may be deleted or overwritten.
"""
import io
import os
import sys

MAIN = r'E:\Projects\NifskopeWildWastelandEdition'
UI = r'E:\Projects\NifskopeWWE_ui'
REL = r'.claude\skills\ww-simulate-before-build\SKILL.md'

PARA = """
## How accurate the simulation actually is (measured, ROADS3, 2026-09-12)

Not a guess. ROADS3 priced `--road-opacity` offline, then baked the same
setting on the built exe and compared the two texel by texel on two chunks:

* on the **aggregates the gate table is read on** -- road mean luminance, rise
  over the surround -- the simulation agreed to **0.286** and **0.221** of a
  level;
* **per texel it did not agree at all**: mean absolute difference **1.583**
  levels, 99th percentile 7.341, worst **15.279** (1.527 / 5.745 / 13.802 on
  the second chunk).

The gap is 8-bit quantisation plus BC1 block compression, which the bake goes
through and the simulation does not. Read that as the licence this technique
carries: it licenses **which settings are worth baking**, and it does not
license a picture, a gate row, or a per-texel claim of any kind. An earlier
draft of ROADS3's own contract amendment said the simulation was "right to
about half a level" before anyone had measured it -- 3x optimistic on the
average and 30x on the tail. A tolerance is a measurement or it is not written
down.
"""

n = 0
for root in (MAIN, UI):
    p = os.path.join(root, REL)
    if not os.path.exists(p):
        print('skipped (absent): %s' % p)
        continue
    s = io.open(p, encoding='utf-8', newline='').read()
    if 'How accurate the simulation actually is' in s:
        print('already carries the audit: %s' % p)
        continue
    s = s.rstrip('\n') + '\n' + PARA
    io.open(p, 'w', encoding='utf-8', newline='').write(s)
    d = io.open(p, 'rb').read()
    print('appended to %s  (%d bytes, CRLF %d)' % (p, len(d), d.count(b'\r\n')))
    n += 1

# ------------------------------------------------------------- PENDING.md
P = os.path.join(MAIN, r'scratchpad\roads3_20260911\PENDING.md')
s = io.open(P, encoding='utf-8', newline='').read()
if s.startswith('# ROADS3 -- SUPERSEDED'):
    sys.exit('PENDING.md already superseded; skill work above is done')
head = """# ROADS3 -- SUPERSEDED: the build WAS spent, this file is history

**Read this first.** `Fallout4.exe` had exited by **2026-09-12 04:08:58**, the
build was spent after a fresh `tasklist` check, and `tools/ww_build.sh` returned
BUILD-RC=0 on the first try -- one build, zero extra relinks. New
`release/NifSkope.exe` **2026-09-12 04:10:38, 21,489,152 B**, md5
`fe65cc978f3896881140c2eea57c69c6`. Gates F2, F3a-F3h and F4 are all measured
on it and are in `scratchpad/lane_roads3_report.md` sections 3 and 4, and both
pictures are re-taken from real bakes. **Nothing below is owed.** It is kept
only as the record of what the lane handed over while it was pending, and as the
worked example the `nifskope-ww-resume-pending` skill asks for.

---

"""
io.open(P, 'w', encoding='utf-8', newline='').write(head + s)
d = io.open(P, 'rb').read()
print('PENDING.md marked superseded (%d bytes, CRLF %d)' % (len(d), d.count(b'\r\n')))
