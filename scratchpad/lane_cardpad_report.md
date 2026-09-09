# Lane CARDPAD — the card spacing is the GAP, not the margin

bungo's correction, verbatim (2026-09-09, after the relay of lane CARDFIT3):

> *"When I say padding 8 for 1k, it's 8 pixels of distance between two rendered
> objects."*

**State.** Built once, `release/NifSkope.exe` **2026-09-09 22:04:35**, newer than
both sources this lane changed, stylesheet in step (`cmp` clean).
`Fallout4.exe` and any `NifSkope.exe` were checked absent before the build, before
the 19-tree bake and before every harness run (`rc=1` each time). Three gates
green. **Nothing committed** (his "Not yet"): 84 uncommitted paths in the tree.
**bungo's open NifSkope window, if any, predates this and needs a restart.**

**One step of the brief is PENDING**: the FO4CS sample set. `Fallout4.exe` came up
at ~22:22 and `make_samples.sh` refused on its own game-up guard, before its first
`rm -rf`, so the shipped sample set is intact and unchanged and still stands on
CARDFIT3's library. The resume is `scratchpad/cardpad_20260909/PENDING.md`.

Pictures: `scratchpad/cardpad_20260909/pics/cardpad_0003a28b.png` (TreeHero01),
`cardpad_000393cd.png` (TreeBlasted02). Scripts, logs and JSON are in
`scratchpad/cardpad_20260909/`.

---

## 1. The rule as now implemented

The number names the **distance between two neighbouring silhouettes** across the
frame border they share. Each of the two frames therefore keeps HALF of it:

```
gap(side) = max( 2, side / 16 ), rounded UP to even
pad(side) = gap(side) / 2                          // on EACH side of a frame
gapX = gap(frameW)   gapY = gap(frameH)
padX = gapX / 2      padY = gapY / 2
inner rect = (frameW - gapX) x (frameH - gapY)
mips = 1 + log2( min( gapX, gapY ) )
```

The inner rect is **15/16 of the frame** on every side that is a multiple of 32 —
120 of 128, 240 of 256 — which is his 8-on-1024 and 16-on-2k exactly. A side of
48, 80 or 112 rounds the gap up to even and gives back at most one texel.

| frame | gap x, y | margin per side | inner rect | inner / frame | mips | gap at the last mip |
|---|---|---|---|---|---|---|
| 16x64 | 2, 4 | 1, 2 | 14x60 | 0.875 x 0.938 | 2 | 1 texel |
| 32x32 | 2, 2 | 1, 1 | 30x30 | 0.938 x 0.938 | 2 | 1 texel |
| 32x64 | 2, 4 | 1, 2 | 30x60 | 0.938 x 0.938 | 2 | 1 texel |
| 48x64 | 4, 4 | 2, 2 | 44x60 | 0.917 x 0.938 | 3 | 1 texel |
| 96x128 | 6, 8 | 3, 4 | 90x120 | 0.938 x 0.938 | 3 | 1.5 texels |
| **128x128** | **8, 8** | **4, 4** | **120x120** | **0.938 x 0.938** | **4** | **1 texel** |
| 256x256 | 16, 16 | 8, 8 | 240x240 | 0.938 x 0.938 | 5 | 1 texel |

**Why the mip count is unchanged from CARDFIT3's table.** Mip *construction* never
mixes frames — the box filter halves an even frame into an even frame — so the
bleed is at *sample* time, where a tap on a frame's own UV border reads half of
that frame's last texel and half of the neighbour's first. What has to survive at
level `k` is the SEPARATION between the two silhouettes, `gap / 2^k`, and the chain
stops at the last level where that is a whole texel. The count was always the
gap's; the gap has not changed; only the padding halved.

