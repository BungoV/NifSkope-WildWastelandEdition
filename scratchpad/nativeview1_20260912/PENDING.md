# DISCHARGED 2026-09-12 20:17 — this file is history, not a to-do list

Every step below was carried out on the director's 19:28 resume, with the game
down and each `tasklist` check its own command read before the command it
guarded. The lane is closed: `DONE` contains `native_view`, `BUILDING` is gone,
and the numbers live in `lane_nativeview1_report.md`. One gate is RED and stays
RED: (c), IoU 0.8179 against the briefed 0.95, cause measured, bar not moved.

The text below is kept exactly as it was written at 19:06, as the record of what
was owed at the stop.

---

# Lane NATIVEVIEW1 — PENDING, 2026-09-12 19:06

## Why this file exists

`Fallout4.exe` is UP (pid 55208, first seen 18:52). The brief's header rule and
CONSTITUTION 6: `tasklist | grep -i Fallout4` before every build and every exe
launch, **up = stop, PENDING.md**. Everything left needs either a build or a
window, so the lane stopped launching things and finished everything that needed
neither.

**Before resuming, run this and read its answer before typing anything else:**

```
tasklist | grep -i -E "Fallout4|NifSkope"
```

Game up = stop again. A NifSkope with no `--port` is bungo's own window: rename
the exe aside as `NifSkope_inuse_<pid>.exe` at LINK time, never kill it, and
never rename or delete any `NifSkope_inuse_*.exe` this lane did not create.

---

## DONE so far

1. `src/lodinative.h` / `.cpp` — the `.lodi` + `.lodo` scene builder. NEW.
2. `src/lodtsheets.h` / `.cpp` — the `.lodt` tile unpacker and its session-only
   resource root. NEW.
3. `src/btdterrain.cpp` — seven edits: the sheet open, the outward region snap,
   the forced mesh-tile size, the sheet UV branch (the no-sheets arm is the
   ORIGINAL expression verbatim), the per-tile texture slots, the notes.
4. `src/nifskope.cpp` — six edits: the include, the `.lodi` file type, the
   `WW_LODL_OBJECTS` append, the `.lodi` loadFile branch, the undo-clear suffix
   list, the Save-As guard. Applied by
   `scratchpad/nativeview1_20260912/hookup.py` (anchored, refusing).
5. `NifSkope.pro` — the four new sources.
6. TWO builds, both with the game down and after PANEL1's relink lock cleared.
   The second, **18:46:50, `release/NifSkope.exe` 22,275,584 B, `BUILD-RC=0`**,
   is what is on disk. Rung: `release/NifSkope.before_nativeview1.exe`,
   22,154,240 B, 18:07:58 — **never delete it**.
7. The `.lodi` route works end to end: chunk (-20,24) dim 4 gives 676 placements
   read, 676 drawn, 33 bases, 50 buckets, 50 shapes, 36,866 vertices, census 676
   rows, 148 ms; the render shows real LOD materials, no missing-texture warning.
8. **Gate (b), two legs MEASURED with no window**: viewer census 676 == the
   `.lodi` chunk table's 676 (independent decoder), 0 keys missing, 0 invented,
   worst world-position difference 0.0046 u against a 1 u floor.
9. `tests/spells/native_open.sh` + `tests/spells/native_open_authority.py` —
   written, `bash -n` clean, the two window-free legs exercised. NEVER RUN whole.
10. The LEGACY shim resource root: `scratchpad/nativeview1_20260912/resroot/`
    (45 files, 233 MB).
11. Docs: `## Viewer` appended to `docs/LODGEN_NATIVE_LODO_LODI.md`; the sheets
    note spliced into `docs/LODGEN_BTD_FORMAT.md`'s "Opening one in NifSkope".
12. `MISTAKES.md` — five entries at the top; copy in `MISTAKES_ENTRIES.md`.
13. `WW_CHANGES_ENTRY.md`, `HANDOFF_BLOCK.md`, `CHANGED_FILES.txt`,
    `SKILL_AMENDMENT.md`, `lane_nativeview1_report.md` — all written.

## NOT DONE

The `DONE` marker is NOT written. `BUILDING` is still there. The lane is open.

---

## RESUME, in order. Every path absolute, every step its own command.

### 0. The gate

```
tasklist | grep -i -E "Fallout4|NifSkope"
```
Read the answer. Game up → stop, update this file's date, do nothing else.

### 1. Exercise the `.lodl` sheets route — DO THIS FIRST, it is the biggest risk

It is compiled and linked and **has never been run once**. Everything in item 4
and gate (d) depends on it.

