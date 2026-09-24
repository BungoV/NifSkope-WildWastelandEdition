
## Build (BUILD6)

Built and gated 2026-09-10 03:5x by lane BUILD6; nothing committed. Every
reading below is off `release/NifSkope.exe` **03:38:56** (18,517,504 B). That is
lane BUILD5b's link, not a fresh one: `src/lodgen.cpp` was written at 03:33:44,
`GeneratedFiles/.obj/lodgen.o` is 03:38:54 and holds the
`lodgenTerrainRingSelfTest` symbols (`nm`, 2 hits), and this lane's `make -j2`
(rc 0, game check rc=1) compiled nothing. The staleness sweep over every changed
file under `src/ res/ tools/ tests/ NifSkope.pro` found one file newer than that
exe, `tests/spells/lodgen_native_baseline.sh` (a harness, not a source). Logs:
`scratchpad/build6_20260910/logs/g*.log`, summary `chain1_summary.txt`.

| # | gate | bar (PENDING.md) | reading | verdict |
|---|---|---|---|---|
| 1 | `ringcontrol.sh` | 12 checks, CONTROL says the old order is REFUSED | **13 checks, 0 failures, RESULT PASS**; CONTROL: the old order gives 1096.000 north / 1096.000 east on the inner boundary, REFUSED (the 13th check is "the self-test's own verdict is PASS"; the count is reported, not re-pinned) | PASS |
| 2 | `lodgen_terrain_vt.sh` | 35 checks, 0 failures | **35 checks, 1 failure, RESULT FAIL** | **FAIL** |
| 3 | V9a / V9b | byte-identical on all four chunks, tint ON and OFF, and `_msn`; FLOOR >= 2 of 8 | V9a ON and OFF: byte-identical on all four; FLOOR 8 of 8 pairs differ; **V9b: `msn differs: Commonwealth.4.-20.28`** (the other three identical) | **FAIL on V9b** |
| 4 | V9c | edge step <= 2.60 (1.961 or better); E/W <= 3.20, N/S <= 3.30; interior 1.20..2.20 | E/W seam 4.955 interior 1.803 ratio **2.75** (edge step **1.961**); N/S seam 4.600 interior 1.603 ratio **2.87** (edge step 1.773) -- the same digits BUILD4 read | PASS |
| 5 | `lodgen_terrain.sh` | 26/0, `UP=G D0=76 D1=32 D2=51 D3=32` both paths | **26 checks, 0 failures**; pyramid-assembled and direct both `UP=G D0=76 D1=32 D2=51 D3=32` (vanilla control `D0=99 D1=67 D2=67 D3=67`) | PASS |
| 6 | `lodgen_identity.sh` | 8 ok, RESULT PASS, baseline unmoved | **8 ok, RESULT PASS**; dim 4: 678 rows, 471 SCOL parts; dim 8: 1,536 rows; 406 shared | PASS |
| 7 | edge band vs lane CLAMP's clamped bake | 0 beyond 4 / 4 / 64 on all four borders; colour + `_msn` must move | colour **0 beyond** (4 chunks, cover and nocover), `_msn` **0 beyond** on all four (BUILD4's 994 / 1,405 north misses -> 0); `_data` **16 beyond 64 on -20.28** in both runs (maxd 79); colour moved 127 texels with `--cover`, 0 without (the same 0 BUILD4 read: the no-cover colour floor is pre-existing, not new); `_msn` moved 22,127 | PASS on colour/`_msn`; `_data` 16 = the wetness-domain defect, left as the resume says |

**The V9b failure, measured (not cured).** Reproduced outside the harness into
`scratchpad/build6_20260910/v9b/` with the harness's own two commands (region
-24 24 -17 31, dim 4, `--cover`; run1 adds `--vt ... --vt-height`). The direct
`_msn` of -20.28 is byte-identical to this lane's gate-7 cover bake. The
pyramid-assembled and direct `_msn` sheets of `Commonwealth.4.-20.28` differ by
**32 texels of 262,144 (0.0122%)**: rows 0-3 (the NORTH border, one BC1 block
row) at x = 252..259 (two blocks), i.e. centred on **x = 256, the -19|-18 cell
corner on the region's outer y=31|32 edge**, maxd 3, nothing on S/E/W, nothing
interior. -24.28 and both y=24 chunks: 0 texels between the two paths. Lane
BUILD4 read V9b green at 35/0 on the exe before this rule, so this is the
ownership rule's own regression: at a cell CORNER on the region's outer ring
the chunk baker and `lodgenBakeVtTile` now resolve the corner sample to
different owners (both call the one filler, but the tile's ring cells and the
chunk's ring cells are not the same set at the region edge). Candidates, named
as candidates for the director: (a) the corner sample of the north ring row
where two ring cells meet -- the rule says "a ring cell never writes the inner
unit's boundary row", and at a corner the inner unit's boundary is one sample
wide while the ring row is two cells wide; (b) the tile path's inner unit being
the tile, not the chunk, so its "own boundary" is a different line at x=256.
Neither was tested; `borders.py` beside the sheets is the instrument.

**The written prediction (PENDING.md §4), checked.** Against BUILD4's own after
sheets: the y=24 chunks came back **byte-identical, all 12 files, cover and
no-cover** (`cmp`). The y=28 `_msn` sheets moved 3,199 (-20.28) and 2,519
(-24.28) texels, **every one in rows 0-7 of the north border** (maxd 7; the
15-28 texels the edge-band classifier files under W/E are corner texels of
those same rows; interior 0; the east COLUMN beyond the corner rows: 0). The
prediction said "north or east"; the reading is north only, which is narrower,
not wider. The `_data` sheets of the y=28 chunks moved 3,278 / 3,800, maxd 47,
0 beyond 64 -- so the 16 beyond-64 texels on -20.28 are NOT this rule's (they
are identical between BUILD4's bake and this one).

Mtimes in one table: `src/lodgen.cpp` 03:33:44; `lodgen.o` 03:38:54; exe
03:38:56; gates run 03:52:17-03:53:23; v9b reproduction 03:5x. Skipped: none
of the seven. `WW_CHANGES.md` paragraph replaced (CR 19,020 unchanged, LF
+25); `scratchpad/clamp2_20260910/DONE` written 03:57 with the verdict.
Owed: the V9b corner (the director's call: revert the ownership rule, or scope
it, or accept a 32-texel corner difference and re-pin V9b -- this lane did not
re-pin); the `_data` 16 (lane CLAMP's owed item 2, unchanged); the mesh path's
missing ring (owed item 3, unchanged). The exe this was gated on has since been
replaced by BUILD6's 03:57:46 link (the native hook-up); the terrain code in
it is the same bytes of `lodgen.cpp` §terrain, and the CLAMP2 gates were NOT
re-run on it.
