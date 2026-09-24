# PENDING -- lane CLAMP, the one-cell ring for the direct terrain chunk bake

**Everything is on disk and nothing has been compiled.** The build slot was held
when the code landed: `scratchpad/water2_20260909/DONE` did not exist and a
`NifSkope.exe` (pid 4120) was in the process table. Per the brief, lane CLAMP
did not poll and did not build.

Read first: `scratchpad/lane_clamp_report.md` (section 0 carries the
PRE-REGISTERED bands -- they were written from the code before any bake existed
and they are not to be adjusted to fit what comes back), then this file.

Files changed, all uncommitted:

| file | what |
|---|---|
| `src/lodgen.cpp` | the ring, and four shared helpers instead of five copies |
| `tests/spells/lodgen_terrain_vt.sh` | V9a tinted half -> `cmp`, V9b, the FLOOR, V9c |
| `docs/LODGEN_TERRAIN_VT.md` | §2.4 prose + the provenance footer re-anchored |
| `MISTAKES.md`, `WW_CHANGES.md` | two entries; the changelog says NOT BUILT |
| `scratchpad/clamp_20260910/` | patch scripts, `edgeband.py`, the before-sheets |

No new `#include` was added, so **qmake is not required** -- plain `make` sees
every dependency it needs. Run it anyway if another pending lane in the same
build added one.

---

## 1. Preflight, once

```bash
cd /e/Projects/NifskopeWildWastelandEdition
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?      # must print rc=1
ls scratchpad/water2_20260909/DONE                          # must exist
```

`rc=0` on the first line, or a missing DONE, means BUILD PENDING again -- stop
and re-file this note.

## 2. Build, gated on make's own exit code

```bash
bash tools/ww_build.sh src/lodgen.cpp
```

or, if that wrapper is not usable from the session:

```bash
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && make -j2 > scratchpad/clamp_20260910/build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" scratchpad/clamp_20260910/build.log | head -20; echo BUILD-RC=$rc; exit $rc'
```

Then the staleness sweep over EVERY changed source, not just `lodgen.cpp`:

```bash
EXE=release/NifSkope.exe
for f in $(git status --porcelain -- src res tools tests | awk '{print $NF}'); do
  [ -f "$f" ] || continue
  [ "$EXE" -nt "$f" ] || echo "STALE vs $f"
done
```

**Things the compiler is most likely to object to, so they are named:** the two
new function templates `lodgenTerrainGridSample` and `lodgenTerrainFillRing`
sit immediately after `LODGEN_MSN_FLAT` (~`src/lodgen.cpp:5059`) and are used
from both `lodgenBakeTerrainTextures` and, inside the anonymous namespace,
`lodgenBakeVtTile`; the chunk baker's ring-fill lambda returns
`const EsmLand *` into either `cells[...]` or a local `ringLand`; and the dead
`sampleF` lambda was removed while `tSky` is still passed to
`lodgenTerrainChannels`.

## 3. Bake the AFTER sheets -- ABSOLUTE paths only

A relative `--tex-dir` resolves against the EXE's folder, not the working
directory (MISTAKES.md, twice now).

```bash
R=/e/Projects/NifskopeWildWastelandEdition
W=$R/scratchpad/clamp_20260910
ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"
DATA="E:/Tools/Fallout 4/DataUnpacked/Data"
mkdir -p $W/after/cover/obj $W/after/cover/tex $W/after/nocover/obj $W/after/nocover/tex
"$R/release/NifSkope.exe" -no-gui lodgen "$ESM" --worldspace 3C \
  --terrain-region -24 24 -17 31 --dim 4 \
  --out-dir "$W/after/cover/obj" --data-root "$DATA" \
  --tex-dir "$W/after/cover/tex" --cover > $W/after/cover.log 2>&1; echo rc=$?
"$R/release/NifSkope.exe" -no-gui lodgen "$ESM" --worldspace 3C \
  --terrain-region -24 24 -17 31 --dim 4 \
  --out-dir "$W/after/nocover/obj" --data-root "$DATA" \
  --tex-dir "$W/after/nocover/tex" > $W/after/nocover.log 2>&1; echo rc=$?
find $W/after -name '*.DDS' | wc -l          # must be 24
```

## 4. The re-baseline measurement -- THE number this lane owes

