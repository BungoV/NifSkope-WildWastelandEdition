# LANE TERRAINFIX -- BUILD PENDING, 2026-09-09 16:45

**Why.** The brief's build gate is checked ONCE and it failed on its first
condition: `scratchpad/images_20260909/DONE` does not exist, so the picture lane
still holds `release/NifSkope.exe` (and `tasklist` shows `NifSkope.exe` pid
29964 running). `Fallout4.exe` was DOWN (`rc=1`), so the game is not the
blocker. Nothing was built and nothing under `release/` was touched.

**What IS done.** All code, both harnesses and all four documents are written
and on disk. `src/lodtfile.cpp` and `src/lodgen.cpp` both pass a syntax +
semantics pass (`g++ -fsyntax-only`, RC=0 each, only the pre-existing warnings:
`lodgenTriangulateGrid`'s unused `grid`, the `"\shack"` escape at 3087,
`sampleF` at 5477, the atlas `%d`/qsizetype at 7205). That proves they COMPILE;
it proves nothing about linking or behaviour.

## Paste-able resume

```bash
# 1. the exe must be free: no NifSkope window, Fallout4 down
cd /e/Projects/NifskopeWildWastelandEdition && tasklist | grep -i -E "NifSkope|Fallout4"

# 2. the gated build chain (nifskope-ww-build-verify)
cd /e/Projects/NifskopeWildWastelandEdition \
 && LOCKED=$(powershell -NoProfile -Command "Get-Process NifSkope -ErrorAction SilentlyContinue | Where-Object { \$_.Path -eq 'E:\Projects\NifskopeWildWastelandEdition\release\NifSkope.exe' } | ForEach-Object { \$_.Id }" | tr -d '\r') \
 && if [ -n "$LOCKED" ]; then mv release/NifSkope.exe "release/NifSkope_inuse_${LOCKED}.exe" && echo "running copy (pid $LOCKED) renamed aside"; else echo "exe not held by a window"; fi \
 && MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && make -j2 > /tmp/ww_build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" /tmp/ww_build.log | head -20; echo BUILD-RC=$rc; ls -l --time-style=+%H:%M:%S release/NifSkope.exe; exit $rc' \
 && test release/NifSkope.exe -nt src/lodgen.cpp && test release/NifSkope.exe -nt src/lodtfile.cpp \
 && echo "exe newer than both changed sources"

# 3. the two gates this lane's changes reach, in this order
timeout 900 bash tests/spells/lodt_write.sh      2>&1 | tail -40   # water v2 + landless
timeout 900 bash tests/spells/lodgen_terrain.sh  2>&1 | tail -30   # the VT _msn, rung 4

# 4. the four-worldspace diff, offline, on FRESHLY written files
#    (it is what found the landless bug; run it on new bakes, not the deployed ones)
for w in "3C Commonwealth Fallout4.esm" "B0F DLC03FarHarbor DLCCoast.esm" \
         "290F NukaWorld DLCNukaWorld.esm" "F94 DiamondCity Fallout4.esm" \
         "52931 NukaWorldAmphitheater DLCNukaWorld.esm"; do
  set -- $w
  D="X:/Programs/Steam/steamapps/common/Fallout 4/Data/$3"
  release/NifSkope.exe -no-gui lodgen "$D" --worldspace $1 --lodt /tmp/tf/$2 >/dev/null 2>&1
  release/NifSkope.exe -no-gui lodgen "$D" --worldspace $1 --heightmap /tmp/tf/$2 >/dev/null 2>&1
  python scratchpad/terrainfix_20260909/lodt_vs_heightmap.py \
     /tmp/tf/$2/Terrain/$2.lodt "$(ls /tmp/tf/$2/Textures/Terrain/$2/*.dds | head -1)" | head -8
done
# EXPECTED: 0 differing texels on all five. Before the fix: 0, 62, 0, 167936, 97.
```

## What the build must confirm, and what it cannot

* Both gates are NEW checks that were run against the OLD state first:
  `lodt_write.sh`'s landless case reads **97 differing texels and exits 1** on
  the shipped `NukaWorldAmphitheater.lodt` (proved today, offline). The VT
  `_msn` checks could not be run red without a build -- they are red by
  construction (nearest sampling gives left-difference classes 1..3 = 0, the
  gate wants > 20), and the simulation in `vt_msn_sim.py` shows the same
  numbers on the same tiles offline.
* **Commonwealth byte identity is now a version-1 question.** The v2 header is
  8 bytes longer, so the Commonwealth `.lodt` is NOT byte-identical to the
  2026-09-05 file any more, by design. The exact-at-the-off-value proof is in
  `lodt_write.sh`: `WW_LODT_VERSION=1` must reproduce a file whose every
  section offset is the v2 one minus 8 and whose bytes past the header are
  identical. If the director wants the old file back on disk:
  `WW_LODT_VERSION=1 release/NifSkope.exe -no-gui lodgen ... --lodt <dir>`.
* **FO4CS refuses version 2 today** (`kVersion = 1u` in
  `src/FarField/FarFieldLodtFormat.h`, lane LODT1 wave 71). Deploying a fresh
  `.lodt` to his mod folder without `WW_LODT_VERSION=1` will make FO4CS reject
  the file. That is bungo's call, not this lane's.
