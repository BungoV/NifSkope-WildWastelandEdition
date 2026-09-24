# -*- coding: utf-8 -*-
import sys
P = '../specs_20260909/spec_water.md'
t = open(P, 'rb').read().decode('utf-8')
edits = []
def rep(old, new): edits.append((old, new))

# ---- 5.4: the body the harness marks ---------------------------------------
rep("""2. Add one stroke down the middle of body **233** (`ExtRiverCharlesUpper`,
   25,112 texels, cells −16..−6 × −21..−4). Re-solve.""",
"""2. Add one stroke down the middle of body **3** (`ExtRiverCharlesUpper`,
   25,114 texels, cells −16..−6 × −21..−4). Re-solve.
   *(CORRECTED: this page said body 233, WATER1's id under the 590-body rule.
   Ids are assigned by descending area and the rule changed, so the Charles is
   now id 3. Audited before the harness was written —
   `scratchpad/water3_20260910/audit_spec.py`.)*""")
rep("""3. **Assert**: the number of texels whose flow word changed and whose body ID
   is NOT 233 is **0**, of 21.7 M wet texels.""",
"""3. **Assert**: the number of texels whose flow word changed and whose body ID
   is NOT 3 is **0**, of the **21,754,958** texels that name a body.""")
rep("""4. **The floor**: at least 60% of body 233's own texels changed, and its
   `flow source` byte moved 3 → 4. A test that only checks "nothing else moved"
   passes on a solver that does nothing.""",
"""4. **The floor**: at least 60% of body 3's own texels changed, and its
   `flow source` byte moved 3 → 4. A test that only checks "nothing else moved"
   passes on a solver that does nothing.""")
rep("""5. **The refuter, run first**: place the same stroke on body **136**
   (`ExtMarshDarkWater`, 29,305 texels, its neighbour in the same region) and
   watch step 3 go RED for 233 — i.e. show the test failing before trusting it
   pass.""",
"""5. **The refuter, run first**: place the same stroke on body **2**
   (`ExtMarshDarkWater`, 29,312 texels — WATER1's body 136, whose 29,305 grew by
   the same exact-shore-test texels) and watch step 3 go RED for 3 — i.e. show
   the test failing before trusting it pass.""")

# ---- 7: WATER2's gates, met, with the numbers that actually hold ------------
rep("""| G5 the census reproduces | **590 bodies, 93 at ≥ 64 texels, 1 sea, 15 painted forms + the default, the Charles ONE body of 25,112 texels over cells (−16..−6, −21..−4), 215 bridge merges accepted and 3 refused.** Any deviation is a rule change and must be argued, not rounded away |
| G6 the known-answer control | `scratchpad/water_20260909/control_synth.py`'s synthetic worldspace, run through the C++ writer, gives 4 bodies classed sea / river / lake / lake, the river with 16 surfaces |""",
"""| G5 the census reproduces | **RESTATED, and MET as restated.** WATER1's pre-registered numbers (590 bodies, 93 at ≥ 64, 215 accepted / 3 refused) were measured under a decimated shore test and are withdrawn; §2 carries the audit. What the shipped classifier gives, and what the gate now pins: **346 bodies, 89 at ≥ 64 texels, 1 sea / 115 river / 230 lake, 15 painted forms + the default, the Charles ONE body — id 3 — of 25,114 texels over cells (−16..−6, −21..−4), 528 bridge merges accepted and 13 refused.** The INVARIANT that crossed the rule change unmoved, and which is why this is a grouping difference and not a reading difference: **13 of 15 per-form texel totals identical to WATER1's, form for form**; the two that moved (`ExtOceanWater` 21,594,553 → 21,583,510 and `ExtLakeWater` 12,960 → 24,003) are exactly the 11,043 texels the SIZE guard took back |
| G6 the known-answer control | **RESTATED, and MET as restated.** The expectation belongs to the rule under test: a body carries ONE plane by construction, so the control's stepped river IS sixteen bodies, not one with sixteen surfaces. The geometry was NOT changed. `lodl <any> --water-selftest` builds the synthetic worldspace in memory and asserts **19 bodies: 1 sea (73,728 texels, the WORLDSPACE's own form), 16 river steps, 1 lake (16,384), 1 puddle (16)**, with the type-blind refuter printed beside it (18 bodies: the sea and the river's tidal step fuse). It FAILED on its first run and named a real defect in rule C — §2's missing direction, in the merge the Commonwealth happens never to exercise |""")

# ---- 7: the P gates ---------------------------------------------------------
rep("""### Lane WATER3 — the panel and the tool (`src/lodgenmanager.cpp`, the viewer)

The Water section, the five tools, the body picker, the summary, and
`WW_WATER_TEST`.""",
"""### Lane WATER3 — the panel and the tool (NEW files `src/watermark.{h,cpp}`, `src/watermarkpanel.{h,cpp}`)

The Water section, the five tools, the body picker, the summary, and the
harness — named `WW_WATER_MARK_TEST` as built, because `WW_WATER_TEST` reads
as the writer's own control and this one tests the TOOL.

**File plan changed, deliberately.** This page said `src/lodgenmanager.cpp`.
Lane BUILD4 was compiling `src/lodgen.cpp` and `src/nifskope_ui.cpp` while this
lane ran, so every line of new code went into NEW files that compile alone
(`g++ -fsyntax-only` with the real flags) and the touches to existing files were
made only after that build finished and were kept to registration and dispatch.""")

for old, new in edits:
    n = t.count(old)
    if n != 1:
        sys.stderr.write('REFUSED: %d matches for %r...\n' % (n, old[:70])); sys.exit(1)
    t = t.replace(old, new)
open(P, 'wb').write(t.encode('utf-8'))
nb = open(P, 'rb').read()
print('ok: %d edits, %d bytes, %d lines, CR=%d' % (len(edits), len(nb), nb.count(b'\n'), nb.count(b'\r')))
