#!/usr/bin/env python3
"""CARDPAD -- two entries in MISTAKES.md, newest at the top, above the entry the
director wrote when bungo corrected the reading of his number."""

P = 'MISTAKES.md'
b = open(P, 'rb').read()
cr0 = b.count(b'\r')
s = b.decode('utf-8')

ANCHOR = "## 2026-09-09 -- padding briefed as a per-side number; bungo meant the GAP"
assert s.count(ANCHOR) == 1

NEW = """## 2026-09-09 -- lane CARDPAD changed a metric and kept its old control, and the control stopped being able to fail

- **What was done:** the cross-frame bleed gate in `tests/spells/lodgen_octahedral.sh`
  moved from "the neighbour's alpha contribution at a border must be 0" to "a whole
  texel of gap must still separate the two silhouettes". Its CONTROL -- the same
  sheet with the margins stripped and the inner rects re-tiled edge to edge -- was
  carried across unchanged.
- **What was true instead:** under the ALPHA metric that control worked, because any
  neighbour alpha at all was a failure. Under the GAP metric it does not: a
  silhouette that does not reach its own inner rect still leaves clear texels at the
  border, and the stripped sheet measured **exactly 1.000 texels of gap** -- passing
  the check it exists to fail.
- **How it was found:** the harness said `FAIL the gap check FAILS on a sheet with
  no margins`, which is the control doing its job one step too late: it cost a
  three-minute two-bake run. Fixed by cropping each frame to its OWN silhouette box
  and resizing it to fill its cell, so neighbouring silhouettes are zero texels
  apart by construction (0.431 measured), with a floor printed beside it -- the
  number of covered texels actually sitting on a cell border, which must be positive
  or the control is void.
- **The rule:** a control belongs to a METRIC, not to a gate. When the quantity a
  check measures changes, the control is re-derived and re-run against the new
  quantity BEFORE the gate is trusted -- and a control gets a floor of its own, so
  "the control failed" cannot mean "the control was empty".

## 2026-09-09 -- a mip-count check in the card harness had been agreeing with the shipped count by coincidence

- **What was done (found, not committed, by lane CARDPAD):**
  `tests/spells/lodgen_octahedral.sh` asserted the shipped mip count against
  `expect = 1; side = min(tw, th); while side >= 16: expect += 1; side //= 2` -- the
  mip law from BEFORE 2026-09-09, which counted levels until a frame spanned eight
  texels. Lane CARDFIT3 replaced the law in the code with `1 + log2(min(pad))` and
  left this check alone; it stayed green.
- **What was true instead:** the two rules agree only on the frame shape this bake
  happens to produce (48x64: both give 3). On a 32-texel short side they differ by a
  level, and the check would have passed a sheet built to no law at all.
- **How it was found:** re-deriving the expectation from the gap while updating the
  gate, and noticing the old expression could not depend on the padding at all.
- **The rule:** when a LAW moves, every check that restates it moves with it in the
  same edit -- and a check that restates a law is written as the law, not as an
  expression that happens to produce the same number on the fixture.

"""

s = s.replace(ANCHOR, NEW + ANCHOR)
nb = s.encode('utf-8')
assert nb.count(b'\r') == cr0
open(P, 'wb').write(nb)
print('MISTAKES.md: %d -> %d bytes, CR %d' % (len(b), len(nb), cr0))
