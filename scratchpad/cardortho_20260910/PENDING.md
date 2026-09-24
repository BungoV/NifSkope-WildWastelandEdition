# LANE CARDORTHO — BUILD PENDING (2026-09-10)

**Why.** The brief's build rule: lanes WATER2 and CLAMP build before this one.
Checked ONCE, after all code was written:

```
$ ls -l scratchpad/clamp_20260910/DONE
ls: cannot access 'scratchpad/clamp_20260910/DONE': No such file or directory
$ tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?
rc=1
```

The game is down; **lane CLAMP has not finished**, and it is alive in
`src/lodgen.cpp` — the same file this lane changed, in a different function.
Building now would link CLAMP's half-written terrain bake. Not polled.

**What is on disk and compile-checked, and what has never run.** Everything
below is written; not one number in the report comes from a running exe.

---

## The paste-able resume

Run this after WATER2 and CLAMP have built, with the game down. One NifSkope
instance at a time — nothing here may run while another lane's harness is alive.

```bash
cd /e/Projects/NifskopeWildWastelandEdition

# 0. the gate, again, because the build rule is per-build and not per-lane
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?          # must print rc=1

# 1. qmake BEFORE make. This lane added no #include and no header member, so
#    strictly make alone would do -- but lane HOOKCAM's glview.h members and
#    lane WATER2's lodtfile.h are in the same working set, and a stale
#    dependency list is invisible. qmake first, per nifskope-ww-resume-pending.
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && qmake NifSkope.pro > scratchpad/cardortho_20260910/qmake.log 2>&1; echo QMAKE-RC=$?; make -j2 > scratchpad/cardortho_20260910/build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" scratchpad/cardortho_20260910/build.log | head -20; echo BUILD-RC=$rc; exit $rc'

# 2. the exe is newer than EVERY changed file, not the one you edited
EXE=release/NifSkope.exe
for f in $(git status --porcelain -- src res tools tests | awk '{print $NF}'); do
  [ -f "$f" ] || continue
  [ "$EXE" -nt "$f" ] || echo "STALE vs $f"
done
cmp release/style.qss res/style.qss && echo "stylesheet in step"

# 3. the gates, the re-bake, the transition measurement, the sample set --
#    one script, in the order they have to run, each step printing what it wrote
bash scratchpad/cardortho_20260910/run.sh 2>&1 | tee scratchpad/cardortho_20260910/run.log
```

Then OPEN every PNG the last step lists
(`scratchpad/cardortho_20260910/cardortho_transition_*.png`) before reporting
them — CONSTITUTION 5.

---

## What each step must produce, pre-registered

| step | what it must show | the floor beside it |
|---|---|---|
| `lodgen_octahedral.sh` bake 1 | the sidecar says `projection ortho`; `orthofit`'s third field is 0 and its first two agree within 0.1% | bake 4's perspective control says `projection persp` on the same line |
| `lodgen_octahedral.sh` bake 4 | all 64 frames of the 512-unit cube within **2 texels** of the pre-registered span (`scratchpad/cardortho_20260910/prereg_cube.md`), central asymmetry ≤ 0.05, near-edge/far-edge width difference ≤ 0.05 | the `WW_IMPOSTOR_PERSP=1` control must EXCEED every one of those three |
| the cube's own size | 256 units ± 3, measured through `WW_RENDER_ORTHO` — a different code path from the bake | the fixture is written by `-no-gui new --cube --size 512` in the same step |
| `lodgen_card_arrays.sh` | layer `id1` carries `projection: "ortho"` | layer `id2`, whose sidecar names no camera, carries **no key at all** |
| `lodgen_impostor_cards.sh`, `lodgen_identity.sh` | unchanged (12 ok / 8 ok) | they are the floor: the crossed card and byte-identity must not have moved |
| the 19-tree re-bake | 19 baked, 0 failed, 19 of 19 sidecars saying `projection ortho`, 0 `orthofit` lines with a perspective third field | — |
| `transition.py` | every card row within one card texel on centre and 2% on both extents, over 3 trees × 2 axis views × 2 distances | the ZEROED-OFFSET control rows must be **0 of 12** passing |
| the sample set | regenerated on the new library; every card `.lodm` and every `cardArray` layer reporting `projection ortho` | a `(absent)` count above zero means a stale card leaked into the set |

## What this lane did NOT do, and must not be reported as done

* Nothing was built, so **the 19-tree library on disk is still CARDFINAL's
  perspective bake**, and so is the FO4CS sample set.
* The gap, mip and per-frame gates from CARDFINAL are unchanged in code and are
  expected to stay green; that expectation is not a measurement.
* The transition table in `scratchpad/lane_cardortho_report.md` is empty. It is
  filled from `transition.log`, not predicted.
