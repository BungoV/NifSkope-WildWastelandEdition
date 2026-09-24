# Lane VT1 — the pyramid-assembled sheet vs the direct bake, 2026-09-16

Tree `E:/Projects/NifskopeWildWastelandEdition`, branch main. Nothing committed.

Rung, taken before any build of this lane (`ls -la` first, then `cp -p`):

```
-rwxr-xr-x 1 bungo 197609 22288896 Sep 16 11:54 release/NifSkope.exe
-rwxr-xr-x 1 bungo 197609 22288896 Sep 16 11:54 release/NifSkope.before_vt1.exe
```

That exe is NATIVEVIEW2's link (11:54:47, 22,288,896 B). `date` read 12:27:45 at
the start of this lane; every time in this file is a read clock, never elapsed
feel. Game check before the copy: `tasklist | grep -i -E "Fallout4|NifSkope"`
returned nothing (rc=1) — no Fallout4, no NifSkope. NATIVEVIEW2 is not running,
so there is no build mutex in this lane.

---

## 0. The cause

### The bytes, first

The two bakes DEFAULTS1 left on disk (`scratchpad/defaults1_20260912/v9a`,
2026-09-12 21:11) are the evidence; no new bake was needed to find the cause.
Region `-24 24 -17 31 --dim 4`, ruled land default, cover off:

| chunk | differing bytes of 174,888 |
|---|---|
| `Commonwealth.4.-24.24` | 0 |
| `Commonwealth.4.-20.24` | 0 |
| `Commonwealth.4.-24.28` | **4** |
| `Commonwealth.4.-20.28` | **27** |

Decoded (`diffmap.py`): every differing byte is a **BC1 index byte** of a mip-0
or mip-1 block — in 13 of the 15 differing blocks the two endpoint colours
`c0`/`c1` are bit-identical and only the 2-bit selectors move. So the two paths
agree on what colours are in the block and disagree on which of them a handful
of texels takes. 33 texels differ at mip 0 across the two chunks (4 and 29), by
2–8 counts a channel.

Where they are, in sheet texels (512×512, one texel = 32 world units, row 0 is
the NORTH edge):

| chunk | differing texels, x | differing texels, y |
|---|---|---|
| `...-24.28` | 247–254 | 0–17 |
| `...-20.28` | 244–267 | 0–17 |

x ≈ 256 is the **internal seam of the pyramid assembly** — a dim-4 chunk is the
2×2 content blocks of the dim-2 level (`lodgen.cpp:11425`, `assembleChunkRow`),
so texel 256 is where two dim-2 tiles meet. y ≈ 0 is the chunk's own **north
cell line**, y = 32 (world Y = 131,072).

### The code line

`lodgenTerrainFillRing` (`src/lodgen.cpp:5724`) fills the ring height grid both
sheet writers read. Its rule, bungo's 2026-09-10 ruling "the cell owns it", is:

```cpp
const bool ringCell = haveInner
    && ( cx < LODGEN_TERRAIN_RING_CELLS
        || cx >= rdim - LODGEN_TERRAIN_RING_CELLS
        || cy < LODGEN_TERRAIN_RING_CELLS
        || cy >= rdim - LODGEN_TERRAIN_RING_CELLS );
...
if ( ringCell && rowInside && gc >= innerLo && gc <= innerHi )
    continue;   // the inner unit's own sample: it owns it
```

`innerLo`/`innerHi` are **derived from the caller's own `rdim`**. The chunk
baker's inner unit is the dim-4 chunk; the tile baker's is the dim-2 tile. So
the same world sample gets a different answer on the two paths — and it is not
inside either inner unit, it is in the **ring**, where the fill is plain
later-wins (south→north, west→east) and the NORTH cell overwrites the shared
VHGT row.

Worked, for `Commonwealth.4.-20.28`, at world Y = 131,072 (the cell line y=32),
under cell x = −18:

* **chunk grid** (inner unit = cells −20..−17 × 28..31): cell (−18,31) is an
  INNER cell and writes its own top row there; cell (−18,32) is a ring cell and
  the whole of that row is inside the inner box, so it is skipped. The sample is
  **cell 31's row 32**.
* **tile (−20,30) grid** (inner unit = cells −20..−19 × 30..31): cell (−18,·) is
  now a RING cell in x, so the inner box no longer covers those columns; cell
  (−18,31) writes there and then cell (−18,32), visited later, **overwrites it**.
  The sample is **cell 32's row 0**.

