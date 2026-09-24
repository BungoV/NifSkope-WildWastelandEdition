# LANE CLAMP -- the direct chunk bake gets the tile baker's one-cell ring

Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, **no commits**.
Read first: `CONSTITUTION.md`, `HANDOFF.md` top block,
`scratchpad/lane_vtfix_report.md`, `WW_CHANGES.md` 2026-09-09 (the V9a re-pin).
Skills loaded: `nifskope-ww-lodgen`, `ww-control-calibration`,
`ww-contract-provenance`, `nifskope-ww-resume-pending`.

All work under `scratchpad/clamp_20260910/`.

**STATUS: BUILD PENDING.** The code, the harness, the document, the instrument
and the before-sheets are on disk; nothing has been compiled. The build slot was
held when the code landed -- `scratchpad/water2_20260909/DONE` absent and a
`NifSkope.exe` (pid 4120) in the process table -- and the brief forbids polling.
The paste-able resume is `scratchpad/clamp_20260910/PENDING.md`. Every number
below is either lane VTFIX's 2026-09-09 measurement, a reading off the source,
or a control run on the OLD exe; none of them is a claim about a built change.

---

## 0. What was pre-registered, BEFORE any code was written or any sheet baked

The brief's bound is one number: *"no texel more than 4 from a chunk border may
change; zero-change interior is the control."* Reading the code first says that
bound is right for two of the three sheets and cannot be right for the third,
and this was written down before the measurement so it is a prediction and not a
fitted bar (CONSTITUTION 4).

The direct chunk bake reads the height grid in three places and each has its own
reach into the neighbourhood:

| consumer | reach | band it can move, dim 4, 32 units a texel |
|---|---|---|
| the `_msn` central difference (`lodgenTerrainHeightAt`, +-1 grid step) | 128 units | **<= 4 texels** |
| the colour sheet, through the ground-cover slope gate reading that normal's Z | the same 128 units | **<= 4 texels** |
| the `_data` sheet's R = AO, `for dist = 128; dist <= 2048; dist *= 1.5` | **2,048 units** | **<= 64 texels** |

So the pre-registered per-sheet bands are 4 / 4 / 64, the 64 read off the march
loop and not off a result. Beyond each band the count must be ZERO; that
zero-change interior is the control, on all three sheets.

## 1. The change

`src/lodgen.cpp`, the terrain bake only. Four things the two bakers must never
drift on now live in ONE home, at the top of the terrain section:

| helper | what | was |
|---|---|---|
| `lodgenTerrainHeightAt` | the eased reconstruction for the normal | already shared |
| `lodgenTerrainMsnPixel` | the msn byte order | already shared |
| **`lodgenTerrainFillRing`** | fills the `(rdim*32+1)^2` grid from a caller-supplied cell fetch | two copies |
| **`lodgenTerrainGridSample`** | the plain-bilinear tap into a sample grid | **five byte-for-byte copies** |

The chunk baker now builds its height grid on `dim + 2 * LODGEN_TERRAIN_RING_CELLS`
cells and offsets its own coordinates by `LODGEN_TERRAIN_RING_UNITS` (4,096) into
it. The msn central difference and the 2,048-unit AO march read that ring, so no
texel of the chunk is computed against a clamped edge. The tile baker was
rewritten onto the same two helpers, which is what makes the sharing real rather
than a second implementation with the same shape.

**Byte-exactness of the interior is arithmetic, not hope.** The old grid
coordinate was `( wx - cwX ) / span * float( hn - 1 )`; the new one is
`( wx - cwX + 4096 ) / 128`. `span = dim * 4096` and `hn - 1 = dim * 32` are both
exact powers of two for every `dim` the generator uses (4, 8, 16, 32), so the old
expression was already exactly `( wx - cwX ) / 128`, and `spacing` was already
exactly 128. The offset is added to values that are integer multiples of 16 at
dim 4. Nothing in the interior can move by a bit; only the clamp at the edge does.

**Two things deliberately did NOT move, and both are stated in the code:**

* **the PAINT stays scoped to the chunk** -- `cells`, `haveLand`,
  `dominantBase`, the per-quadrant cover constants. Every texel this bake writes
  lands inside the chunk, so a neighbour's paint has nothing to contribute, and
  widening the scope would move the dominant base -- which is precisely what
  V9a's cover-free half exists to catch.
