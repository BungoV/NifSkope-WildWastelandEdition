# lane RENAME — BUILD PENDING

The brief's build gate was checked ONCE, at the end of the lane, and it failed
its second condition:

```
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?
rc=1                                   <- PASS: no game, no NifSkope
ls scratchpad/offscreen_20260909/DONE
(does not exist; that folder holds PENDING.md instead)
```

Lane OFFSCREEN ended BUILD PENDING itself (`Fallout4.exe` was up while it ran),
so `src/nifskope_ui.cpp` carries its finished but **unbuilt** +123 lines, and
`release/NifSkope.exe` (17:22:05) is older than every source either lane
touched. Nothing was built, nothing under `release/` was touched, and
**bungo's installed `.lodt` files were NOT renamed** — that step needs the new
exe to verify each file after the move, and moving them without a verify would
leave five files whose name and content nobody had checked together.

**Everything else in the brief is finished and on disk.** All six changed
sources pass `g++ -fsyntax-only`, RC=0 each (the command is in
`scratchpad/rename_20260909/syntax.txt`).

---

## What is on disk

| file | state |
|---|---|
| `src/lodtfile.h` | +20 lines; `LODL_MAGIC` promoted to the header |
| `src/lodtfile.cpp` | +24 lines; writes `.lodl`, refuses `LDTX` by name, `WW_LODL_VERSION` |
| `src/io/lodvfile.h` | +23 lines; `LODTEX_MAGIC` (`LDTX`) and the retired-`LODV` constant |
| `src/io/lodvfile.cpp` | +20 lines; writes/validates the new magic, two named refusals |
| `src/btdterrain.h` / `.cpp` | prose + `WW_LODL_REGION` / `WW_LODL_PLANE`, old names refused aloud |
| `src/nifcli.cpp` | command `lodl`, flags `--lodl` / `--lodt-check`; three retired spellings refuse by name; `--candidates trees` is tree-only |
| `src/lodgen.cpp` | VT container path `.lodt`; the three hex comments corrected |
| `src/lodgenmanager.cpp` | the panel's visible strings only |
| `tools/bake_impostor_cards.sh` | the `CANDIDATES=trees` comment |
| `tests/spells/lodl_write.sh` | renamed from `lodt_write.sh`, +105 lines (the refusal gate) |
| `tests/spells/lodl_open.sh`, `lodl_btd.sh`, `lodl_open_authority.py` | renamed and updated |
| `tests/spells/lodgen_terrain_vt.sh`, `lodgen_vt_check.py` | `.lodt` + `LDTX` + the real-file refusal check |
| `docs/LODGEN_*.md` | renamed family throughout; `LODGEN_NATIVE_LODG_LODI.md` → `LODGEN_NATIVE_LODO_LODI.md` |
| `scratchpad/rename_20260909/GUI_CHANGE_NEEDED.md` | the six lines another lane owns |

**`git mv` was used for all five renames**, so the rename shows as a rename.

---

## The resume, paste-able

Run lane OFFSCREEN's resume FIRST if it has not run — its build is this one's
build, and one build serves both lanes.