Bethesda's two cells disagree on that shared row — measured by lane BUILD4 and
re-measured here: up to **56 world units** on tile (−20,30) and **64** on tile
(−18,30), 31 samples each.

`ringdiff.py` reproduces `lodgenTerrainFillRing` offline against the already
dumped Commonwealth VHGT (`scratchpad/build4_20260910/land.bin`) and finds the
disagreement without a build:

```
tile (-20,28) dim 2: overlap 129x129, differing samples  0, max |dh|  0.0 u
tile (-18,28) dim 2: overlap 129x129, differing samples  0, max |dh|  0.0 u
tile (-20,30) dim 2: overlap 129x129, differing samples 31, max |dh| 56.0 u
tile (-18,30) dim 2: overlap 129x129, differing samples 31, max |dh| 64.0 u
```

— one grid row only (1024, i.e. world Y 131,072) and the columns of the ring
cell beside the tile seam.

### Why the warp is the carrier, and why `--land-warp 0` and `--land-guide off`
### both hid it

Under the ruled default the guide rule is FLATWARP at strength 1
(`lodgen.cpp:6171`): the warp amplitude is `341 * (1 − min(1, |macro slope| /
0.5))`. The **macro slope** is a Sobel over that same ring height grid at ±512
world units (`lodgenLandMacroGradient`, `lodgen.cpp:6202`). So a grid sample the
two paths disagree on changes the amplitude, which moves the land diffuse
lookup, which occasionally crosses a texel and changes one BC1 selector.

* `--land-warp 0` → amplitude 0 → `lodgenLandWarpAt` returns the coordinate
  untouched, so the disagreement has nothing to act on.
* `--land-guide off` → the macro gradient is **not computed at all** (both
  sampling sites guard it with `lodgenLandGuideRule() != LODGEN_LANDGUIDE_OFF`),
  so the grid disagreement is never read.
* hex and mip bias do not read the height grid at all — innocent, as DEFAULTS1
  measured.

That is the whole chain, and it is a WORLD-POSITION violation: the texel's value
depends on which inner unit it was baked inside.

### The probe, with a floor and a refuter

`texelpredict.py` computes the macro slope at every one of the 262,144 texels of
a chunk on BOTH grids and marks the texels where it differs. `predict_check.py`
then asks whether the texels that ACTUALLY differ are inside that set:

```
Commonwealth.4.-24.28    actual differing texels    4   predicted set   144   outside prediction 0
    refuter ok: shifted 8 texels east, 2 texels fall outside
Commonwealth.4.-20.28    actual differing texels   29   predicted set   384   outside prediction 0
    refuter ok: shifted 8 texels east, 14 texels fall outside
Commonwealth.4.-24.24    actual differing texels    0   predicted set     0   outside prediction 0
Commonwealth.4.-20.24    actual differing texels    0   predicted set     0   outside prediction 0

10 checks, 0 failures
```

Four things make that a measurement and not an agreement:

1. **Containment**, 0 texels outside the prediction on either chunk.
2. **The prediction is small** — 144 and 384 texels of 262,144 (0.05% and
   0.15%), so containment is not vacuous; the check fails if the predicted set
   ever reaches a quarter of the sheet.
3. **It predicts the innocent chunks too**: the two chunks whose sheets are
   byte-identical are exactly the two where the grids do not disagree at all.
4. **The refuter fires**: the same prediction shifted 8 texels east stops
   containing the observed texels (2 and 14 outside).

---

## 1. The fix

### What changed, in one sentence

`lodgenTerrainFillRing` no longer derives the protected inner unit from the
caller's own `rdim`; the caller may NAME it, and the tile baker names the box of
the **chunk it will be assembled into**. One filler, one rule, two callers who
agree about which cell owns a shared VHGT row.

Three edits, all in `src/lodgen.cpp`:

1. **A named box.** A small `LodgenRingInner { given, loX, hiX, loY, hiY }`
   and a defaulted parameter on `lodgenTerrainFillRing`. `given == false` is
   the old derived box, bit for bit: the chunk baker passes nothing and is
   unchanged. The inner test also moved from "is this cell within
   `LODGEN_TERRAIN_RING_CELLS` of the grid edge" to "does this cell's own 33x33
   block sit wholly inside the box", which is the SAME predicate when the box
   is the derived one and is the thing that generalises when it is not.
