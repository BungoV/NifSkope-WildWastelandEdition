# -*- coding: utf-8 -*-
import sys
P = '../specs_20260909/spec_water.md'
t = open(P, 'rb').read().decode('utf-8')
edits = []
def rep(old, new): edits.append((old, new))

# ---- 2: the merge/bridge clauses, with the DIRECTION lane WATER2 measured ---
rep("""merge       a component whose type is WATER_TYPE_DEFAULT (0xFFFF) is merged into
            the same-height PAINTED component it touches; with more than one
            candidate, into the largest, and the body is flagged AMBIGUOUS.
bridge      two components at the same height whose shores are within 2 texels
            (256 world units) are merged when their types are equal or one
            inherits.  REFUSED when both are painted with different types.""",
"""merge       a component whose type is WATER_TYPE_DEFAULT (0xFFFF) is merged into
            the same-height PAINTED component it touches; with more than one
            candidate, into the largest, and the body is flagged AMBIGUOUS.
            THE INHERITING SIDE IS ONLY ABSORBED WHEN IT IS THE SMALLER OF THE
            TWO (the SIZE guard, §2 note below).
bridge      two components at the same height whose shores are within 2 texels
            (256 world units) are merged when their types are equal or one
            inherits, under the SAME size guard.  REFUSED when both are painted
            with different types.  The shore test is EXACT -- a disc scan around
            every texel, never a thinned point cloud.""")

rep("""Measured on the Commonwealth: 804 (type-keyed) → 781 (height-keyed) → 792 (rule
C) → **590 (rule D)**, 215 bridge merges accepted, **3 refused**. Rule D is the
first rule under which the Charles is ONE body (25,112 texels, cells −16..−6 ×
−21..−4, assembled from 9 components); under every other rule it is two or more.""",
"""**CORRECTED, and this is the page's biggest change.** WATER1 measured
804 → 781 → 792 → **590**, 215 bridge merges accepted and 3 refused. Two defects
were found by lane WATER2 and both are measured, not argued
(`scratchpad/water2_20260909/bridge_exact.py`, `bridge_effect.py`,
`bridge_variants.py`, `merge_guard_variants.py`):

* **the shore test was DECIMATED.** WATER1's script compared point clouds
  thinned to at most 4,000 points a body — 4,000 of 21,585,117 for the sea — and
  a thinned point set can only make a minimum distance LARGER, so the error is
  ONE-SIDED and every disagreement is a merge the stated rule requires. Exact:
  **545** pairs within two texels against the decimated test's 218.
* **the merge had no DIRECTION.** With the exact test the biggest body came out
  as 21,587,443 texels of `ExtMarshScumWater` — the Commonwealth's ocean
  carrying a marsh's name — because "equal or one inherits" was read in both
  directions. The SIZE guard above is the fix, and it was chosen on a number:
  it is the only single clause under which "the painted type with the most area"
  and "the majority type counting inherited area" name the SAME form for every
  one of the 346 bodies.

Measured on the Commonwealth, as SHIPPED: 805 components → 793 (rule C) →
**346 (rule D)**, **528** bridge merges accepted, **13 refused**; class sea 1,
river 115, lake 230. Rule D is still the first rule under which the Charles is
ONE body — **body id 3, 25,114 texels, cells −16..−6 × −21..−4** — and under
every other rule it is two or more. Two texels more than WATER1's 25,112, which
is the exact shore test picking up what the thinned one missed.""")

rep("""**Bodies below 4 texels are noise** — 288 of the 590 are single-sample dips
below the sea plane at the 128-unit sample rate.""",
"""**Bodies below 4 texels are noise** — **115 of the 346** are single-sample dips
below the sea plane at the 128-unit sample rate (WATER1 said 288 of 590).""")

# ---- 3.3 -------------------------------------------------------------------
rep("48 bytes; 590 bodies = 28,320 bytes for the Commonwealth.",
    "48 bytes; **346 bodies = 16,608 bytes** for the Commonwealth, at file offset\n`0x2249AE6`.")

# ---- 3.8: the stride refusal, reversed --------------------------------------
rep("""* `bodyRecordBytes` larger than this reader's record → *"the body table's
  records are N bytes; this reader knows M"*;""",
"""* `bodyRecordBytes` **SHORTER** than this reader's record → *"the body table's
  records are N bytes; this reader knows M"*. **CORRECTED: this page had the
  refusal the wrong way round**, which would have made the stride field useless.
  §3.3 states the rule correctly and the reader implements §3.3: a LONGER record
  is the forward-compatible case (stride past the fields you do not know), a
  SHORTER one is missing fields the reader needs and is refused by name;""")

# ---- 4.1 step 6 ------------------------------------------------------------
rep("""6. Class per §2; assign IDs by descending area, so ID 1 is the sea and the
   list a user sees is sorted where it matters.""",
"""6. Class per §2; assign IDs by descending area, so ID 1 is the sea and the
   list a user sees is sorted where it matters. **BUILT as stated**: id 1 is the
   21,575,619-texel sea, id 2 the largest marsh, id 3 the Charles.""")

for old, new in edits:
    n = t.count(old)
    if n != 1:
        sys.stderr.write('REFUSED: %d matches for %r...\n' % (n, old[:70])); sys.exit(1)
    t = t.replace(old, new)
open(P, 'wb').write(t.encode('utf-8'))
nb = open(P, 'rb').read()
print('ok: %d edits, %d bytes, %d lines, CR=%d' % (len(edits), len(nb), nb.count(b'\n'), nb.count(b'\r')))