```
cd /e/Projects/NifskopeWildWastelandEdition
WW_WINDOW_AT=1960,40 \
WW_LODL_REGION="-20,24,-17,27,2" \
WW_LODL_SHEETS="E:/Projects/NifskopeWildWastelandEdition/scratchpad/showcase1_20260912/out/look/mod/Terrain" \
WW_LODL_SHEET_CACHE="E:/Projects/NifskopeWildWastelandEdition/scratchpad/nativeview1_20260912/work/sheetcache" \
WW_RENDER_SHOT="E:/Projects/NifskopeWildWastelandEdition/scratchpad/nativeview1_20260912/work/lodl_lit.png" \
WW_RENDER_SIZE=1024x1024 WW_RENDER_VIEW=1 WW_RENDER_CLEAN=1 \
WW_RENDER_CENTER="-73728,106496,0" WW_RENDER_ORTHO=8192 \
timeout 300 release/NifSkope.exe --port 42931 \
 "E:/Projects/NifskopeWildWastelandEdition/scratchpad/showcase1_20260912/out/lodl/Terrain/Commonwealth.lodl" 2>&1 | tail -30
```

**Read the notes, not the picture.** They must say `lit from <container>: level
dim D, NxM tiles over this region, K unpacked`. If they say "no .lodt sheets
(...)" or "no sheet tile of this region unpacked", the route fell back and the
picture is the data view. Then look at `work/sheetcache/Textures/LODLSheets/`:
loose DDS pairs there means the unpack worked and the failure is downstream.

Known unknowns, in the order they are likely to bite:
  * the container's `compression` must be 0 — `src/lodtsheets.cpp` refuses a
    compressed one by name;
  * sheet row 0 is NORTH, the mesh is row-0-SOUTH; a Y mirror shows up as the
    terrain colours flipped top-to-bottom against the `.BTR` render;
  * the region snaps outward to whole sheet tiles, so the built region is
    probably bigger than the four cells asked for — the notes print both.

### 2. Re-bake ONE chunk with identity ON (gate (b)'s manifest leg)

Never the whole Commonwealth, never bungo's `Data/Terrain`, and with the exe
COPY, not `release/NifSkope.exe`. It is `-no-gui`, so it needs no GUI slot — but
it still needs the game DOWN.

```
cd /e/Projects/NifskopeWildWastelandEdition
O="E:/Projects/NifskopeWildWastelandEdition/scratchpad/nativeview1_20260912/gate_bake"
mkdir -p "$O/obj" "$O/tex" "$O/mod" "$O/native"
"E:/Projects/NifskopeWildWastelandEdition/scratchpad/showcase1_20260912/ns_run/NifSkope.exe" \
 -no-gui lodgen \
 "X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm" \
 --worldspace 3C --terrain-region -20 24 -17 27 --dim 4 \
 --out-dir "$O/obj" --data-root "E:/Tools/Fallout 4/DataUnpacked/Data" \
 --vt "$O/mod" --tex-dir "$O/tex" \
 --native "$O/native" --native-mesh-report "$O/native/mesh_report.txt" \
 --land-guide aspecthex --land-guide-scale 256 --land-hex 256 \
 --terrain-object-ao --erosion 1 --erosion-iterations 4 --erosion-seed 7 \
 --msn-cache "E:/Tools/Upscale/esrgan-bat/output" --sheet-format legacy \
 --road-detail 1 --cover --arrays --atlas \
 --no-terrain-identity \
 > "$O/bake.log" 2>&1; echo "exit $?"; tail -5 "$O/bake.log"
ls "$O/obj"/*.manifest.txt
```

That is `scratchpad/showcase1_20260912/bake_look.sh`'s own argv with the region
narrowed to one chunk and **`--no-identity` DROPPED** — `src/lodgen.cpp:3758`
puts the manifest's placement rows inside `if ( opts.identity )`, which is the
whole reason the look bake has no manifest.