2. **The tile baker names the chunk.** In `lodgenBakeVtTile`, before the fill:
   `px0 = lodgenVtFloorTo( cellX0, dim * 2 )`, same on Y, and the box is that
   chunk's closed grid rectangle in the tile grid's own coordinates. Every VT
   level shares one origin aligned to the coarsest dim (docs section 2.1), so
   the parent chunk is fixed -- no option, no level index, no region rectangle
   in it. That is what makes the result a function of world position.
3. **The self-test learned the case.** `lodgenTerrainRingSelfTest` gained a
   PART TWO: a synthetic grid where the south cell and the north cell disagree
   about their shared row on purpose, and three assertions -- the derived box
   takes the north cell's copy at the sample, the chunk box takes the south
   cell's, and (the CONTROL) those two must DIFFER, so the test cannot pass by
   the two rules being the same rule. A fourth asserts that naming the derived
   box explicitly changes not one sample, which is the no-op guarantee the
   chunk baker depends on. It runs in every bake under `WW_TERRAIN_RING_TEST=1`
   and the harness now greps its verdict: **13 checks, 0 failures, RESULT
   PASS**, inside the bake process.

### The proof: the same four chunks, baked four ways

Region `-24 24 -17 31 --dim 4`, bare ruled land default, `--road-detail 1`,
cover off. The rung is `release/NifSkope.before_vt1.exe` (22,288,896 B,
2026-09-16 11:54), the new exe is `release/NifSkope.exe` (22,293,504 B, 12:49:19).
Counts are differing BYTES of 174,888, by `cmp -l`.

| chunk | RUNG: assembled vs direct | NEW: assembled vs direct | DIRECT: rung vs new | ASSEMBLED: rung vs new |
|---|---|---|---|---|
| `Commonwealth.4.-24.24` | 0 | 0 | 0 | 0 |
| `Commonwealth.4.-20.24` | 0 | 0 | 0 | 0 |
| `Commonwealth.4.-24.28` | **4** | **0** | 0 | 4 |
| `Commonwealth.4.-20.28` | **27** | **0** | 0 | 27 |

Read the columns in order and the whole lane is in them:

* column 1 is **the refuter** brief item 3 asks for -- the rung exe fails the
  new arm with exactly the 4 and 27 bytes DEFAULTS1 reported on 2026-09-12;
* column 2 is the fix;
* column 3 is **why the STOP clause does not apply**: the direct bake writes
  the same bytes it wrote before. Not on the colour sheets alone -- **all 12
  files of the direct bake, 0 differ**, colour, `_msn` and `_data` together.
  No ruled default moved, and the reference the gate compares against did not
  move either;
* column 4 is the look that DID move, and it moved onto column 3.

What moves on the pyramid side, whole:

```
DIRECT bake, rung vs new:     12 files, 0 differ
ASSEMBLED bake, rung vs new:  15 files, 6 differ
    Commonwealth.4.-20.28.DDS        27 bytes      Commonwealth.4.-20.28_data.DDS   20 bytes
    Commonwealth.4.-24.28.DDS         4 bytes      Commonwealth.4.-24.28_data.DDS   47 bytes
    Commonwealth.8.-24.24.DDS        18 bytes      Commonwealth.8.-24.24_data.DDS   25 bytes
the .lodt containers:  VT.2 435 bytes of 2,231,776   VT.4 101 of 560,608   VT.8 52 of 142,816
```

The `_msn` sheets do not move on either path, which is consistent with the
cause: `_msn` never reads the macro slope. The `_data` sheets move because the
wetness term is an accumulation over the same ring grid; the harness already
states that `_data` is not pinned to identity across the two paths and names it
as owed, and this lane does not change that.

The dim-8 assembled sheet moved too (18 bytes) and there is **nothing to
compare it against**: the direct chunk path writes no terrain sheets at
`--dim 8` (measured: `sheets=0` from a bake that otherwise reported
`2 chunk(s) written, 0 empty, 0 failed`). The rule is level-independent by
construction -- a level-D tile names `floorTo( cellX0, 2D )` -- but that is an
argument, not a measurement, and it is named under Owed rather than claimed.

### Offline first, before any build