```bash
cd /e/Projects/NifskopeWildWastelandEdition

# 0. the gate
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?        # want rc=1

# 1. QMAKE FIRST, then build. This is owed from BUILD1, which patched
#    Makefile.Release by hand for the lodtfile.h dependency, and it is owed
#    again now: src/lodtfile.cpp gained an include of io/lodvfile.h and
#    src/io/lodvfile.cpp gained one of lodtfile.h. qmake's dependency lists are
#    frozen at generation time, so a NEW include is invisible to make until
#    qmake reruns ("A successful build is not a consistent one", the
#    nifskope-ww-build-verify skill).
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && qmake NifSkope.pro > /tmp/ww_qmake.log 2>&1; echo QMAKE-RC=$?; make -j2 > /tmp/ww_build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" /tmp/ww_build.log | head -20; echo BUILD-RC=$rc; ls -l --time-style=+%H:%M:%S release/NifSkope.exe release/style.qss; exit $rc'

# 2. the dependency really survived the qmake re-run (this is the check the
#    hand-patched Makefile was standing in for)
grep -c "lodtfile\.h" Makefile.Release                 # want >= 2
grep -n "lodvfile\.h" Makefile.Release | head          # lodtfile.o must list it
grep -n "^release/lodtfile\.o:" -A3 Makefile.Release | grep -c "lodvfile.h"   # want 1
grep -n "^release/lodvfile\.o:" -A3 Makefile.Release | grep -c "lodtfile.h"   # want 1

# 3. the exe is newer than everything this lane changed, and the sheet is in step
for f in src/lodtfile.cpp src/lodtfile.h src/io/lodvfile.cpp src/io/lodvfile.h \
         src/btdterrain.cpp src/btdterrain.h src/nifcli.cpp src/lodgen.cpp \
         src/lodgenmanager.cpp; do
  test release/NifSkope.exe -nt "$f" && echo "newer than $f" || echo "STALE vs $f"
done
cmp res/style.qss release/style.qss && echo "sheet in step"

# 4. THE GATES, in the order the brief pre-registered them
bash tests/spells/lodl_write.sh   2>&1 | tail -60   # byte identity + BOTH refusal directions
LODL="$PWD/scratchpad/rename_20260909/Commonwealth.lodl" \
  bash tests/spells/lodl_open.sh  2>&1 | tail -30   # want 23 checks, 0 failures
bash tests/spells/lodgen_terrain.sh   2>&1 | tail -20
bash tests/spells/lodgen_identity.sh  2>&1 | tail -10
bash tests/spells/lodgen_terrain_vt.sh 2>&1 | tail -30   # the sheets as .lodt

# 4b. the four-worldspace verify, 0 differing, under the NEW name
#     (bake into a scratch dir, then --verify-only in place)
for WS in "X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm 3C Commonwealth" \
          "X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm F94 DiamondCity" \
          "X:/Programs/Steam/steamapps/common/Fallout 4/Data/DLCCoast.esm B0F DLC03FarHarbor" \
          "X:/Programs/Steam/steamapps/common/Fallout 4/Data/DLCNukaWorld.esm 290F NukaWorld"; do
  set -- $WS   # careful: the esm path has a space; run these four by hand
done

# 5. bungo's installed set: rename .lodt -> .lodl, keep the .bak-20260909 as they are
T="E:/Projects/Fallout 4 Mods/mods/FO4CS/Terrain"
ls -l "$T"
for n in Commonwealth DLC03FarHarbor DiamondCity NukaWorld NukaWorldAmphitheater; do
  mv "$T/$n.lodt" "$T/$n.lodl"
done
# verify each ONE AT A TIME with the new command; --info must print, not refuse
for n in Commonwealth DLC03FarHarbor DiamondCity NukaWorld NukaWorldAmphitheater; do
  echo "== $n"; release/NifSkope.exe -no-gui lodl "$T/$n.lodl" --info 2>&1 | head -4
done
ls -l "$T"
```

### The five installed files, as they stand now (17:27, version 2)

| file | bytes | becomes |
|---|---|---|
| `Commonwealth.lodt` | 35,953,294 | `Commonwealth.lodl` |
| `DLC03FarHarbor.lodt` | 9,195,933 | `DLC03FarHarbor.lodl` |
| `DiamondCity.lodt` | 53,148 | `DiamondCity.lodl` |
| `NukaWorld.lodt` | 7,182,356 | `NukaWorld.lodl` |
| `NukaWorldAmphitheater.lodt` | 38,303 | `NukaWorldAmphitheater.lodl` |

The five `*.lodt.bak-20260909` copies (2026-09-05, version 1) **stay exactly as
they are**, name included: they are the way back to the pre-version-2 set and
renaming them would make the ladder unreadable.

---

## What would refute this lane

* `lodl_write.sh`'s first Python block failing `magic is still LODT after the
  .lodl rename`: the rename touched a byte, which it must not have.
* Either refusal check passing for the wrong reason — the harness requires the
  refusal TEXT to name the other format, not merely a non-zero exit, and it runs
  a control on each side (a real `.lodl` must still open, a real `.lodt`
  container must still validate).
* `tests/spells/lod_generation.sh` failing exactly one check, *"with one, the
  panel says what it will write"*: **expected**, and it is
  `scratchpad/rename_20260909/GUI_CHANGE_NEEDED.md` §2 — the panel summary now
  says `.lodl` and the self-test in `src/nifskope_ui.cpp` still looks for
  `.lodt`. Any OTHER failure in that harness is this lane's.
* A build that succeeds without the qmake re-run: `make` had no reason to
  rebuild `lodtfile.o` against a header it does not know is included, and the
  link would then mix a `LodtFile::open` that does not know `LDTX` with a
  `lodvValidate` that does. Step 2 is what catches it.