```bash
python $W/edgeband.py $W/before/cover/tex   $W/after/cover/tex   | tee $W/edgeband_cover.txt
python $W/edgeband.py $W/before/nocover/tex $W/after/nocover/tex | tee $W/edgeband_nocover.txt
```

PRE-REGISTERED, and these are the bars, not wishes:

| sheet | band | beyond the band | why that band |
|---|---|---|---|
| colour | <= 4 texels | **0** | the normal's +-1 grid step, 128 units, 32 units a texel |
| `_msn` | <= 4 texels | **0** | the same |
| `_data` | <= 64 texels | **0** | the AO march's own `dist <= 2048.0f` |

FLOOR: the colour and `_msn` totals must be NON-ZERO on the cover run, or the
ring is inert and every band passed on nothing. `edgeband.py` fires that itself.

**Second control, on the cover-free pair:** with the tint off the colour sheet
never reads the normal, so its four sheets must be **byte-identical before and
after** (`edgeband.py` prints `0` with a `-` for max distance) while `_msn` and
`_data` still move inside their bands. A colour sheet that moved with the tint
off would mean the ring reached the paint composite, which it must not.

Record, for `WW_CHANGES.md`: the differing count and the max distance per sheet,
and the twelve `sha256[:16]` values `edgeband.py` prints at the end -- that list
IS the new baseline, and per CONSTITUTION it is written beside the numbers that
justify it, never alone.

## 5. The harnesses, sequentially, one instance at a time

```bash
mkdir -p $W/logs
run () { local l="$1"; shift; local t="$1"; shift
  echo "### $l start $(date +%H:%M:%S)"
  timeout "$t" "$@" > "$W/logs/$l.log" 2>&1
  echo "### $l rc=$?  $(date +%H:%M:%S)"
  grep -E "checks, [0-9]+ failures|^RESULT|^PASS|^FAIL" "$W/logs/$l.log" | tail -4; echo; }
run terrain_vt 3600 bash tests/spells/lodgen_terrain_vt.sh
run terrain    1800 bash tests/spells/lodgen_terrain.sh
run identity    900 bash tests/spells/lodgen_identity.sh
```

Expected, each with the reason it is expected:

* **`lodgen_terrain_vt.sh` RESULT PASS.** V9a's cover-free half was already
  green; its tinted half is now a `cmp` and V9b pins the `_msn`; the FLOOR must
  report `8 of 8 sheet pairs differ`; V9c must print an E/W ratio near **2.75**
  with an interior control near **1.80**, and N/S near **2.87** / **1.60** --
  those are the RINGED readings lane VTFIX measured on 2026-09-09, and the
  direct sheets should now reproduce them because they are the same bytes.
  The check count rises from 32 to 35 (V9a-2 stayed one check, V9b, the FLOOR
  and V9c are new).
* **`lodgen_terrain.sh` PASS**, and its pyramid statistics UNCHANGED:
  `UP=G D0=76 D1=32 D2=51 D3=32` on both the assembled and the direct sheet,
  with vanilla's own `99/67/67/67` as the control. The VT path was not touched;
  the direct sheet now equals the assembled one, and those two already read the
  same four numbers before this change.
* **`lodgen_identity.sh` RESULT PASS and its baseline UNMOVED.** It bakes
  `--objects ... --no-ao` and compares `.bto` files and manifests; no terrain
  sheet is written on that path and the Land vertex channels were not touched.
  If it moves, something reached the mesh path and the change is wrong.

If a gate fails: measure the cause, say it with the number, and **do not land a
fix** -- report it (`nifskope-ww-resume-pending` §6).

## 6. Close it out

1. `WW_CHANGES.md` -- replace the **STATUS: BUILD PENDING** block with the exe
   timestamp, each harness with its counts, the per-sheet band numbers and the
   twelve hashes.
2. `scratchpad/lane_clamp_report.md` -- APPEND a `## Build` section (do not
   rewrite the lane's text): the gate table, every mtime in ONE table, what was
   skipped and why.
3. `MISTAKES.md` -- anything found while building, the moment it is found.
4. `scratchpad/clamp_20260910/before/` is the ONLY picture of the clamped
   behaviour there will ever be; keep it. `scratchpad/clamp_20260910/ns_before/`
   is a 17 MB copy of the old `release/` and may be deleted once the gates pass.
5. Tell bungo his open NifSkope window needs a restart (CONSTITUTION 6).
6. Nothing is committed without his word (CONSTITUTION 8).