The rule was modelled against the dumped Commonwealth VHGT
(`scratchpad/build4_20260910/land.bin`) before a line of it was compiled, with
its own floor and refuter (`ringfix_model.py`):

```
chunk dim 4                  OLD rule WORLD-BOX rule
Commonwealth.4.-24.24            0 (0 u)       0 (0 u)
Commonwealth.4.-20.24            0 (0 u)       0 (0 u)
Commonwealth.4.-24.28           46 (32 u)       0 (0 u)
Commonwealth.4.-20.28           62 (64 u)       0 (0 u)
FLOOR  the world-box rule must give 0 differing samples: ok
REFUTER the old rule must give MORE than 0 on this set: ok (108)
```

108 disagreeing height samples went to 0, and the bake then agreed on the bytes.

### The picture (a look moved, so one pair is owed)

`scratchpad/vt1_20260912/images/vt1_pyramid_sheet_rung_vs_new.png` -- the
assembled sheet `Commonwealth.4.-20.28`, mip 0, crop x 232..279 y 0..23 at 14x
nearest, three panels: RUNG, NEW, and where they differ. The seam column x=256
is drawn on both sheet panels. Burned into the image: 27 bytes of 174,888, 29
texels of 262,144, and the assembled-vs-direct counts 27 (rung) and 0 (new).
Both panels are real DDS files off disk decoded here, not through the writer's
own code.

---

## 2. Gates