**The sheet's outer border: half a gap, and it already has it.** There is no
neighbouring frame beyond it, so padding every frame by `gap/2` is correct at the
edge with no special case — an interior border carries `gap/2` from each of the two
frames meeting on it, an outer border carries `gap/2` and faces the sheet edge.
That holds because a card sheet is sampled **CLAMPED**: the card quad's UV rect is
a sub-rect of the sheet, the `.lodm` carries no wrap mode and the DDS header sets
no wrap flag (`dwCaps 0x401008`, complex/texture/mipmap). Under WRAP the outer
border would face the opposite edge's frames and would need a whole gap. The gap
gate measures INTERIOR borders only, for exactly this reason, and says so.

**What the `.lodm` and the sidecar carry.** The sidecar meta line is now
`gap <x> <y>` — the distance between two silhouettes. The `.lodm` keeps
`card.pad` / `array.pad` as **the per-side number** (as bungo's brief asked) and
gains `card.gap` / `array.gap`, exactly twice it, so a reader never has to guess
which quantity a lone number meant, and so the three vintages of card set can each
be read under their own law:

| the set carries | the margin per side | the mip cap |
|---|---|---|
| `gap` (this law) | `gap/2` | `1 + log2(min(gap))` |
| `pad` alone (lane CARDFIT3) | `pad` | `1 + log2(min(pad))` |
| neither (older) | `max(4, longSide/16)` | `1 + log2` of that |

`--card-half-aux` is unchanged in principle: the gap is rounded up to even so a
halving lands the margins on whole texels, and `auxMips = 1 + log2(min(gap)/auxDiv)`
comes down with it. On the smallest frames (16 and 32 texels, gap already at its
floor of 2) that division reaches 1 and the aux sheets ship a **single** level — a
fallback that names itself instead of bleeding silently. Half-aux is off by default
and was off for every measurement here.

**Untouched, as the brief required:** the aspect fit (still the smallest multiple
of 16 whose inner rect does not crop the silhouette), the edge dilation
(`max(8, max(fw,fh)/8)` passes, coverage alpha never moved), the alpha padding, the
one-scale rule, `card.center`, the 1% measurement margin, and `_fs.DDS` as DXT5.
The aspect fit's *inputs* move, because the inner rect it fits into is now bigger —
that is the law working, and §2 shows the two trees it moved.

---

## 2. Fill and spacing, CARDFIT3 vs now, over the 19 trees

Same 19 Sanctuary trees (region −20 24 −17 27, `CANDIDATES=trees`), `OCT=8
TILE=128`, half-aux off, both libraries baked by this tree.
BEFORE = `scratchpad/cardfit_20260909/cards_after` (exe 20:49:01).
NOW = `scratchpad/cardpad_20260909/cards_gap` (exe 22:04:35, 19 baked / 0 failed).
Coverage floor 16/255 — the same threshold lane CARDFIT3 measured on, so the two
reports compare; the BEFORE column reproduces their §4.1 numbers exactly.
Script: `scratchpad/cardpad_20260909/measure_gap.py`; JSON beside it.

### 2.1 Fill — the union of the 64 views' silhouette boxes

| | before (min / median / max) | now (min / median / max) |
|---|---|---|
| silhouette / frame, x | 0.250 / **0.729** / 0.875 | 0.250 / **0.854** / **0.938** |
| silhouette / frame, y | 0.797 / **0.859** / 0.875 | 0.844 / **0.922** / **0.938** |
| silhouette / inner rect, x | 0.333 / 0.875 / 1.000 | 0.286 / 0.932 / 1.000 |
| silhouette / inner rect, y | 0.911 / 0.982 / 1.000 | 0.900 / 0.983 / 1.000 |
| **inner rect / frame, y** | 0.875 on all 19 | **0.9375 on all 19** |
| total sheet area | 4,915,200 texels | **4,816,896** (−2.0%) |
| frame shapes | 7 | **6** |

Two trees changed frame shape, because the wider inner rect let a narrower frame
hold the silhouette:

| base | before | now |
|---|---|---|
| `000531ae` TreeMapleblasted04 | 32x64, fill x **0.438** | 16x64, fill x **0.875** |
| `000531b3` TreeMapleblasted05 | 32x32, fill x **0.500** | 16x32, fill x **0.875** |

`0003e08d` TreeBlasted05 stays at 0.250: it is already on the 16-texel floor in a
16x32 frame, and there is no narrower rung. That one is the aspect ladder's floor,
not the spacing's.

### 2.2 Spacing and bleed — measured on the sheets, at every shipped mip

Three numbers, all read off the baked sheets with the mip chain rebuilt by lodgen's
own filter (2×2 box, rounded half-up):

* **clear texels between two silhouettes** — the number of wholly transparent texel
  columns actually left at the row where two neighbours come closest. This is
  bungo's own quantity;
* **narrowest gap** — the same separation measured as transparent COVERAGE over
  exactly the two texels a border tap reads, `(255−αA)/255 + (255−αB)/255`. This is
  what the shipped law promises to keep at ≥ 1 texel;
* **worst neighbour alpha at a border** — lane CARDFIT3's stricter metric: half the
  neighbour's edge alpha, i.e. what a bilinear tap on the border actually picks up.

| | before | now |
|---|---|---|
| clear texels, mip 0 (min / median / max) | 4 / 10 / 24 | 2 / 7 / 16 |
| clear texels, last shipped mip | 2 / 4 / 6 | 0 / 2 / 4 |
| narrowest gap at any shipped mip | 2.000 / 2.000 / 2.000 | **1.408** / 1.957 / 2.000 |
| sheets under one texel of gap | 0 of 19 | **0 of 19** |
| worst neighbour alpha at a border | 0 of 19 sheets, max 0/255 | **13 of 19 sheets, max 64/255** |

**The promise holds: no sheet drops below one texel of gap at any shipped mip**,
with 0.408 texels of margin on the worst of the 19.

**The cost, stated rather than buried.** One texel of gap at the deepest level is
half a texel of margin on each side, and a bilinear tap taken exactly on a frame
border reaches half a texel. So at that last level a border tap now picks up some
of the neighbour's edge — 13 of 19 sheets, worst 64/255 (`000531b3`), then 56/255
(`0003a28b` TreeHero01) — where the per-side reading gave 0 on all 19. This is not
a defect in the implementation; it is what the criterion "gap ≥ 1 texel" costs,
and the arithmetic is exact:

* bleed-free at level `k` needs `pad / 2^k ≥ 1` — the per-side reading;
* his rule ships while `gap / 2^k ≥ 1`, and `gap = 2·pad`, so it ships **one level
  further** than bleed-free for the same spacing.

Zero is bought by shipping one level fewer at the same spacing —
`mips = 1 + log2(min(pad))`, so 3 levels on a 128 frame instead of 4 — not by more
padding. **Shipped as he stated it; the alternative is a one-line change and his
call.** For scale: the law before CARDFIT3 had the same half-texel geometry at its
deepest level and measured 26/255, which is the defect CARDFIT3 was chartered to
remove.

### 2.3 Per base

`run0 (runLast)` = clear texels at mip 0 and at the last shipped mip.

| base | frame B→A | fill x B→A | run0 B | run0 A | narrowest gap A | alpha bleed A | mips |
|---|---|---|---|---|---|---|---|
| `00038599` TreeBlasted01 | 16x64 | 0.750 → 0.875 | 6 (2) | 4 (2) | 1.953 | 6 | 2 |
| `000393cd` TreeBlasted02 | 16x64 | 0.750 → 0.812 | 8 (4) | 6 (3) | 1.976 | 3 | 2 |
| `0003a28b` TreeHero01 | 128x128 | 0.875 → **0.938** | 24 (3) | 16 (2) | 1.561 | **56** | 4 |
| `0003e08d` TreeBlasted05 | 16x32 | 0.250 → 0.250 | 6 (3) | 4 (1) | 1.639 | 46 | 2 |
| `0003e08f` TreeMapleblasted07 | 32x64 | 0.562 → 0.562 | 13 (6) | 9 (4) | 2.000 | 0 | 2 |
| `0003e0d1` TreeMapleForest7 | 16x64 | 0.625 → 0.750 | 9 (4) | 9 (4) | 2.000 | 0 | 2 |
| `00049532` TreeBlasted04 | 16x64 | 0.500 → 0.625 | 8 (4) | 4 (2) | 2.000 | 0 | 2 |
| `0004a073` TreeMapleForest1 | 48x64 | 0.729 → 0.771 | 13 (3) | 9 (2) | 1.655 | 44 | 3 |
| `0004a074` TreeMapleForest2 | 96x128 | 0.812 → 0.875 | 23 (6) | 16 (4) | 2.000 | 0 | 3 |
| `0004a075` TreeMapleForest3 | 32x64 | 0.500 → 0.531 | 9 (4) | 5 (2) | 2.000 | 0 | 2 |
| `0004d93b` TreeMapleblasted01 | 32x64 | 0.844 → 0.906 | 10 (5) | 7 (3) | 1.980 | 2 | 2 |
| `000503b6` TreeMapleblasted02 | 32x64 | 0.688 → 0.750 | 10 (5) | 7 (3) | 2.000 | 0 | 2 |
| `000531ae` TreeMapleblasted04 | **32x64 → 16x64** | 0.438 → **0.875** | 12 (6) | 6 (2) | 1.894 | 13 | 2 |
| `000531b3` TreeMapleblasted05 | **32x32 → 16x32** | 0.500 → **0.875** | 4 (2) | 2 (0) | **1.408** | **64** | 2 |
| `000a7206` TreeBlasted01Lichen | 16x64 | 0.750 → 0.875 | 4 (2) | 3 (1) | 1.835 | 21 | 2 |
| `000d9ca8` TreeElmForest01 | 96x128 | 0.875 → 0.938 | 19 (4) | 11 (2) | 1.972 | 3 | 3 |
| `000d9ca9` TreeElmForest02 | 96x128 | 0.875 → 0.938 | 19 (4) | 11 (2) | 1.957 | 5 | 3 |
| `0012154f` BurntTreeUpright03 | 16x32 | 0.500 → 0.500 | 6 (3) | 4 (2) | 1.894 | 13 | 2 |
| `00121550` BurntTreeUpright02 | 48x64 | 0.792 → 0.854 | 14 (3) | 10 (2) | 1.882 | 15 | 3 |

### 2.4 The pictures

`pics/cardpad_0003a28b.png` (TreeHero01, 128x128) and `pics/cardpad_000393cd.png`
(TreeBlasted02, 16x64). Each is four panels — CARDFIT3 above, now below, mip 0 left
and the last shipped mip right — cropped to the two whole frames whose silhouettes
come closest, magnified with the texel grid drawn where a texel is visible.
Checkerboard = transparent, blue = the frame border, green band = the gap the law
promises, red bar = the clear run measured on the sheet at the tightest row.
TreeHero01: 29 → 24 clear texels at mip 0 and 4 → 3 at mip 3, with the tree visibly
larger in the lower row (0.875 → 0.938 of the frame). Both PNGs were opened and
read before being reported.

---

## 3. Gates

`tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` printed **rc=1** before the
build and before each of these runs.

| gate | result | what is new in it |
|---|---|---|
| `tests/spells/lodgen_octahedral.sh` | **73 checks, 0 failures, RESULT PASS** (`scratchpad/cardpad_20260909/oct_harness.log`) | the gap law and the sidecar's `gap` line; a check that no superseded `pad` line survives beside it; the inner rect exactly 15/16 on a side that is a multiple of 32; the fill and air-budget checks against the new inner rect; the mip-bleed check rewritten to the gap rule at every shipped mip on every interior border; the DDS mip expectation re-derived from the gap; the `.lodm`'s `pad` and `gap` and that the gap is twice the padding |
| `tests/spells/lodgen_impostor_cards.sh` | 12 ok, 0 failures, **RESULT PASS** | unchanged |
| `tests/spells/lodgen_identity.sh` | 8 ok, 0 failures, **RESULT PASS** | unchanged — identity is untouched, as required |

Numbers the octahedral gate printed on its own bake (48x64 frame, `TILE=64`):

```
gap: derived 4,4  sidecar ['4','4']; margin per side 2,2; inner rect 44x60 of 48x64
     (0.9167 x 0.9375 of the frame)
covered texels inside the 2,2-texel padding: 0
union of the 16 silhouette boxes: 29 x 53; inner rect 44 x 60 (long axis fill 88.3%)
air on the short axis: inner 44, silhouette 29 -> 15 texels (one rung is 16)
3 shipped mips (gap 4,4): narrowest gap across an interior frame border 1.659 texels,
     over 2352 border samples
CONTROL, every silhouette cropped to its own box and filling its cell: 380 covered
     texels sit on a cell border; narrowest gap 0.431 texels over 2184 samples
_oct_d.DDS: 192x256 DXT5 mips 3 (expected 3)
.lodm spacing: pad [2,2] (derived [2,2]), gap [4,4] (derived [4,4])
```

**The zero-spacing control fails as it must**, and it now carries a floor of its
own — the count of covered texels actually sitting on a cell border (380), so
"the control failed" cannot silently mean "the control was empty". §4 records why
the control had to be rebuilt.

**Not run, and owed:** `tests/spells/lodgen_card_arrays.sh`. This lane changed the
card-array grouping (it now reads `gap` and derives the mip cap from it), so that
gate is reached by the change and should have run; the game came up first. It is in
`PENDING.md` with the sample-set step.

**Contract provenance** (`ww-contract-provenance`): both pages re-stamped and every
line number re-derived from its anchor in one scripted pass
(`scratchpad/cardpad_20260909/anchors.py`) —
`docs/LODGEN_CARD_SHEETS.md` 6 unchanged, 16 moved, **0 anchors not found**;
`docs/LODGEN_LODM_FORMAT.md` 8 unchanged, 13 moved, **0 not found**. Seven anchors
that the pass could not resolve were repaired first (§4). The version constants were
re-read last: `LODM_VERSION = 1` and the payload's `lodm: 1` are unchanged by this
lane, and no consumer version moves.

**Line endings**, measured with Python byte counts, before and after every write:
`src/`, `docs/`, `tools/`, `tests/` and `MISTAKES.md` are LF-only and stayed at
CR = 0; `WW_CHANGES.md` is mixed and its CR count is **19,020 before and after**.

---

## 4. Mistakes

Three, all in `MISTAKES.md` at the repo root, written when each was recognised.

1. **Changed a metric and kept its old control.** The bleed gate moved from "no
   neighbour alpha at a border" to "a whole texel of gap"; its control — the sheet
   with the margins stripped and the inner rects re-tiled — was carried across
   unchanged and measured **exactly 1.000 texels**, passing the check it exists to
   fail. Cost a three-minute two-bake run. The control now crops each frame to its
   own silhouette box and resizes it to fill its cell, so neighbours are zero texels
   apart by construction (0.431), with a floor printed beside it. *A control belongs
   to a metric, not to a gate.*
2. **Found: a mip-count check that had been agreeing by coincidence.** The harness
   asserted the shipped mip count with the pre-2026-09-09 expression ("levels until
   a frame spans eight texels"). CARDFIT3 changed the law in the code and left this
   check alone; the two agree only on the 48x64 frame this fixture happens to
   produce, and the check would have passed a sheet built to no law at all. It is
   now written as the law. *When a law moves, every check that restates it moves in
   the same edit.*
3. **Edited a shell script while that script was running.**
   `tools/bake_impostor_cards.sh` was patched (a header comment) at 22:11:57 while
   the same script was baking tree 14 of 19. Bash reads a script by byte offset;
   changing its length under a running shell can make it resume mid-line. It was
   harmless here only because the edited region had been consumed ten minutes
   earlier — luck, not a check. Found by reading the mtime table at the end of the
   lane. The bake did complete: 19 of 19 sets, no zero-length files, no failure
   lines. *Check that no job is running a file before patching it, comments
   included.*

Also found and repaired, though not mistakes of this lane's making: **seven
provenance anchors in the two contract pages could not be resolved** — one spanned
two source lines so no single line could contain it, two matched three places each
(`LODM_MAGIC`, `if ( auxDiv > 1 )`), one carried an ellipsis inside its own
backticks, and three were multi-site rows carrying one anchor and several numbers.
Each is now anchored on a fragment asserted UNIQUE in the current source, so the
scripted pass can keep them honest.

Two things this lane deliberately did **not** do: it did not change the mip cap away
from bungo's stated rule to remove the border bleed §2.2 measures (that is a design
decision and it is his), and it did not touch the aspect ladder, the dilation or
`card.center`.

---

## 5. Skill review

**Loaded and used:** `nifskope-ww-lodgen` (the build incantation, the exe lock, the
one-instance GUI rule, the bake driver's `CANDIDATES=trees`, and above all its
*"compile a harness's embedded Python BEFORE running the harness"* section — run
after every edit to `lodgen_octahedral.sh`, 7 blocks, all clean, which is why no run
was lost to a syntax error this time). `nifskope-ww-build-verify` (the gated chain
on `make`'s own exit code, `test exe -nt source`, `cmp` on the stylesheet, patch
scripts written with the Write tool and never a heredoc — and the one time I broke
that rule, patching a script through a heredoc, it ate a `\n` and produced a
SyntaxError exactly as the skill says it would). `ww-contract-provenance` (the five
steps, and its worked anchor script `scratchpad/rename_20260909/p14_anchors.py`,
copied and re-pointed rather than rewritten — that copy is most of why the pass came
out at 0 anchors not found). `nifskope-ww-resume-pending` (for the PENDING.md shape
once the game came up).

**Amended:** none. Nothing a skill claimed was disproved by this lane.

**Wished had existed, and what will recur:**

* **`ww-sheet-metrics`** — named by lane CARDFIT3 as "worth doing when a second
  sheet format needs them", and this lane is the second use. I re-derived the
  union-of-views box, the fill ratios, the mip rebuild with lodgen's filter, and a
  bleed metric with its control, from CARDFIT3's scripts, and then had to add two
  metrics they did not have (the coverage-weighted gap across a border, and the
  clear-run in texels). **The threshold matters and is exactly the kind of thing a
  skill saves**: my first table used `alpha >= 128` and disagreed with CARDFIT3's
  report by 0.17 on the headline number until I found their `COV = 16`. A written
  procedure that fixes the coverage floor, the mip filter and the four metrics
  would have saved that round trip and will save the next one. **I did not write it
  yet, and I am naming why rather than declining**: the scripts are in
  `scratchpad/cardpad_20260909/measure_gap.py` and are parameterised by the
  sidecar, so promoting them is mostly moving files — but they still read only the
  card sidecar's own line format, and a skill written now would be a skill about
  cards rather than about sheets. The honest next step is the third use, on an
  atlas or a VT tile, which is what would show which parts are general.
* **A skill for "draw a measured overlay on a texel-level picture"** — the
  checkerboard-under-transparency, the texel grid only where a texel is visible,
  the nearest-neighbour magnification, and the rule that a narrow frame must be
  shown WHOLE rather than cropped to a band (my first pass cropped a 16x64 frame to
  a ten-row band and it read as stripes, not as two trees). Every picture proof
  this repo owes bungo about a sheet or an atlas has this shape. Worth writing when
  the render-hook camera is fixed and the two kinds of picture — rendered and
  texel-level — can be described together.

**Declined:** a skill for the gap law itself. It is a contract, not a procedure, and
it lives in `docs/LODGEN_CARD_SHEETS.md` §3.1 where a reader of the format will find
it. It will not be re-derived; it will be read.