* **the per-sample channels stay on the chunk grid** (`chgt`, a copy of the
  ring's middle). `lodgenTerrainChannels` computes wetness as a **flow
  accumulation over the whole grid it is handed**, so widening that grid moves
  the sheet's INTERIOR, not its edge -- and it still would not match the pyramid,
  whose tiles accumulate over a tile-sized grid rather than a chunk-sized one.
  The `_data` sheet therefore cannot be pinned to identity between the two paths
  by any ring, the harness says so in words instead of pinning a bar it cannot
  hold, and the fix -- a wetness domain that is not the bake unit -- is a
  separate track, named in section 6.

`docs/LODGEN_TERRAIN_VT.md` §2.4 said the ring was the one place the two paths
differ; it now says that difference is gone, names the `_data` exception, and
its provenance footer was re-derived (section 4).

## 2. The instrument, and the controls it was proved on FIRST

`scratchpad/clamp_20260910/edgeband.py` decodes mip 0 of a DXT1 or DXT5 sheet
from the format -- sharing no code with `lodgenWriteDds`, so a check cannot pass
because the writer and the reader agree with each other -- and reports, per
chunk and per sheet, the differing-texel count, the largest distance from the
chunk's outer boundary at which any of them sits, and the count at or beyond the
pre-registered band.

Run on two known-answer inputs on the OLD exe, before it was believed
(`ww-control-calibration` parts 1, 2 and 5):

| input | what it is | reading |
|---|---|---|
| the same sheet set against itself | the known answer, must read 0 | **0 differing on all 12 sheets**, and the FLOOR fired (`the colour sheets did not move at all: the ring is inert`), rc 1 |
| `--cover` against `--no-cover`, the same bake otherwise | the CEILING: the same data with the tinting property removed | **428,272 texels**, **416,280 of them at or beyond the 4-texel band**, the band bar failed on all four chunks, rc 1 |

The ceiling's count on `Commonwealth.4.-24.24` is **207,945**, which reproduces
lane VTFIX's independently written decoder to the texel. That agreement is the
check on the emulated decode, per the skill's "emulating a lossy stage".

Discriminating power on this data: the ceiling puts 416,280 texels outside the
band where the subject is bounded at 0, and the floor puts 0 inside it. The
independent replication is four chunks with four different paint sets, plus
**V9c**, a second instrument that asks a different question entirely --
continuity across a seam rather than difference between two bakes.

## 3. Gates

Pre-registered in `scratchpad/clamp_20260910/PENDING.md` §4 and §5, all of them
BEFORE the build. Nothing in this table has been run against the change.

| gate | bar | status |
|---|---|---|
| `lodgen_terrain_vt.sh` V9a, tint OFF | byte-identical, 4 chunks | pending (was green) |
| `lodgen_terrain_vt.sh` V9a, tint ON | **byte-identical, 4 chunks** -- tightened from a bounded band | pending |
| V9b, the `_msn` sheets | byte-identical, 4 chunks -- the operand that used to differ | pending |
| the FLOOR under both | each path's cover sheet must differ from its own cover-free sheet, `>= 2` of 8 pairs | pending |
| V9c, which normal is right | E/W seam <= 3.20x the interior step (ringed 2.75, clamped 4.07), N/S <= 3.30 (2.87 / 3.78), a sheet's own edge step <= 2.60 (1.961 / 3.310), interior control in 1.20..2.20 | pending |
| `edgeband.py`, cover run | colour and `_msn` move only within 4 texels, `_data` within 64, **zero beyond**, and colour + msn must move at all | pending |
| `edgeband.py`, cover-free run | the colour sheets **byte-identical before and after** -- without the tint the colour never reads the normal -- while `_msn` and `_data` move in their bands | pending |
| `lodgen_terrain.sh` | PASS, and the pyramid statistics UNCHANGED: `UP=G D0=76 D1=32 D2=51 D3=32` assembled and direct, vanilla `99/67/67/67` as the control | pending |
| `lodgen_identity.sh` | RESULT PASS, baseline UNMOVED | pending |

V9c's bars sit BETWEEN the two known readings rather than around one of them, so
they discriminate instead of accommodating; and the interior control is taken at
x = 100, 200, 300, 400, never one texel in, because a clamped bake's own edge
column is inside the defect and using it once reversed the verdict (lane VTFIX,
mistake 3).

## 4. The re-baseline

**There is no terrain-sheet hash pinned anywhere in `tests/spells/`, and that is
a reading of the files, not an assumption.** `lodgen_identity.sh` bakes
`--objects ... --no-ao` and compares `.bto` files and their manifests; no
terrain sheet is written on that path, and the Land VERTEX channels are untouched
because `lodgenTerrainChannels`'s other caller, on the mesh path, was not
changed. `lodgen_terrain.sh` checks the sheets structurally (fourCC, mip count,
up-in-green, the x-mod-4 roughness) and pins no bytes. So the identity gate does
not move, and the honest re-baseline is a MEASUREMENT plus the hashes beside it.

The before-sheets are baked and kept: `scratchpad/clamp_20260910/before/`, from
`release/NifSkope.exe` of 2026-09-10 00:13, four chunks x three sheets x cover
and cover-free. `Commonwealth.4.-24.24.DDS` with cover hashes
`aee0793ae2c1576b...` -- byte for byte the file lane VTFIX measured on
2026-09-09, which is what makes the before/after diff comparable with their
numbers. That directory is the only picture of the clamped behaviour there will
ever be and is not deleted.

`edgeband.py` prints the twelve new `sha256[:16]` values at the end of its cover
run. The resume writes them into `WW_CHANGES.md` **beside the band numbers that
justify them** -- a re-pin without a number is forbidden (CONSTITUTION 4).

## 5. Mistakes

Both written into `MISTAKES.md` at the root the moment they were recognised.

1. **A line number re-derived from the WRONG END of a range.** The provenance
   pass rewrote `lodgen.cpp:6831-6833` to `7083-7085` by treating the row's
   anchor as the range's first line; the anchor quotes `unitsPerTexel`, which is
   its LAST line, and the truth is 7081-7083. A plausible wrong number, which is
   the exact failure `ww-contract-provenance` warns about. Found by reading the
   anchor grep beside the script's output. Repaired by hand and the row's anchor
   re-pointed at `worldUnitsPerTile` so the range anchors its own start.
2. **The relative `--out-dir` trap, which is already in this file.** The before
   bakes were written with `--tex-dir before/cover/tex` and landed under
   `ns_before/before/cover/tex`, because a relative output path resolves against
   the EXE's folder. Exit code 0 and a full census row, and for a moment the
   reading was that the bake wrote nothing. Recording the repeat is itself the
   entry (CONSTITUTION 2); the defence is a rule about the argument -- every
   lodgen path is passed ABSOLUTE -- not a memory about the tool.

Not a mistake but worth the line: one patch was attempted through a bash
heredoc and its assertion refused, which is the third time this tree has paid
for that. Every patch here is a file written with the Write tool and run, as
`nifskope-ww-lodgen` says.

## 6. Owed, named so it is not lost

1. **The build and every gate in section 3.** `PENDING.md` is the resume.
2. **The `_data` sheet's wetness domain.** It is a flow accumulation over the
   bake unit, so a chunk's wetness and a tile's wetness are different fields at
   the same world point, and no ring closes that. Until it has a domain that is
   not the bake unit, the `_data` sheets of the two paths cannot be pinned to
   each other. Candidates, unmeasured and named as candidates: accumulate over
   the ring and accept the interior move; accumulate once per worldspace region
   and sample; or drop wetness from the sheet. **None of these is to be chosen
   without a measurement of what each does to the interior.**
3. **The Land VERTEX channels have the same clamp.** `lodgenTerrainChannels` is
   called a second time from the mesh path (`src/lodgen.cpp:903`) on a
   chunk-sized grid, so a `.btr`'s per-vertex AO carries the same false plateau
   at every chunk edge. Not touched here: it moves `.bto`/`.btr` bytes and
   `lodgen_identity.sh`'s own baseline, which is a different gate and a different
   lane.
4. **Five other documents cite `src/lodgen.cpp` by line and are stale.**
   `docs/LODGEN_CARD_SHEETS.md` (17 citations), `LODGEN_LODM_FORMAT.md` (13),
   `LODGEN_TEXTURE_ARRAYS.md` (12), `LODGEN_VERTEX_PACKING.md` (10),
   `F4FX_PROVENANCE.md` (2). They were ALREADY stale before this lane -- the
   footer of the one document lane CLAMP owns claimed 8,286 lines against a file
   that had 8,489 -- and this change adds 86 more (the anchors this lane
  re-derived moved by 252 lines in all, of which 166 were other lanes). Lane CLAMP owns only
   `LODGEN_TERRAIN_VT.md` and re-anchored that one (5 rows moved, 0 anchors
   missing, 1 multi-site row derived from both its anchors; new stamp
   `36e00f03dc138693`, 373,908 bytes, 8,574 lines). The other five want one
   scripted pass with `scratchpad/rename_20260909/p14_anchors.py`.

## 7. Finished-work skill review (CONSTITUTION 1a)

**Loaded and used.** `nifskope-ww-lodgen` -- the CLI table and worldspace IDs,
the rule about running a COPY of `release/` for a CLI run that must not block a
build (which is how the before-sheets exist at all while two other lanes hold the
link), the embedded-Python compile gate for a harness (`3 blocks ok`, run after
every edit), and the editing traps: patches written with the Write tool, anchor
counts asserted, line endings measured with Python byte counts. See mistake 2 for
the trap that is in the skill's own family and that I walked into anyway.
`ww-control-calibration` -- its five parts are section 2 one for one: the
known-answer input, the floor that fires when the effect is absent, the ceiling
from the same data with the property removed, the independent replication, and
checking the emulated decoder against another decoder's number before trusting
it. `ww-contract-provenance` was **not named in the brief** and was loaded
anyway, because the change moved `src/lodgen.cpp` by 87 lines and a document I
own cites it by line; it produced mistake 1 and its repair.
`nifskope-ww-resume-pending` -- the shape of `PENDING.md`, the staleness sweep
over every changed file rather than the one edited, the sequential harness chain
with its echoed summaries, and the rule that a resuming lane measures a failure
and does not land a cure. `nifskope-ww-build-verify` was named in the brief and
**not** loaded: no build was attempted, so its procedure never applied. Saying so
rather than implying it was followed.

**The skill that should exist, and that lane VTFIX already recommended:
`ww-sheet-diff`.** This lane is the second consumer in two days of exactly the
same three pieces, and re-derived all three: a BC1/BC3 mip-0 decoder sharing no
code with `lodgenWriteDds` (written a THIRD time here, as `edgeband.py`, and a
FOURTH time inside `lodgen_terrain_vt.sh`, because a spell may not import from
`scratchpad/`); the locality statistic that separates a chunk-boundary defect
from an interior one, with the band's share of the sheet printed beside it or
"100% within 4 texels" means nothing; and the seam-continuity test with its
interior control. Lane CLAMP adds two things the skill must carry that VTFIX
could not have known: **the band is per CONSUMER, and it is read off the code
before the bake** -- a normal's central difference reaches one grid step and an
AO march reaches 2,048 units, so one bound for a whole sheet set is wrong by
sixteen times; and **the floor belongs inside the diff tool**, because the
moment two sheets become byte-identical every band bar passes on nothing and only
a floor notices. The working code is `scratchpad/clamp_20260910/edgeband.py` plus
VTFIX's `ddsdiff.py` / `seam.py`. **The director should place it in both skill
trees**, since they drift.

**Declined, with reasons.** A skill for the ring itself: it is one function with
its contract in its own comment, and a skill nobody would think to load is a
skill nobody loads. A skill for "run three lodgen harnesses on a scratch copy of
the exe": four lines of shell, already the second half of a `nifskope-ww-lodgen`
bullet and now also §5 of this lane's `PENDING.md`.

## Build (BUILD4)

Built and gated by lane BUILD4 on 2026-09-10. Appended, not rewritten; the
lane's own text above is untouched. Skills loaded: `nifskope-ww-resume-pending`,
`nifskope-ww-build-verify`, `nifskope-ww-lodgen`, `ww-texel-picture`.

### B.1 The build

`Fallout4.exe` and `NifSkope.exe` both absent (`rc=1`) before the build and
before every exe launch below.

`qmake NifSkope.pro` then `make -j2`, per `nifskope-ww-resume-pending` §3 --
qmake first because lane WATER2 added a NEW `#include "esmfile.hpp"` to
`src/lodtfile.cpp`, which a frozen dependency list cannot see.

| step | result |
|---|---|
| `QMAKE-RC` | 0 |
| `BUILD-RC` | 0, `make[1]: Nothing to be done for 'first'` |
| staleness sweep, all 15 changed files under `src/ res/ tools/ tests/` | 0 STALE |
| `cmp res/style.qss release/style.qss` | in step |

**The exe already carried this lane's code.** Lane WATER2's link at 01:01:04
post-dates every changed source, `src/lodgen.cpp` at 00:40:11 included, and the
object-level check confirms it rather than inferring it: `lodgen.o` 00:44:00,
`nifskope_ui.o` 00:44:24, `lodtfile.o` 01:01:01, and every one of the 31 objects
that include `glview.h`, the 5 that include `lodtfile.h` and the 2 that include
`btdterrain.h` is newer than its header. So no relink was owed and none happened.

**One finding, recorded in `MISTAKES.md`:** even AFTER the qmake run,
`Makefile.Release`'s dependency block for `lodtfile.o` names only
`src/lodtfile.cpp src/lodtfile.h src/esmdata.h src/io/lodvfile.h` -- not
`lib/libfo76utils/src/esmfile.hpp`. qmake's scan does not follow into `lib/`.
Harmless today (the object is 01:01:01 and the header is 2026-08-31), latent
tomorrow.

### B.2 Every clock in one table (CONSTITUTION 4)

| artefact | mtime |
|---|---|
| `release/NifSkope.exe` | 2026-09-10 01:01:04 |
| `release/style.qss` | 2026-09-10 01:01:04 |
| `src/lodgen.cpp` | 2026-09-10 00:40:11 |
| `tests/spells/lodgen_terrain_vt.sh` | 2026-09-10 00:22:51 |
| `docs/LODGEN_TERRAIN_VT.md` | 2026-09-10 00:26:13 |
| `scratchpad/clamp_20260910/before/.../-24.24.DDS` | 2026-09-10 00:16:46 |
| `scratchpad/clamp_20260910/after/.../-24.24.DDS` | 2026-09-10 01:07:31 |
| `scratchpad/clamp_20260910/edgeband_cover.txt` | 2026-09-10 01:07:50 |
| `scratchpad/clamp_20260910/logs/terrain_vt.log` | 2026-09-10 01:09:16 |
| `scratchpad/build4_20260910/land.bin` | 2026-09-10 01:11:08 |

### B.3 The gate table, against section 3's pre-registration

Both AFTER bakes ran with ABSOLUTE `--out-dir` and `--tex-dir` (mistake 2 of the
lane's own section 5), rc 0 each, **24 of 24** `.DDS` written.

| gate | bar | measured | verdict |
|---|---|---|---|
| V9a, tint OFF | byte-identical, 4 chunks | byte-identical | ok |
| V9a, tint ON | byte-identical, 4 chunks | byte-identical | ok |
| V9b, `_msn` | byte-identical, 4 chunks | byte-identical | ok |
| the FLOOR under both | >= 2 of 8 pairs differ | **8 of 8** | ok |
| V9c E/W ratio | <= 3.20 (ringed 2.75, clamped 4.07) | **2.75**, interior 1.803 | ok |
| V9c N/S ratio | <= 3.30 (2.87 / 3.78) | **2.87**, interior 1.603 | ok |
| V9c edge step | <= 2.60 (1.961 / 3.310) | **1.961** | ok |
| `lodgen_terrain_vt.sh` | RESULT PASS, 32 -> 35 checks | **35 checks, 0 failures, PASS** | ok |
| `lodgen_terrain.sh` | PASS, pyramid stats unchanged | **26 checks, 0 failures**; assembled and direct both `UP=G D0=76 D1=32 D2=51 D3=32`; vanilla `99/67/67/67` | ok |
| `lodgen_identity.sh` | RESULT PASS, baseline unmoved | **8 ok, RESULT PASS** | ok |
| `edgeband.py` colour, cover | <= 4 texels, 0 beyond, must move | 127 moved, maxd 3, **0 beyond** | ok |
| `edgeband.py` `_msn`, cover | <= 4 texels, 0 beyond | 25,537 moved; maxd 3 and 0 beyond on the y=24 chunks; **maxd 7, 1,014 and 1,405 beyond** on the y=28 chunks | **MISS** |
| `edgeband.py` `_data`, cover | <= 64 texels, 0 beyond | 65,872 moved; maxd 47, 0 beyond on three chunks; **maxd 79, 16 beyond** on `-20.28` | **MISS** |
| `edgeband.py` colour, cover-free | byte-identical | **byte-identical, all four chunks** | ok |
| `edgeband.py` `_msn`/`_data`, cover-free | move inside their bands | identical readings to the cover run | as cover |

V9c's three numbers landed on the pre-registration to three significant figures
(2.75 / 2.87 / 1.961 predicted, 2.75 / 2.87 / 1.961 measured), which is the
strongest single result of the round: the direct sheets now reproduce lane
VTFIX's RINGED readings because they are the same bytes.

The FLOOR line `the colour sheets did not move at all: the ring is inert` prints
on the **cover-free** run and that is by construction -- §4's second control
asks the colour sheets NOT to move there. On the cover run, which is what the
floor guards, it stayed silent (127 texels moved).

### B.4 The new baseline, beside the numbers that justify it

The twelve `sha256[:16]` of the cover AFTER sheets are in
`scratchpad/clamp_20260910/edgeband_cover.txt` and are copied into
`WW_CHANGES.md` next to the band table, never alone (CONSTITUTION 4).

### B.5 The two misses, cause MEASURED, neither fixed nor re-pinned

`nifskope-ww-resume-pending` §6: a resuming lane's product is a verdict.

**Localised** (`scratchpad/build4_20260910/localise.py`, which reuses
`edgeband.py`'s decoder rather than writing a fourth one):

| sheet | beyond-band texels, by nearest border | distance histogram |
|---|---|---|
| `-24.28 _msn` | W=10 E=10 **N=994** S=0 | 4:290 5:298 6:241 7:185 |
| `-20.28 _msn` | W=0 E=0 **N=1,405** S=0 | 4:388 5:394 6:347 7:276 |
| `-20.28 _data` | E=16 | 76:4 77:4 78:4 79:4 -- exactly one 4x4 BC block |
| `-24.24 _msn` (a PASSING chunk, as control) | W=242 E=277 N=262 S=218 | 3:999 -- one distance only |

**The cause, from the MASTER and not from our output**
(`scratchpad/build4_20260910/seamcheck.py` over `--dump-land`, cells
x=-24..-17): Bethesda's landscape disagrees across exactly ONE shared vertex row
in this neighbourhood.

| shared row | max &#124;difference&#124;, VHGT units of 8 |
|---|---|
| y=23 &#124; y=24 | 0 across all eight columns |
| y=27 &#124; y=28 | 0 across all eight columns |
| **y=31 &#124; y=32** | **2, 1, 4, 6, 9, 8, 7, 4** (16..72 world units) |
| y=32 &#124; y=33 | 0 across all eight columns |
| both east seams, x=-21 and x=-17, y=24..31 | 0 |

`lodgenTerrainFillRing`'s documented south-to-north order therefore lets the
y=32 cell overwrite the row it shares with y=31 -- which is the chunk's OWN
boundary grid row. Everywhere else the two cells agree exactly, so the ring can
only change points OUTSIDE the chunk.

The reach then follows arithmetically, and the two histograms are its two cases:
a texel at distance `t` reads heights at `t +- 4` texels through a bilinear tap,
so grid index -1 is read out to **t = 3** (every passing border: one distance,
3) and grid index 0 out to **t = 7** (the north border: 4,5,6,7 and nothing
further). **The pre-registered band of 4 counted the central difference's
128-unit step but not the bilinear tap's own 128-unit footprint, and assumed the
ring could only touch points outside the chunk.** Section 0's own words bind
here: the band is not to be adjusted to fit what came back, so it is reported
missed and left for the lane or the director.

The 16 `_data` texels are one BC block, and the mechanism is already owed item 2
above: `chgt` copies the ring's middle, whose north boundary row moved, and
wetness is a flow accumulation over that whole grid -- a global operator, so it
has no band at all. Nothing new was learned; the outlier is that defect showing
through.

### B.6 What was NOT done

* **Nothing was committed** (CONSTITUTION 8).
* No fix was landed for either miss, and no band was re-pinned.
* `scratchpad/clamp_20260910/before/` is kept -- two gates are red, and it is
  the only picture of the clamped behaviour there will ever be (`§8` of
  `nifskope-ww-resume-pending`). `ns_before/` is kept for the same reason.
* Harnesses NOT run, with the reason: everything in `tests/spells` outside the
  three above. The change is confined to the terrain bake in `src/lodgen.cpp`;
  it reaches no NIF writer, no panel, no collision, no `.lodl`/`.lodt` reader.
* bungo's open NifSkope window predates the exe and needs a restart.
