# -*- coding: utf-8 -*-
"""Bring spec_water.md current with lane WATER2's measured truth.
Every replacement is exact-and-unique or the script refuses."""
import io, sys
P = '../specs_20260909/spec_water.md'
b = open(P, 'rb').read()
assert b.count(b'\r') == 0, 'spec_water.md is LF-only'
t = b.decode('utf-8')
edits = []

def rep(old, new):
    edits.append((old, new))

# ---- the banner ------------------------------------------------------------
rep("""> **SPEC / NOT YET WRITTEN.** No code in this tree implements any section, rule
> or panel below. Lane WATER1 was read-only: it measured the Commonwealth and
> wrote this. Every number in §1 is measured and its script is named; everything
> from §2 on is DESIGN and carries no provenance because it has no source yet.
> The provenance footer covers only the claims this page makes about the
> EXISTING v1/v2 format and its writer/reader.""",
"""> **STATUS, 2026-09-10. Half of this page is BUILT and half is still design,
> and every paragraph now says which.** Lane WATER1 (read-only) wrote the page;
> lane WATER2 built the format, the writer, the reader, the three planes and
> the CLI, and MEASURED several of WATER1's numbers to be wrong; lane WATER3 is
> building the marking tool of §5 and edited this page.
>
> * **BUILT** (WATER2, `src/lodtfile.{h,cpp}`, `src/nifcli.cpp`,
>   `src/btdterrain.{h,cpp}`, gate `tests/spells/lodl_water.sh` 56/0):
>   §2's body rule, §3's whole file layout, §3.7's stroke store (written
>   present and EMPTY), §4.1 and §4.2, and the viewer's `bodyid` / `flow` /
>   `shore` planes.
> * **BUILT** (WATER3, `src/watermark.{h,cpp}`, `src/watermarkpanel.{h,cpp}`):
>   §4.3's propagation, §5's tool and panel, §5.4's harness.
> * **DESIGN, with no code**: §6, the FO4CS consumer's checklist — nothing has
>   read a version-3 file outside this tree.
>
> **CORRECTED BY MEASUREMENT.** WATER1's census is superseded where the two
> disagree: **346 bodies, not 590** (§2), the record-stride refusal is the
> other way round (§3.8), and §7's G5/G6 numbers are restated. Each correction
> names the script that measured it. Numbers in §1 that WATER1's own read-only
> pass produced are kept where nothing later re-measured them and are marked
> where something did.""")

# ---- 1.2 -------------------------------------------------------------------
rep("""2. **16 forms serve 590 bodies.** `ExtLakeWater` alone paints 16 separate lakes
   with one colour and one velocity, `ExtOceanWater` 405. There is no per-body
   anything in vanilla — that is the whole gap.""",
"""2. **16 forms serve the whole worldspace's water.** `ExtLakeWater` alone paints
   15 separate lakes with one colour and one velocity, `ExtOceanWater` 176.
   There is no per-body anything in vanilla — that is the whole gap.
   *(CORRECTED: WATER1 said 590 bodies, 16 `ExtLakeWater` lakes and 405
   `ExtOceanWater` bodies. The shipped classifier gives **346 bodies**, 15 and
   176; see §2. The per-form TEXEL totals did not move — that is the invariant
   no grouping rule can touch.)*""")

# ---- 1.3 -------------------------------------------------------------------
rep("""3. **FO4's water is FLAT.** 589 of 590 bodies carry exactly ONE water height.
   *There is no height gradient inside a river to read a direction from.*""",
"""3. **FO4's water is FLAT.** WATER1 measured 589 of its 590 candidate bodies
   carrying exactly ONE water height; under the shipped rule D a body carries
   one height BY CONSTRUCTION (the component key is (height, type)), so the
   fact this states is now a property of the classifier and the measurement
   that matters is the one behind it: a river that steps N times is N bodies.
   *There is no height gradient inside a river to read a direction from.*""")

# ---- 1.6 -------------------------------------------------------------------
rep("""6. **So flow cannot be computed for most water.** Of the 93 bodies at or above
   64 texels: 26 `drain` (a lower body within 64 texels, 22 of them with the
   outlet on the body's own axis), 1 `bed`, 12 `zero`, and **54 need a human
   stroke**.""",
"""6. **So flow cannot be computed for most water.** MEASURED ON THE SHIPPED FILE
   (`scratchpad/water3_20260910/audit_spec.py`, through the independent decoder,
   never the writer): of the **89** bodies at or above 64 texels — 1 sea, 42
   rivers, 46 lakes — **29** are served by `drain`, **1** by `bed`, **12** by
   `none` (the sea and round lakes with no outlet, which is bungo's own rule and
   not a gap), and **47 are served only by the form's `NAM0`** — the vanilla
   fallback floor, one velocity shared by every body of that form. Those 47 are
   what a human stroke is for. *(WATER1 said 93 bodies, 26 / 1 / 12 / 54.)*""")

for old, new in edits:
    n = t.count(old)
    if n != 1:
        sys.stderr.write('REFUSED: %d matches for %r...\n' % (n, old[:70]))
        sys.exit(1)
    t = t.replace(old, new)
open(P, 'wb').write(t.encode('utf-8'))
nb = open(P, 'rb').read()
print('ok: %d edits, %d bytes, %d lines, CR=%d' % (len(edits), len(nb), nb.count(b'\n'), nb.count(b'\r')))
