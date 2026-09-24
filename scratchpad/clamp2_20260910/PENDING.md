# LANE CLAMP2 -- BUILD PENDING resume

Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, **commit
nothing**. Read `CONSTITUTION.md`, then `scratchpad/lane_clamp2_report.md`,
then this file. Skills: `nifskope-ww-resume-pending`, `nifskope-ww-build-verify`,
`nifskope-ww-lodgen`, `ww-sheet-diff`, `ww-control-calibration`.

The code, the control, the document and the ledger entry are on disk and the
translation unit compiles (`g++ -fsyntax-only` on `src/lodgen.cpp`, rc 0, no new
warning). **Nothing has been linked or run.**

## 1. What changed

| file | what |
|---|---|
| `src/lodgen.cpp` | `lodgenTerrainFillRing` -- a ring cell no longer writes the inner unit's own boundary row/column; `lodgenTerrainRingSelfTest` + `lodgenTerrainRingSelfTestOnce` (new, env-gated), called once from `lodgenBakeTerrainTextures` |
| `docs/LODGEN_TERRAIN_VT.md` | §2.4 gains the ownership rule; the five `lodgen.cpp` footer rows re-derived from their anchors (+257 each) and the stamp re-taken |
| `tests/spells/lodgen_terrain_vt.sh` | a comment only -- the check count stays **35** |
| `scratchpad/clamp2_20260910/ringcontrol.sh` | the known-answer control |
| `WW_CHANGES.md` | the entry, marked NOT BUILT |

## 2. Build

```
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && make -j2 > /tmp/b.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" /tmp/b.log | head; echo BUILD-RC=$rc'
```

`Fallout4.exe` and `NifSkope.exe` must both be absent first
(`tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` -> `rc=1`). No new
`#include` was added by this lane, so a fresh `qmake` is not owed by it -- but
run `qmake NifSkope.pro` first anyway if any other lane landed a new include
since 2026-09-10 01:01. Then the staleness sweep over **every** changed file
under `src/ res/ tools/ tests/`, not just `lodgen.cpp`, and read the exe's
timestamp beside every verdict below.

## 3. The gates, pre-registered here BEFORE the build

Run them in this order; the first two are the cheap refuters.

| # | gate | command | bar |
|---|---|---|---|
| 1 | the known-answer control | `bash scratchpad/clamp2_20260910/ringcontrol.sh` | `RESULT PASS`, 12 checks, and the CONTROL line must say the old order is **REFUSED** |
| 2 | `lodgen_terrain_vt.sh` | `bash tests/spells/lodgen_terrain_vt.sh` | **35 checks, 0 failures, RESULT PASS** -- the count must not have moved |
| 3 | V9a/V9b inside it | (same run) | byte-identical on all four chunks, tint ON and OFF, and `_msn`; FLOOR ≥ 2 of 8 pairs differ |
| 4 | V9c inside it | (same run) | edge step **1.961 or better**, bar 2.60; E/W ≤ 3.20, N/S ≤ 3.30; interior control 1.20..2.20 |
| 5 | `lodgen_terrain.sh` | `bash tests/spells/lodgen_terrain.sh` | **26 checks, 0 failures**; pyramid stats `UP=G D0=76 D1=32 D2=51 D3=32` on both paths |
| 6 | `lodgen_identity.sh` | `bash tests/spells/lodgen_identity.sh` | **8 ok, RESULT PASS**, baseline unmoved (it must not move: the mesh path was not touched) |
| 7 | the edge band | see §4 | colour / `_msn` / `_data` **0 beyond 4 / 4 / 64 on all four borders**, and colour + `_msn` must still MOVE |

Gate 6 is also the empirical half of the lane's answer about the land VERTEX
channels: the mesh path was not changed, so if its baseline moves, something in
the shared terrain section reached further than this lane believes.

## 4. The edge band, exactly which two directories

`edgeband.py` answers "how far from a chunk border did the RING move a texel",
so its **before** is the CLAMPED bake, the same one lane CLAMP kept, and its
**after** is a fresh bake off the new exe. Do not use BUILD4's `after/` as the
before -- that is the ringed-but-unowned bake and the diff would then answer a
different question (it is still worth printing as a second run; see below).

```
mkdir -p scratchpad/clamp2_20260910/after/cover/tex scratchpad/clamp2_20260910/after/nocover/tex
# ABSOLUTE paths only: a relative --tex-dir resolves against the EXE's folder
release/NifSkope.exe -no-gui lodgen "X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm" \
  --worldspace 3C --terrain-region -24 24 -17 31 --dim 4 \
  --out-dir "E:/Projects/NifskopeWildWastelandEdition/scratchpad/clamp2_20260910/after/cover/obj" \
  --tex-dir "E:/Projects/NifskopeWildWastelandEdition/scratchpad/clamp2_20260910/after/cover/tex" \
  --data-root "E:/Tools/Fallout 4/DataUnpacked/Data" --cover
# and the same without --cover into after/nocover/tex

python scratchpad/clamp_20260910/edgeband.py \
  scratchpad/clamp_20260910/before/cover/tex scratchpad/clamp2_20260910/after/cover/tex \
  > scratchpad/clamp2_20260910/edgeband_cover.txt
python scratchpad/clamp_20260910/edgeband.py \
  scratchpad/clamp_20260910/before/nocover/tex scratchpad/clamp2_20260910/after/nocover/tex \
  > scratchpad/clamp2_20260910/edgeband_nocover.txt
```

(Check the exact leaf names under `scratchpad/clamp_20260910/before/` before
running -- lane CLAMP's mistake 2 put one set a level deeper than intended.)

**The second run, which is the direct measurement of THIS lane's change**:
`edgeband.py scratchpad/clamp_20260910/after/cover/tex <this lane's after>`.
It isolates the ownership rule from the ring, and its prediction is written down
here so it is a prediction: **only the north border row and the east border
column of the height grid moved**, so the differing texels must sit within 7
texels of the NORTH or EAST border and nowhere else -- and on the y=24 fixture
chunks, whose y=23|24 and east seams agree by 0 in the master, the sheets should
be **byte-identical**. If the y=24 chunks move, the change reached further than
the master's disagreement and something else is wrong.

Expected on the primary run, against BUILD4's table: the `_msn` misses (994 and
1,405 texels beyond 4, all NORTH, at distances 4-7) go to **0**, and the
`_data` outlier (16 texels beyond 64 on `-20.28`, one BC block on the E border)
should go with them -- `chgt` copies the ring's middle, whose north row moved.
If those 16 survive, they are the wetness-domain defect and not this rule; say
so and leave them, do not widen the band.

## 5. Then, and only with numbers beside them

1. `WW_CHANGES.md` -- replace the **NOT BUILT AND NOT GATED YET** paragraph with
   the gate table's readings. A re-pin without a number is forbidden.
2. `scratchpad/lane_clamp2_report.md` -- append a `## Build` section; do not
   rewrite the lane's own text.
3. `HANDOFF.md` -- the CLAMP2 line in the top block.
4. `MISTAKES.md` -- anything recognised on the way, unprompted.
5. Tell bungo his open NifSkope window needs a restart if the exe relinked.

## 6. If a gate fails

`nifskope-ww-resume-pending` §6: measure the failure and report a verdict; do
not land a cure. In particular do NOT adjust a band to fit what came back --
lane CLAMP's section 0 pre-registered 4/4/64 off the code and BUILD4 reported a
miss rather than moving the bar, and that is the standard here.
