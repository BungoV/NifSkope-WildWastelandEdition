# PENDING — lane HKX4b (glTF 2.0 export), 2026-09-10

Read `scratchpad/lane_hkx4_report.md` first, then this. Skill
`nifskope-ww-resume-pending`.

## What is already done, and needs nothing

The writer and everything that gates it are FINISHED and MEASURED through the
standalone `release/gltfexport_dump.exe` — no NifSkope.exe was needed and none
was built or launched:

```
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc \
  'cd /e/Projects/NifskopeWildWastelandEdition && bash tests/spells/gltf_gates.sh'
```
last run 2026-09-10 13:55 — **0 gates not as registered**: G2 0/0/0 failures on
three exports, G2f 12 of 12 mutants refused, G3 2 / 2 / 0 (the two are the
pre-registered `LLeg_Toe1` pair of the assembled fixture; the vanilla donor is
the clean control), G3f all 7 sabotages red, G4 Blender 4.5 imports all three
with 9 / 9, 9 / 9 and 1 / 1 meshes matched by name.

Syntax with the real `Makefile.Release` flags, `SYNTAX-RC=0` on every one:
`src/gltfexport.cpp`, `src/gltfexportnif.cpp`, `src/lib/importex/gltfanim.cpp`,
`tests/gltfexport_dump.cpp`, and the CLI function that lives inside
`hookup.py` (extracted into `scratchpad/hkx4_20260910/probe_cli.cpp` and
compiled against the real headers — so the hook-up's biggest insert is not
untested text).

## Ordering against lane HKX5b — read this before P1

While this lane ran, HKX5b added `src/gltfimport.{h,cpp}` and
`src/hkxwrite.{h,cpp}` to `NifSkope.pro` **directly**, and `src/gltfimport.h`
already includes `src/gltfexport.h` and hard-codes the up-axis quaternion this
writer emits (`const Q4 EXPORT_UP = Q4{ -S, 0.0, 0.0, S };`,
`src/gltfimport.cpp:688`). **`src/gltfexport.cpp` is NOT in `NifSkope.pro`**
— only `gltfimport` is — so a build made before P1 below will compile
`gltfimport.o` and fail to link it.

So: **apply this hook-up BEFORE the next full build**, whichever lane triggers
it. The three anchors it uses in `NifSkope.pro` still match exactly once after
HKX5b's additions (re-checked at 14:0x, the file having grown 18,779 → 18,859
bytes under this lane).

## What is PENDING, in order

**P1. Apply the hook-up.** `python scratchpad/hkx4_20260910/hookup.py --check`
reports **11 of 11 anchors match exactly once, 0 insertions already present**
(NifSkope.pro 18,779 B / CR 0, src/lib/importex/importex.cpp 7,044 B / CR 0,
src/nifcli.cpp 263,848 B / CR 0). Run it with `--apply`. It refuses if any
anchor has moved or if the `lane HKX4` marker is already in a file, so a
double-apply is impossible.

**P2. qmake BEFORE make.** Three new sources join the build
(`src/gltfexport.cpp`, `src/gltfexportnif.cpp`,
`src/lib/importex/gltfanim.cpp`) and `src/nifcli.cpp` gains an `#include` of a
new header. qmake freezes the dependency lists when the Makefile is generated,
so `make` alone would link a fresh object against a stale one.

```bash
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && qmake NifSkope.pro > scratchpad/hkx4_20260910/qmake.log 2>&1; echo QMAKE-RC=$?; make -j2 > scratchpad/hkx4_20260910/build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" scratchpad/hkx4_20260910/build.log | head -20; echo BUILD-RC=$rc; exit $rc'
```

**P3. Read the dependencies back, per object, by name** (never `grep -A3` —
a dependency ten continuation lines down is missed):

```bash
grep -n "gltfexport\.h\|gltfexportnif\.h" Makefile.Release | cut -c1-160
```
and for each hit walk back to its `GeneratedFiles/.obj/<name>.o:` line. The
objects that must exist and must name the new headers: `gltfexport.o`,
`gltfexportnif.o`, `gltfanim.o`, `nifcli.o`.

**P4. The exe is newer than EVERY changed file**, not just the one you edited —
other lanes are alive in this tree:

