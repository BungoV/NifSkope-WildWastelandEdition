# UINOTES1b's owed chains, run by lane ROADS4 -- 2026-09-12 06:10:16 to 06:22:33

Lane UINOTES1b ended BUILD PENDING with both chains owed on its own exe. Lane
ROADS4's brief made them item -1. They were run on that exe, unmodified, the
moment `Fallout4.exe` went down, and BEFORE lane ROADS4 built anything.

* exe under test: `release/NifSkope.exe` **2026-09-12 05:48:33, 21,817,856 B**,
  md5 `980e64c1aa4e5478b5833d83ebea9655` (the same bytes are now kept as
  `release/NifSkope.before_roads4.exe`, copied 06:23:28, md5 identical).
* `Fallout4.exe` count 0 and `NifSkope.exe` count 0 before every harness; both
  scripts check both names separately and skip rather than proceed.
* chain logs: `scratchpad/roads4_20260912/item_minus1_chains.txt`; per-harness
  output under `scratchpad/uinotes1_20260912/logs/lodgen/f4_*.txt` and
  `.../logs/ui/`.

**Any differing row below is REPORTED, not fixed** -- lane ROADS4 does not own
this code and the brief forbids touching it.

## The lodgen chain (`lodgen_chain.sh`), 06:10:16 - 06:20:35

| harness | expected (ROADS3's F4) | measured on the 05:48:33 exe | rc | secs | row |
|---|---|---|---|---|---|
| `lodgen_roads` | 11 / 0 | **11 / 0** | 0 | 24 | same |
| `lodgen_terrain` | 26 / 0 | **26 / 0** | 0 | 13 | same |
| `lodgen_terrain_vt` | 41 / 1 (V9c known red) | **41 / 2** | 1 | 46 | **DIFFERS, +1** |
| `lodgen_ground_cover` | 29 / 5 | **29 / 6** | 1 | 64 | **DIFFERS, +1** |
| `lodgen_terrain_pbrm` | 14 / 0 | **14 / 0** | 0 | 230 | same |
| `lodgen_native` | green | **18 / 0, RESULT PASS** | 0 | 178 | same |
| `lodl_open` | 23 / 0 | **23 / 2** | 1 | 52 | **DIFFERS, +2** |
| `lod_generation` | 116 / 0 | **116 / 0** | 0 | 6 | same |

### The three differing rows, named

**`lodgen_terrain_vt` +1 and `lodgen_ground_cover` +1 are LANE ROADS4'S OWN
DOING, and they are not a defect in the exe.** The extra red line in each is

```
  FAIL the exe is newer than every source this answer depends on
  FAIL C0 the exe is newer than every source this answer depends on
```

Lane ROADS4 edited `src/lodgen.h` and `src/nifcli.cpp` at 06:07-06:08 (item 0,
the `--road-detail` default flip) while `Fallout4.exe` was still up and no build
was allowed. Those two files are therefore newer than the 05:48:33 exe, and the
staleness check in both harnesses reads that correctly. The other 40 and 28
checks are unaffected: `V9c the direct sheets are continuous ACROSS a chunk
seam` is the known red lane UINOTES1b inherited, and `ground_cover`'s five are
its known five. Nothing about the exe moved. The lesson is a process one and is
written up in lane ROADS4's `MISTAKES_ENTRIES.md`: **an inherited chain is run
before the lane's own source edits land on disk, or its staleness checks read
the lane instead of the exe.**

**`lodl_open` +2 is REAL and it is a CRASH.** Every `.lodl` plane render on this
exe segfaults:

```
tests/spells/lodl_open.sh: line 271: 200043 Segmentation fault   WW_LODL_REGION=... WW_LODL_PLANE=... WW_RENDER_SHOT=... WW_RENDER_FLAT=1 WW_RENDER_VIEW=1 WW_RENDER_SIZE=360x360 timeout 120 "$NS" --port "$PORT" "$LODL"
  ...  plane watertype did not render
  ...  plane cellflags did not render
  ...  plane cellrange did not render
  ...  plane overview did not render
  ...  0 planes rendered, 0 distinct pictures, blank-by-their-own-account: none
  FAIL all 0 planes render, 0 distinct pictures, and no two differ only by accident
tests/spells/lodl_open.sh: line 320: 200170 Segmentation fault   WW_RENDER_SHOT=... WW_RENDER_VIEW=1 WW_RENDER_SIZE=900x900 timeout 900 "$NS" --port "$PORT" "$LODL"
  FAIL the whole worldspace renders non-blank
```

Six separate invocations, six segmentation faults, five different plane names
and the whole-worldspace shot. The 21 non-render checks in the same harness all
pass, so the `.lodl` reader is fine and it is the **headless render path
(`WW_RENDER_VIEW=1`) that dies**. This is not the `numpy` symptom
(`nifskope-ww-lodgen` records that one as EMPTY numbers under a
`ModuleNotFoundError`); the numbers here are a hard `0` and the shell reports
the signal. It was green at 23/0 when ROADS3 ran the same harness on the
2026-09-12 04:10:38 exe, so the crash arrived with one of the nine UI rulings
in the 05:48:33 build.

Lane ROADS4 does not touch `src/nifskope_ui.cpp`, `ui/*` or the render path --
that is lane UINOTES1b's territory by its own brief -- so this is handed back,
not fixed.

## The UI chain (`ui_chain.sh after`), 06:20:35 - 06:22:33

| harness | expected | measured | rc | row |
|---|---|---|---|---|
| `animws` | (not in the brief's list) | **210 / 1**, 1 skip | 1 | reported |
| `hkxanim_ui` | 48 / 1 | **48 / 1** | 1 | same |
| `ui_align` | 15 / 0 | **15 / 0** | 0 | same |
| `water_ui` | 84 / 0 | **84 / 0** | 0 | same |
| `files_tab` | 29 / 1 | **29 / 1** | 1 | same |
| `top_bar` | 43 / 5 | **43 / 5** | 1 | same |
| `skeleton_overlay` | 5 / 1 | **5 / 1** | 1 | same |

Every row the brief named matches its expected count exactly. `skeleton_overlay`'s
single red is its known one, quoted from the log:

```
FAIL: the four renders are not the same size: [(941, 1524, 3), (941, 1524, 3), (965, 1024, 3), (965, 1024, 3)]
```

`0 NifSkope process(es)` left running after each chain.
