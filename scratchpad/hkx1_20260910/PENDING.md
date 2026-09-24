# PENDING -- lane HKX1 (FO4 .hkx animation reader), 2026-09-10

Everything below is what the lane could not run because the brief forbids
launching or relinking `release/NifSkope.exe` while bungo bakes. The reader
itself, its gates and the standalone driver ARE built and green; only the
integration build of NifSkope with `src/hkxanim.cpp` in the .pro is owed.

## 1. Build (nifskope-ww-build-verify; qmake BEFORE make: two NEW files in the .pro)

```bash
tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?      # rc=1 or stop
cd /e/Projects/NifskopeWildWastelandEdition
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'export PATH="$PATH:/e/Tools/GIT/cmd"; cd /e/Projects/NifskopeWildWastelandEdition && qmake NifSkope.pro > scratchpad/hkx1_20260910/qmake.log 2>&1; echo QMAKE-RC=$?; make -j2 > scratchpad/hkx1_20260910/build.log 2>&1; rc=$?; grep -E "error:|Error [0-9]" scratchpad/hkx1_20260910/build.log | head -20; echo BUILD-RC=$rc; ls -l --time-style=+%H:%M:%S release/NifSkope.exe release/style.qss; exit $rc'
grep -n "hkxanim" Makefile.Release | cut -c1-120        # the object and its dependency block must exist
test release/NifSkope.exe -nt src/hkxanim.cpp && echo "exe newer than hkxanim.cpp"
```

Expected: `QMAKE-RC=0`, `BUILD-RC=0`, `GeneratedFiles/.obj/hkxanim.o` listed,
no `error:` lines. Nothing in NifSkope calls the reader yet (HKX2/HKX3 do), so
the link adds the object and changes no behaviour; the syntax pass already
passed with the real flags (`scratchpad/hkx1_20260910/build_dump.sh`,
`SYNTAX-RC=0`).

## 2. The standalone driver and the gates (need NO NifSkope.exe; rerun after the build to prove nothing moved)

```bash
MSYSTEM=UCRT64 CHERE_INVOKING=1 /c/msys64/usr/bin/bash -lc 'cd /e/Projects/NifskopeWildWastelandEdition && bash scratchpad/hkx1_20260910/build_dump.sh'
python tests/spells/hkxanim_gates.py      | tail -3   # expect: 134 checks, 3 failures (the three pre-registered fixture claims: (b) name set, (b) Weapon translation 1.48e-3, (c) T-pose != bind pose)
python tests/spells/hkxanim_synthetic.py  | tail -2   # expect: 29 checks, 0 failures  PASS
python tests/spells/hkxanim_mutate.py     | tail -2   # expect: 20 checks, 0 failures  PASS
```

The fixtures live in `scratchpad/hkx1_20260910/clips/` (extracted from
`X:\Programs\Steam\steamapps\common\Fallout 4\Data\Fallout4 - Animations.ba2`
with `tools/ba2get.py`, unpacked with HKXPACK; `.claude/skills/ww-hkx-animation`
has the commands if they need regenerating).

## 3. The four documents

`WW_CHANGES.md` -- splice `scratchpad/hkx1_20260910/WW_CHANGES_ENTRY.md`
(never `sed -i`; the file is mixed CRLF/LF, 19,020 CR); `MISTAKES.md` --
five entries already written by the lane; `scratchpad/lane_hkx1_report.md` --
append a `## Build (resume)` section with the exe mtime and the three gate
tallies; `HANDOFF.md` top block -- HKX1 landed, HKX2 next.