```bash
EXE=release/NifSkope.exe
for f in $(git status --porcelain -- src res tools tests | awk '{print $NF}'); do
  [ -f "$f" ] || continue; [ "$EXE" -nt "$f" ] || echo "STALE vs $f"; done
```

**P5. The CLI, on the built exe.** This is the first time any of this code runs
inside NifSkope, and it must reproduce the standalone driver's numbers exactly:

```bash
release/NifSkope.exe -no-gui gltf fixtures/human_male_vanilla.nif \
    -o scratchpad/hkx4_20260910/out/cli_jog.gltf \
    --clip scratchpad/hkx1_20260910/clips/jog.hkx \
    --bones scratchpad/hkx1_20260910/clips/skeleton.hkx
```
Expect `139 nodes, 9 shapes (9 skinned), 6443 vertices, 11375 triangles` and
`78 of 95 tracks matched`. Then run the gates on it:

```bash
python tests/spells/gltf_check.py    scratchpad/hkx4_20260910/out/cli_jog.gltf     # 0 failures
python tests/spells/gltf_readback.py scratchpad/hkx4_20260910/out/cli_jog.gltf \
    --nif fixtures/human_male_vanilla.nif --clip scratchpad/hkx1_20260910/clips/jog.hkx \
    --bones scratchpad/hkx1_20260910/clips/skeleton.hkx                            # 2 failures (LLeg_Toe1)
```
**If the CLI's numbers differ from the driver's, the model path and the
container path disagree and that is the finding** — measure which field, do
not patch around it. The three readers (C++ driver, `NifModel`, Python) exist
precisely so a disagreement names itself.

**P6. The menu, by hand, once.** `File > Export > .glTF (skeleton, skin,
animation)` on the fixture. With no clip loaded it must say, in its own
message box, that **no animation was written** — that sentence is the whole
reason the entry is safe to ship before lane HKX3 registers the clip provider.

**P7. Then, and only then, update the four documents** that currently say the
build is pending: `WW_CHANGES.md` (splice
`scratchpad/hkx4_20260910/WW_CHANGES_ENTRY.md`, and assert the CR count is
unchanged — the file is MIXED), the **Status** paragraph of
`docs/GLTF_INTERCHANGE.md`, `scratchpad/lane_hkx4_report.md`, and the HANDOFF
top block.

## Not this lane's, and not to be invented by the resume

* `gltfExportSetClipProvider()` is HKX3's call — see `CHANGE_NEEDED.md`
  beside this file. Until it is made, the menu entry exports the character
  without an animation and says so; the CLI is unaffected.
* `src/hkxanim.cpp` belongs to HKX2b. This lane read it and changed nothing.
* The `.dds` textures are not written and are not meant to be; embedding them
  is `src/lib/importex/gltf.cpp`'s job.


---

## RESOLVED by lane BUILD8, 2026-09-10

P1 applied (11 of 11 anchors, CR 0 unchanged on all three files). P2 `qmake` +
`make -j2` both RC=0; `release/NifSkope.exe` 14:37:53, 19,382,272 bytes. P3 the
dependency block of `gltfexport.o`, `gltfexportnif.o`, `gltfanim.o` and
`nifcli.o` all name `gltfexport.h`. P4 the exe is newer than all 72 changed
files. P5 the CLI reproduced the driver EXACTLY -- 139 nodes, 9 shapes
(9 skinned), 6443 vertices, 11375 triangles, 78 of 95 tracks -- with
`gltf_check.py` 6411 / 0 and `gltf_readback.py` 3234 / 2 (the pre-registered
`LLeg_Toe1` pair). P7 done: `WW_CHANGES.md`, the Status paragraph of
`docs/GLTF_INTERCHANGE.md`, `scratchpad/lane_hkx4_report.md`.

**P6 IS NOT DONE.** The menu entry `File > Export > .glTF (skeleton, skin,
animation)` was never opened by hand: the lane kept `release/NifSkope.exe` free
of a GUI session so bungo could take it. It is the one item of this resume still
owed, and it is one click plus reading the message box.

Everything else this lane's resume asked for is measured in the
`## Build (BUILD8)` section of `scratchpad/lane_hkx4_report.md`.