Every count below is beside the exe it was run against:
**`release/NifSkope.exe`, 22,293,504 bytes, 2026-09-16 12:49:19.**
Rung for the refuters: `release/NifSkope.before_vt1.exe`, 22,288,896 bytes,
2026-09-16 11:54 (NATIVEVIEW2's link).

| gate | result | counts |
|---|---|---|
| `tests/spells/lodgen_terrain_vt.sh` (both arms) | **FAIL** | 44 checks, **1 failure** -- V9c, and only V9c |
| `tests/spells/lodgen_terrain.sh` | PASS | 26 checks, 0 failures |
| `tests/spells/lodgen_native_baseline.sh --check` | PASS | 25 files in the baseline, 25 baked, **0 differ**, 0 failures |
| `tests/spells/lodgen_defaults.sh` | **SKIPPED** | exit 2 before any check: see below |
| `tests/spells/lodt_write.sh` | **SKIPPED** | the file is not in the tree: see below |

### The VT suite, both arms

The suite was 43 checks and is 44; the arm this lane added is inside it:

```
ok   V9a with the ground-cover tint OFF the assembled colour sheet is byte-identical to a direct bake, on all four dim-4 chunks
ok   V9a with the ground-cover tint ON  the assembled colour sheet is byte-identical to a direct bake, on all four dim-4 chunks
ok   V9a-3 under the four OLD land switches the assembled colour sheet is byte-identical to a direct bake too
ok   FLOOR the two looks really are different bakes, so V9a-3 was asked of the OLD one
     ring: self-test 13 checks, 0 failures, RESULT PASS
ok   the ring fill's own self-test passes in the bake process, with its two refuters
ok   V9b the assembled and direct _msn sheets are byte-identical
FAIL V9c the direct sheets are continuous ACROSS a chunk seam
44 checks, 1 failures
RESULT FAIL
```

The first three lines are brief item 3: the suite now bakes on the BARE ruled
default (`LANDARGS` empty) and keeps a second arm that spells the four old
switches, and both must pass. The FLOOR beneath them is not decorative -- it
asserts the two arms are different bakes on at least 3 of the 4 chunks, so a
change that made the default pass by quietly reverting the look would fail
here rather than pass twice.

### The two skips, named

**`tests/spells/lodt_write.sh` does not exist.** 86 shell spells are in
`tests/spells/`; none is called that. It is named in `HANDOFF.md` (line 5883),
in the `nifskope-ww-lodgen` skill and in three older briefs, and the only copy
on disk is `scratchpad/build1_20260909/lodt_write.sh.orig`. `git log` has no
commit touching that path and `git status` does not list it, so it was never in
this tree under that name. The lane did not resurrect it: restoring a retired
gate is a director's call, and a gate nobody has run since 2026-09-09 is not
evidence about today's exe. **Nothing this lane changed can reach the `.lodt`
writer** -- `src/lodtfile.cpp` is byte-for-byte untouched (141,680 B, measured)
-- and the `.lodt` containers ARE exercised: the VT suite reads, validates and
mutation-tests three of them, and it passes every one of those checks.

**`tests/spells/lodgen_defaults.sh` refuses to start.** It exits 2 with
`no rung at /e/.../release/NifSkope.before_defaults1.exe`. That harness is built
on a PRE-2026-09-12 exe: every phase compares the new exe's default bake against
that rung with the old switches spelled. The only rungs on disk are today's
11:40 and 11:54 links, both of which already carry the new defaults, so running
it with `RUNG=` pointed at either would turn its refuters into false reds (phase
(a)'s "the rung with `--no-identity` wrote NO manifest" cannot hold on an exe
where the sidecar no longer depends on the flag, and phase (b) compares the new
exe's OLD-switch bake against the rung's DEFAULT bake, which on a post-DEFAULTS1
rung is the new look). Rebuilding the pre-DEFAULTS1 exe means moving git state,
which this lane is forbidden to do.

What stands in its place, measured rather than assumed: **`lodgen_defaults.sh`
guards that the default bake did not move, and this lane proves that directly**
-- the direct chunk bake of the probe region is byte-identical between the rung
and the new exe across all 12 files, and `lodgen_native_baseline --check`
re-bakes 25 output files against pinned hashes with 0 differing. The
`28/0` the brief asks for is NOT claimed; the gate did not run, and reinstating
it is bungo's call.

---

## 3. Build and chain

Game check, as its own command, before anything was built:

```
tasklist | grep -i -E "Fallout4|NifSkope"   ->  nothing, rc=1     (12:48:43)
```

No Fallout4.exe, no NifSkope.exe -- so no build mutex was needed and no window
of bungo's was at risk. The build then ran through the in-tree gated chain,
`bash tools/ww_build.sh src/lodgen.cpp`, which is the `nifskope-ww-build-verify`
chain as one script:

```
exe not held by a window
BUILD-RC=0
-rwxr-xr-x 1 bungo 197609 22293504 12:49:19 release/NifSkope.exe
exe newer than the sources
copies in step
WWBUILD-RC=0
```

The gates on that, each one read rather than assumed:

* **make's own exit code**, not a grep's: `BUILD-RC=0` is `make -j2`'s rc,
  captured before the error grep runs.
* **The exe is newer than EVERY changed file**: exe 12:49:19 against
  `src/lodgen.cpp` 12:43:01, `tests/spells/lodgen_terrain_vt.sh` 12:44:48,
  `docs/LODGEN_TERRAIN_VT.md` 12:48:37.
* **The stale-object check**: `GeneratedFiles/.obj/lodgen.o` is 12:49:17, after
  its source at 12:43:01. No header was touched this lane -- the new
  `LodgenRingInner` type is file-local to `lodgen.cpp` and nothing outside it
  can see it -- so `lodgen.o` is the whole set, and `src/lodgen.h` is unmodified.
* **make -n prints zero compile lines** after the build: 0 lines match a compile
  step, so nothing was left to build behind the link.
* The suite's OWN preflight agrees from the other side: `ok the exe is newer
  than every source this answer depends on`, over eight named sources.

Nothing was committed, stashed or reset. `git status` is exactly what it was
before this lane apart from the three changed paths.

---

## 4. Owed / red / bungo's calls

### V9c, the seam -- red, measured, and NOT this lane's cause

The brief asks for the cause with numbers, and for a fix only if it is the same
root cause as item 1. **It is not**, so nothing was changed for it.

**First, that it is not this lane's.** V9c reads the `_msn` sheets of the DIRECT
bake only, and those are byte-identical between the rung and the new exe on all
four chunks. The numbers are identical to the last digit:

```
direct bake                      E/W seam interior  ratio | N/S seam interior  ratio | edge E/W edge N/S
NEW exe, bare ruled default       188.074   13.243  14.20 |   35.857   12.182   2.94 |   14.348   11.905
RUNG exe, bare ruled default      188.074   13.243  14.20 |   35.857   12.182   2.94 |   14.348   11.905
NEW exe, four OLD land switches   188.074   13.243  14.20 |   35.857   12.182   2.94 |   14.348   11.905
NEW exe, --land-warp 0 only       188.074   13.243  14.20 |   35.857   12.182   2.94 |   14.348   11.905
NEW exe, --land-guide off only    188.074   13.243  14.20 |   35.857   12.182   2.94 |   14.348   11.905
```

Five bakes, one number. The land default does not reach the `_msn` sheet at all,
so item 1's carrier -- the macro slope the warp is steered by -- cannot be V9c's
cause either.

**What the cause is, in two parts, both proven.**

1. **V9c decodes a DXT5 file with a DXT1 reader.** The `_msn` sheet is **DXT5**,
   349,680 bytes, 512x512, 10 mips -- exactly twice the colour sheet's 174,888
   because a DXT5 block is 16 bytes, 8 of alpha then 8 of colour. V9c's decoder
   walks the payload in 8-byte steps, so every second "block" it decodes is an
   ALPHA block read as colour. The fingerprint is unmistakable: **four fully
   white columns out of every eight, 131,072 texels of 262,144, exactly 50%** --
   and (255,255,255) is not a unit normal in the sheet's own R=east G=up B=north
   encoding, so it is not a normal at all. The colour sheet decoded by the same
   reader has **0 white texels**.

2. **The bars were pinned on a file the generator no longer writes.** All four
   `_msn` sheets in this region are **byte-identical to vanilla's own shipped
   files** (349,680 B each, from the unpacked Data). That is by design and by
   bungo's ruling, written out at `lodgen.cpp:6680`: a chunk that has a shipped
   vanilla `_msn` gets VANILLA'S FILE, copied byte for byte, and our normal bake
   is skipped for it. V9c's bars -- interior control 1.20..2.20, edge step at
   most 2.60 -- were pinned on 2026-09-09 against OUR generated sheet. They are
   absolute magnitudes, and they are being asked of Bethesda's file.

**With the sheet decoded as what it is, the seam is fine.** Same statistic, same
files, DXT5 reader:

```
                          E/W seam   interior   ratio          N/S seam  interior  ratio
ours, read as DXT1 (V9c)   188.074     13.243   14.20            35.857    12.182   2.94
ours, read as DXT5          28.516     28.804    0.99            33.426    25.488   1.31
vanilla, read as DXT1      188.074     13.243   14.20            35.857    12.182   2.94
vanilla, read as DXT5       28.516     28.804    0.99            33.426    25.488   1.31
```

The E/W ratio that V9c calls 14.20 against a bar of 3.20 is **0.99** -- the step
across the chunk seam is the same size as the step between two ordinary
neighbouring columns. The N/S ratio is 1.31 against a bar of 3.30. **There is no
seam discontinuity in these sheets.** What remains outside the bars under the
right reader is the interior magnitude itself (28.8 where the bar says
1.20..2.20) and the absolute edge step (21.1 where the bar says 2.60), and both
are properties of vanilla's own sheet, identical in vanilla's file and ours.

**The candidates, named, for whoever takes it:**

* (a) fix the reader in `tests/spells/lodgen_terrain_vt.sh` -- V9c's decoder
  should read the block stride from the fourCC and skip DXT5's alpha half. This
  is a harness change, inside this lane's writer list, and it was NOT made
  because the brief says fix only on a shared root cause;
* (b) re-pin the interior and edge bars against a sheet that is actually being
  written today, and say in the check which chunks are vanilla copies -- a seam
  RATIO is meaningful on a copied sheet, an absolute magnitude is not;
* (c) decide whether V9c should run on chunks that reuse vanilla's `_msn` at
  all. It is measuring Bethesda's continuity, not ours.

The lane's recommendation is (a) then (b), as one small harness change with the
old numbers kept beside the new ones. **It is bungo's call, and nothing landed.**

### Also owed, named here rather than fixed

* **`--tex-dir` with a RELATIVE path silently writes no sheets and exits 0.**
  The bake runs to the end, reports `0 empty, 0 failed`, prints its stage times
  and census, and leaves the directory empty; `--out-dir` relative works and
  even echoes the relative path back, which is what makes it convincing. It cost
  this lane four bakes. A generator defect, outside the brief.
* **The rule above dim 4 is argued, not measured.** A level-D tile names
  `floorTo( cellX0, 2D )`, so the fix is level-independent by construction, and
  the assembled dim-8 sheet did move (18 bytes) in the direction the argument
  predicts. But the direct chunk path writes **no** terrain sheets at `--dim 8`,
  so there is no reference to compare it with. Either the direct path should
  write them, or the check should be built another way.
* **`_data` is still not pinned across the two paths**, and this lane did not
  change that. Its wetness term accumulates over whichever grid its baker was
  handed. The `_data` sheets moved by 20 and 47 bytes with the fix, in the same
  places and for the same reason as the colour sheets; that they moved TOWARD
  the direct bake is not claimed, because nothing measured it.
* **`lodgen_defaults.sh` has no rung to run against** (section 2). Reinstating a
  pre-DEFAULTS1 exe, or re-basing that harness on a rung that exists, is a
  director's call.
* **`tests/spells/lodt_write.sh` is referenced in four places and exists in none
  of them** (section 2).

---

## 5. Mistakes

Three, all written into the root `MISTAKES.md` at the TOP the moment they were
recognised, and repeated in `scratchpad/vt1_20260912/MISTAKES_ENTRIES.md`.

1. **A comparison of two files that were not there, printed as "0 bytes
   differ".** The dim-8 generality check compared an assembled sheet with a
   direct one and printed a clean pass for both exes. Both direct dim-8 bakes
   had written no sheets at all -- the line immediately above said `sheets=0`
   and was not read -- so `cmp` was handed a missing file on one side. A check
   that cannot fail on its input is not a check; this one could not even be
   handed an input. Found because the next bake printed `sheets=12`. The rule
   taken from it: a comparison states how many files it compared and refuses at
   zero.

2. **Believing a relative `--tex-dir`.** Four bakes were thrown away before the
   pattern was seen. Now named as a generator defect under Owed.

3. **Two errors in the offline model, caught before the build.** The parent-box
   expression carried a half-finished edit (`- ry0 * 0 -`) that happened to give
   the right answer on the probe set, and the model executed `main()` at import,
   which would have fired silently when the texel predictor imported it. Both
   were found by reading the file back before quoting its numbers.

Nothing in this list reached the exe or the gates. The rule that caught all
three is the same one: read the count, not the verdict.

---

## 6. Skill review

`nifskope-ww-lodgen` (invoked; it earned its place three times) --

* Its named-interpreter trap is what kept this lane honest: every probe here was
  run through `/c/Users/bungo/AppData/Local/Programs/Python/Python39/python` by
  name, so no empty number was ever read as a defect.
* Its compile-the-embedded-Python rule was applied to
  `tests/spells/lodgen_terrain_vt.sh` after every edit (`bash -n` plus three
  PYEOF blocks compiled) and before the suite was ever run.
* Its heredoc trap earned itself again: the first attempt to append section 4 of
  this report through a quoted heredoc died with `unexpected EOF while looking
  for matching quote`. The section was written with the file tool and appended
  in binary instead, which is what the skill already says to do.

**What the skill should gain, from this lane:**

1. **The sheet formats, in one line.** A terrain colour sheet is DXT1, 174,888
   bytes for 512x512 with 8 mips; `_msn` is **DXT5**, 349,680 bytes, 10 mips,
   and vanilla's colour sheet is DXT5 too. A probe that decodes one with the
   other's stride produces four white columns in every eight and a statistic
   that looks like a seam defect. This cost V9c its meaning and would have cost
   this lane a day if the white band had not been checked against vanilla's own
   file.
2. **`--tex-dir` must be absolute.** A relative one writes nothing, exits 0, and
   prints a full census; `--out-dir` relative works, which makes it worse.
3. **Which sheets are vanilla's, copied.** `lodgen.cpp:6680` gives a chunk with
   a shipped vanilla `_msn` that exact file, byte for byte. Any statistic about
   "our" normals must first ask whether the file is ours. In this region, all
   four were Bethesda's.
4. **The ring fill has two callers and one rule.** After this lane,
   `lodgenTerrainFillRing` takes the inner unit as a parameter, the chunk baker
   passes none and gets the old behaviour exactly, and the tile baker passes the
   chunk it will be assembled into. Anyone adding a third caller must decide
   which box it belongs to before writing a line.
5. **A determinism claim needs two writers.** The doc's older "seamless and
   deterministic" paragraph measured 72 file comparisons across three ring
   origins -- all on the same writer, which is why it could not see this. The
   new section 2.5j says so in the doc; the skill should say it too.

`nifskope-ww-build-verify` (used through `tools/ww_build.sh`) -- nothing to add.
It gated on make's own rc, renamed nothing it should not have, and its
exe-newer-than-sources check agreed with the suite's independent preflight.
