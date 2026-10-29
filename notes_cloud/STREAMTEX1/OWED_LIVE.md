# STREAMTEX1: owed live (main session, Windows + game data + GPU)

Nothing here was built or run against the game: the cloud lane has no GPU, no Fallout 4 data, no Windows.
Done offline: `bash tests/spells/stream_tex_unit.sh` (PASS; three reds FAIL), and a syntax check of every
changed source against Qt 6.4 (see SKILL_ww-cloud-syntax-check.md).

## 0. Build

Build as usual (nifskope-ww-build-verify). New files: `src/cellstreamtex.h`, `src/gl/streamtexpool.h` (both in
NifSkope.pro HEADERS; header-only policy, the hooks are in src/cellstream.cpp). Re-run qmake: the .pro changed.
Optional: `python tools/wwswitch_survey.py --inc src/wwswitchstages.inc` (the lane added only its two rows by hand).

## 1. A/B first: is it the textures? (old path, the pool off)

Each run writes its own folder; same Concord walk as FRAMESPEED2 (20 crossings, far LOD on).

    OUT=$PWD/scratchpad/streamtex1/ab_keep0 PHASE="walk check" \
      WW_CELL_STREAMTEX_RED=off WW_CELL_STREAM_TEXKEEP=0 bash tests/spells/cell_stream.sh
    OUT=$PWD/scratchpad/streamtex1/ab_default PHASE="walk check" \
      WW_CELL_STREAMTEX_RED=off bash tests/spells/cell_stream.sh

Read from each walk.stream.txt / the check: (d) private step 20 / step 0, `texEvicted`, `texReloaded`,
`texKeepEvicted`, and the `stream tex:` stderr lines in walk.notes (one per crossing; with RED off they show the old
path's churn). Rows:

| row | what | expected |
|-----|------|----------|
| A1 | ab_keep0 (d) ratio | the keep-all figure; if it is ~1.275 too, textures are not what the timer saves |
| A2 | ab_default (d) ratio | FRAMESPEED2's ~1.275 (the current default) |
| A3 | ab_default sum of texKeepEvicted | > 0 = the timer frees loaded cells' textures (the churn this lane removes) |
| A4 | ab_default texReloaded | the churn's size: textures freed and loaded again within 2 crossings |

If A1 is about A2 and A3/A4 are small, the 1.275 is not the texture rule: say so and stop at row 2 anyway.

## 2. The walk with the pool (default 256 MB)

    OUT=$PWD/scratchpad/streamtex1/pool PHASE="walk reds check" bash tests/spells/cell_stream.sh

| row | bar |
|-----|-----|
| P1 | (d) private after crossing 20 <= 1.05 x the fresh open (step 0) |
| P2 | (a) cpuMaxMs < 8 ms on every crossing (the checker's on-processor frame row) |
| P3 | (e) PASS: texKeepFreed 0 over the walk; no crossing reloads past the 2 crossings' release minus the budget |
| P4 | (a) loaded set == 5x5 and (c) no hole / no double still PASS (the pool must not change what is drawn) |
| P5 | red nounload still FAILS (d) |
| P6 | pool row: max texKeptMb <= texBudgetMb + one crossing's release (the pool is trimmed every 2 s) |

Also run the picture phase once (`PHASE="pictures check"`): (b) streamed == fixed must still pass (nothing drawn
is freed: the last 3 paints' textures are never evicted).

## 3. If P1 fails

Re-run with `WW_CELL_STREAMTEX_MB=0` (keep nothing past its last cell) to bound what the pool can do; if that still
fails (d), the growth is elsewhere (driver pools, heaps: the dump's heapBusy/heapHeld/regions split it).