**Byte-identity of the bake outputs is a gate** (the brief's Rules): this lane
changed only readers, so a fresh (-20,24) bake from `release/NifSkope.exe` must
`cmp` equal to one from `release/NifSkope.before_nativeview1.exe`. Do that once,
on the `.BTO` and the `.lodi`, and record it.

### 3. Run the harness whole

```
cd /e/Projects/NifskopeWildWastelandEdition
GBAKE="E:/Projects/NifskopeWildWastelandEdition/scratchpad/nativeview1_20260912/gate_bake/obj" \
GNATIVE="E:/Projects/NifskopeWildWastelandEdition/scratchpad/nativeview1_20260912/gate_bake/native" \
bash tests/spells/native_open.sh 2>&1 | tee scratchpad/nativeview1_20260912/work/native_open.log
```

It refuses by itself if the game is up. It runs `lodl_open.sh` inside gate (a);
that is the only other GUI harness it starts, one at a time. Record the IoU and
the MAD as NUMBERS in the report's gate table, and the `upp` line it prints from
`release/ww_camera_pin.log`.

If a gate fails, the fix is the code, not the bar — except `MAD_BAR` (default
24), which was never calibrated because the lit route has never run. Calibrate it
by MEASURING the `.BTR`-against-`.BTR` difference of two renders of the same
chunk first, and say in the report what the bar is made of.

### 4. The pictures — `scratchpad/nativeview1_20260912/images/`

All of them: label burned in, `WW_RENDER_CLEAN=1`, textured roads
(`--road-detail 1` is already in the bake), sizes read back with PIL, and beside
each one the `.BTR`/`.BTO` render from the SAME camera as a control column
labelled LEGACY, which needs
`WW_LODGEN_RESOURCES="E:/Projects/NifskopeWildWastelandEdition/scratchpad/nativeview1_20260912/resroot;E:/Projects/NifskopeWildWastelandEdition/scratchpad/showcase1_20260912/out/look/obj"`.

  i.   terrain + objects together — `.lodl` with `WW_LODL_SHEETS` and
       `WW_LODL_OBJECTS` — top (`WW_RENDER_VIEW=1`) and one oblique (the pin).
  ii.  objects alone — the `.lodi` — two views, the placement count in the
       caption (read it from the notes, not from memory).
  iii. the far region at the coarsest `.lodi` level (`WW_LODI_LEVEL=7`). **Say
       plainly that this bake carries NO impostor cards** — `bake_look.sh`
       passes no `--impostors`.
  iv.  the AO greyscale IS NOT OWED: identity is off in the look arm. Say so.

**Send nothing. The director sends.**

### 5. Finish the build chain (`nifskope-ww-build-verify`)

Only if a further build is needed; otherwise just the three unread legs against
the 18:46:50 exe:

```
cd /e/Projects/NifskopeWildWastelandEdition
git status --porcelain -- src res tools tests | awk '{print $2}' | while read f; do
  [ -f "$f" ] && [ "$f" -nt release/NifSkope.exe ] && echo "NEWER THAN THE EXE: $f"
done; echo "sweep done"
ls -la release/lodinative.o release/lodtsheets.o release/btdterrain.o release/nifskope.o 2>/dev/null
make -n -f Makefile.Release 2>/dev/null | grep -cE '^\s*g\+\+.*-c '
```
The last number must be 0. Read every count next to the exe timestamp.

### 6. Close

  * splice nothing into `WW_CHANGES.md`, `HANDOFF.md` or the skill yourself —
    the director splices; the texts are already written here.
  * update `lane_nativeview1_report.md` §1 (numbers), §2 (the chain's last
    legs), §3 (the picture list with PIL sizes), §4 (what is still owed).
  * `rm scratchpad/nativeview1_20260912/BUILDING`
  * write `scratchpad/nativeview1_20260912/DONE` containing `native_view`
  * final summary in plain language: exe timestamp and bytes, the gate numbers,
    each picture file with its size, and what is owed.

---

## Facts a resume should not have to re-derive

  * `.lodi` chunk occupancy, this bake: (-20,32) **0**, (-16,32) 39, (-12,32) 72,
    (-20,28) 511, (-16,28) 460, (-12,28) 520, **(-20,24) 676**, (-16,24) 693,
    (-12,24) 552, (-20,20) 0, (-16,20) 1, (-12,20) 2. Total 3,526 over 12 chunks,
    10 present.
  * `occluderCount` is **0** in this `.lodi`, so `WW_LODI_BOXES=1` draws nothing
    here. Do not read that as a bug.
  * `out/look/obj/Commonwealth.4.-20.32.BTO` does not exist (only the `.BTR`).
  * `WW_RENDER_SIZE` floors the WIDTH at about 1024 px: 480x480 came back
    1024x445. Ask for >= 1024 and read the frame back with PIL.
  * A `.BTO` wants `data\Textures\Terrain\Commonwealth\Objects\Commonwealth.LodgenObjects.DDS`
    and a `.BTR` wants `Data\Textures\Terrain\Commonwealth\Commonwealth.4.-20.24.DDS`;
    the bake writes both to `out/look/tex/...`, which is why the shim root exists.
  * `src/lodtfile.*` is the `.lodl` reader. The `.lodt` sheet reader is
    `src/io/lodvfile.*` (`LDTX`). The brief's wording points at the wrong one.
  * Anything carrying a backslash goes through the Write/Edit tools. A bash
    heredoc halves them; it happened twice in this lane.
