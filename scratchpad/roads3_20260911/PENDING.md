# ROADS3 -- SUPERSEDED: the build WAS spent, this file is history

**Read this first.** `Fallout4.exe` had exited by **2026-09-12 04:08:58**, the
build was spent after a fresh `tasklist` check, and `tools/ww_build.sh` returned
BUILD-RC=0 on the first try -- one build, zero extra relinks. New
`release/NifSkope.exe` **2026-09-12 04:10:38, 21,489,152 B**, md5
`fe65cc978f3896881140c2eea57c69c6`. Gates F2, F3a-F3h and F4 are all measured
on it and are in `scratchpad/lane_roads3_report.md` sections 3 and 4, and both
pictures are re-taken from real bakes. **Nothing below is owed.** It is kept
only as the record of what the lane handed over while it was pending, and as the
worked example the `nifskope-ww-resume-pending` skill asks for.

---

# ROADS3 -- BUILD PENDING

**Why:** `Fallout4.exe` PID 22908 was up at **2026-09-12 03:57:20** (`tasklist`,
8,305,996 K working set). The standing rule is that the game being up ends the
lane BUILD PENDING, so no compiler was run, no exe was linked, no NifSkope was
launched and no harness was run. The source edit is complete and reviewed by
eye; **it has never been compiled.**

Everything that did not need the compiler is finished and is in
`scratchpad/lane_roads3_report.md` sections 0, 1, 2, 4, 5, 6, 7 and in
`images/cmp_road_wash.png` + `images/cmp_road_profile.png`.

## State on disk

| thing | state |
|---|---|
| `release/NifSkope.exe` | GRADE1's, **untouched**, 2026-09-12 03:06:21, 21,489,152 B |
| `release/NifSkope.before_roads3.exe` | the rung, a byte copy of the above, md5 `6af74b4b4667ce50c4506a2d42a04fdf` |
| `src/lodgen.h`, `src/lodgen.cpp`, `src/nifcli.cpp` | **edited, uncompiled** |
| backups of those three before the edit | `scratchpad/roads3_20260911/lodgen.h.bak`, `lodgen.cpp.bak`, `nifcli.cpp.bak` |
| the edit as a re-runnable script | `scratchpad/roads3_20260911/patch_road_opacity.py` then `patch_usage_synopsis.py` |
| bakes on the rung | `scratchpad/roads3_20260911/out/{rung_roads,rung_noroads,rung_detail1,rung_legacy}/{t2020,t0808}` |
| nothing committed, nothing stashed | confirmed; the tree is shared |

## Resume, in order

**0. The game.** `tasklist | grep -i Fallout4` first, every time. Up = stop
here again. Also check no NifSkope is running that is not yours, and remember
lane UINOTES1 may have its own `--port` instance out of
`E:\Projects\NifskopeWWE_ui` -- wait for it, never kill it.

**1. Build.** MSYS2 UCRT64, the tree's usual `make`/`qmake` route
(`.claude/skills/nifskope-ww-build-verify` has it). Rename the exe aside at link
time, never kill a window. Count the relinks and say the number. Expect the
first compile to be the first time this code has ever seen a compiler: the
likely complaints are the float-equality comparison in
`coverOpts.roadOpacity == 1.0f` (the same pattern `g_landGrade != 1.0f` already
uses, so it should be accepted) and nothing else.

**2. Verify the exe** is newer than all three changed sources, print its
timestamp and size, and confirm the rung is still the launch bytes.

**3. Gate F2, the byte identity.** Both tiles, `cmp` EVERY file:

```
# the off value must reproduce the rung exactly
scratchpad/roads3_20260911/r3_bake.sh new_default      # no new flags at all
scratchpad/roads3_20260911/r3_bake.sh new_opacity1     # --road-opacity 1
# and the compare must be shown able to FAIL
scratchpad/roads3_20260911/r3_bake.sh new_opacity0326  # --road-opacity 0.326
```

* `new_default` vs the rung's `rung_roads`: must be 9 of 9 identical on both
  tiles, colour, `_msn`, mask, `.lodl`, all of them.
* `new_opacity1` vs `rung_roads`: must be 9 of 9 identical -- naming the flag at
  its default must change nothing, which is what proves the default IS 1.0.
* `new_opacity0326` vs `rung_roads`: the COLOUR sheet must differ and the `_msn`
  and `.lodl` must NOT. That is the compare shown able to fail.
* `--roads-legacy` vs the rung's `rung_legacy`: must still be identical, i.e.
  the new switch did not move the way back.

**4. Gate F3, on the real bakes.** Re-run `r3_sim.py`'s gate reader against the
BAKED `new_opacity0326` sheet instead of the simulated one and show the numbers
agree with section 1.7 to about half a level. If they do not, the simulation was
wrong and the report says so.

**5. Gate F4, the harness chain** at GRADE1's baselines, second monitor via
`tests/spells/_harness.sh`, one GUI instance, your own unused `--port`:

| harness | baseline |
|---|---|
| `lodgen_roads.sh` | 11/0 |
| `lodgen_terrain.sh` | 26/0 |
| `lodgen_terrain_vt.sh` | 41/1 |
| `lodgen_ground_cover.sh` | 29/5 |
| `lodgen_terrain_pbrm.sh` | 14/0 |
| `lodgen_native.sh` | 18/0 |
| `lodl_open.sh` | 23/0 |
| `lod_generation.sh` | 116/0 |

plus the S1 seam number (not worse than 4.242) and the raised-highway clearance
(+0.001). At the shipped default the bytes do not move, so all of these should
be unchanged -- show it, do not assert it.

**6. Re-take the two pictures** from real bakes: `python r3_pics.py` after
pointing its candidate columns at the baked variants, so the word SIMULATED
comes off them. Say in the report which panels changed and by how much.

**7. Finish the report:** replace the PENDING rows in section 3.2 with the
measured ones, add the `## DONE` section with the new exe's timestamp, size and
the full gate table, and write `scratchpad/roads3_20260911/DONE`.

## What must NOT be re-done

Sections 0, 1, 4 (as simulations), 5, 6, 7 of the report, the two skills, and
the four rung bakes. The measurement work is finished; only the build-side gates
are owed.
