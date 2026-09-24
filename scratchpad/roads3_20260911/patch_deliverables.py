"""Bring WW_CHANGES_ENTRY.md and HANDOFF_BLOCK.md up to the built exe.

Both were written while the lane was BUILD PENDING. The build was then spent
(once, first try, zero extra relinks), so every sentence that says otherwise is
replaced with what was measured. Written as a file: prose apostrophes, and
heredocs arrive CRLF.
"""
import io
import sys

D = r'E:\Projects\NifskopeWildWastelandEdition\scratchpad\roads3_20260911'


def load(n):
    return io.open(D + '\\' + n, encoding='utf-8', newline='').read()


def save(n, s):
    io.open(D + '\\' + n, 'w', encoding='utf-8', newline='').write(s)
    d = io.open(D + '\\' + n, 'rb').read()
    print('%-24s %6d bytes  CRLF %d' % (n, len(d), d.count(b'\r\n')))


def sub(s, old, new):
    if s.count(old) != 1:
        sys.exit('REFUSED: %d matches for %r' % (s.count(old), old[:70]))
    return s.replace(old, new)


# =============================================== WW_CHANGES_ENTRY.md
w = load('WW_CHANGES_ENTRY.md')
if 'It was then built' in w:
    sys.exit('REFUSED: WW_CHANGES_ENTRY.md already updated')

w = sub(w, u"""**What ships: `--road-opacity A`, default 1.** It scales how strongly the road
paint is mixed into the ground under it \u2014 `colour = ground + (roadColour \u2212
ground) \u00d7 coverage \u00d7 A` \u2014 and at the default of 1 the multiply is branched over
entirely, so the bake is the previous bake's bytes by construction.""",
    u"""**What ships: `--road-opacity A`, default 1.** It scales how strongly the road
paint is mixed into the ground under it \u2014 `colour = ground + (roadColour \u2212
ground) \u00d7 coverage \u00d7 A` \u2014 and at the default of 1 the multiply is branched over
entirely, so the bake is the previous bake's bytes. That last part is not an
argument about the code: every file of both bakes was compared on both chunks,
and the new exe with no flag, and with `--road-opacity 1`, and with
`--roads-legacy`, reproduces the previous exe's output **9 of 9 and 10 of 10
files identical in all three arms**. The same run shows the compare can fail:
`--road-opacity 0.326` moves 3 files on one chunk and 4 on the other, and every
one of them is a colour sheet \u2014 the normal sheet, the wetness sheet, the objects
and the mesh files are byte-identical at every setting.""")

w = sub(w, u"""**The default did NOT move, and that is a refusal with arithmetic, not
caution.** Every candidate was simulated on the already-baked sheets, using the
generator's own composite, before any code existed.""",
    u"""**The default did NOT move, and that is a refusal with arithmetic, not
caution.** Every candidate was priced on the already-baked sheets, using the
generator's own composite, before any code existed \u2014 and the three that mattered
were then baked for real and read the same way.""")

w = sub(w, u"""Pictures: `scratchpad/roads3_20260911/images/cmp_road_wash.png` (both chunks,
vanilla | shipped | two candidate opacities, every number burned in) and
`cmp_road_profile.png` (the cross-road luminance profile with the per-texel
opacity the wash would need on the same axis).""",
    u"""**It was then built and gated.** One build, first try, no extra relinks; the
new `release/NifSkope.exe` is 2026-09-12 04:10:38, 21,489,152 B. The eight
harnesses come back at the previous lane's counts row for row \u2014 `lodgen_roads`
11/0, `lodgen_terrain` 26/0, `lodgen_terrain_vt` 41/1, `lodgen_ground_cover`
29/5, `lodgen_terrain_pbrm` 14/0, `lodgen_native` no failures in any of its
seven sections, `lodl_open` 23/0, `lod_generation` 116/0. The single red row in
`lodgen_terrain_vt` was made to answer for itself by re-running that harness on
the OLD exe: it fails there with digit-for-digit identical numbers, so it is not
this change's. Baked, `--road-opacity 0.83` puts the Sanctuary road within
**0.09 of a level** of vanilla's brightness and `0.326` puts it within **0.28**
of vanilla's rise over the ground \u2014 and neither does both, which is the 22-level
gap above.

**One thing this change cannot do, with the number.** The two-tone band is the
road mesh's own ramped skirt showing through. Correlating road luminance with
the mesh's interpolated vertex alpha: vanilla reads **+0.001**, ours reads
**\u22120.792** at the default, **\u22120.694** at 0.83 and **\u22120.436** at 0.326 \u2014 while our
own unpainted ground on the same texels reads **\u22120.325**. So turning the opacity
down walks the skirt signature toward the ground's own floor and can never reach
vanilla's zero: the ramp is still in the geometry underneath. Removing it is a
mesh change, not a colour one, and it is not attempted here.

Pictures: `scratchpad/roads3_20260911/images/cmp_road_wash.png` (both chunks,
vanilla | shipped | two candidate opacities, all four columns real bakes, every
number burned in) and `cmp_road_profile.png` (the cross-road luminance profile
with the per-texel opacity the wash would need on the same axis).""")
save('WW_CHANGES_ENTRY.md', w)

# =============================================== HANDOFF_BLOCK.md
h = load('HANDOFF_BLOCK.md')
if 'BUILT AND GATED' in h:
    sys.exit('REFUSED: HANDOFF_BLOCK.md already updated')

h = sub(h, u"""- ROADS3 MEASURED AND WRITTEN, **BUILD PENDING \u2014 THE GAME WAS UP**.
  `Fallout4.exe` PID 22908 was running at **2026-09-12 03:57:20** (8,305,996 K),
  so under the standing rule no compiler was run, no exe was linked, no NifSkope
  was launched and no harness was run. `release/NifSkope.exe` is still GRADE1's,
  **untouched**: 2026-09-12 03:06:21, 21,489,152 B. Rung
  `release/NifSkope.before_roads3.exe` is a byte copy of it, md5
  `6af74b4b4667ce50c4506a2d42a04fdf`. Markers:
  `scratchpad/roads3_20260911/DONE` in, `BUILDING` gone, and
  `scratchpad/roads3_20260911/PENDING.md` carries the resume steps in order with
  the exact commands. Report `scratchpad/lane_roads3_report.md` (sections 0-7).
  Entry text `scratchpad/roads3_20260911/WW_CHANGES_ENTRY.md`. **Two MISTAKES
  entries NOT appended by the lane** \u2014 `scratchpad/roads3_20260911/MISTAKES_ENTRIES.md`,
  the director splices. Contract amended in place:
  `docs/LODGEN_TERRAIN_VT.md` section 1a (new 1a.5d, plus 1a.4, 1a.6, 1a.7 and
  1a.8), LF-only. **`src/lodgen.h`, `src/lodgen.cpp` and `src/nifcli.cpp` are
  edited and have NEVER been compiled** \u2014 backups beside the lane as
  `lodgen.h.bak`, `lodgen.cpp.bak`, `nifcli.cpp.bak`, and the edit is re-runnable
  as `patch_road_opacity.py` + `patch_usage_synopsis.py`. Nothing committed,
  nothing stashed. **bungo's open NifSkope window does NOT need a restart** \u2014
  no new exe exists.""",
    u"""- ROADS3 **BUILT AND GATED**. `Fallout4.exe` was up at 03:57:20 (PID 22908) so
  the lane was written BUILD PENDING; it had exited by **04:08:58**, the build
  was spent after a fresh `tasklist`, and `tools/ww_build.sh` returned
  **BUILD-RC=0 on the first try \u2014 one build, zero extra relinks**. New
  `release/NifSkope.exe` **2026-09-12 04:10:38, 21,489,152 B**, md5
  `fe65cc978f3896881140c2eea57c69c6`. Rung kept aside as
  `release/NifSkope.before_roads3.exe`, md5
  `6af74b4b4667ce50c4506a2d42a04fdf`, **equal to the exe this lane found at
  launch**; the two are the same size and different bytes. The old exe was
  renamed aside at link time, never killed; no window of bungo's was touched.
  Markers: `scratchpad/roads3_20260911/DONE` in, `BUILDING` gone;
  `PENDING.md` is superseded and says so at its top. Report
  `scratchpad/lane_roads3_report.md` (sections 0-7 and `## DONE`). Entry text
  `scratchpad/roads3_20260911/WW_CHANGES_ENTRY.md`. **Four MISTAKES entries NOT
  appended by the lane** \u2014 `scratchpad/roads3_20260911/MISTAKES_ENTRIES.md`, the
  director splices. Contract amended in place: `docs/LODGEN_TERRAIN_VT.md`
  section 1a (new 1a.5d with its build results, plus 1a.4, 1a.6, 1a.7, 1a.8),
  LF-only, byte-counted. `src/lodgen.h`, `src/lodgen.cpp` and `src/nifcli.cpp`
  changed; backups beside the lane (`*.bak`) and the edit re-runnable as
  `patch_road_opacity.py` + `patch_usage_synopsis.py`. Nothing committed,
  nothing stashed. **bungo's open NifSkope window DOES need a restart** \u2014 it is
  still on the 03:06:21 exe.

  **THE GATES, ON THE BUILT EXE.** F2 byte identity: the new exe with no flag,
  with `--road-opacity 1`, and with `--roads-legacy` reproduces the rung **9/9
  and 10/10 files identical in all three arms on both chunks**, every file
  compared, not a sample. The compare is shown able to fail in the same run:
  `--road-opacity 0.326` moves 3 files on (-20,20) and 4 on (-8,8), **all of
  them colour** \u2014 the `_msn`, the `_data`, the `.bto`, the `.lodl` and the
  `.lodm` are byte-identical at every setting. F4 chain at GRADE1's baselines
  row for row: `lodgen_roads` **11/0** (R5 floor 0.1354, after 0.3435, reference
  0.4039, bars 0.2708/0.3231 both cleared; centreline colour error 38.04 \u2192
  22.34), `lodgen_terrain` **26/0**, `lodgen_terrain_vt` **41/1**,
  `lodgen_ground_cover` **29/5**, `lodgen_terrain_pbrm` **14/0**,
  `lodgen_native` **0 failures in all seven sections**, `lodl_open` **23/0**,
  `lod_generation` **116/0**. Zero NifSkope processes left running.

  **THE ONE RED ROW IS NOT THIS LANE'S, AND THERE IS A CONTROL FOR IT.**
  `lodgen_terrain_vt` holds its 41/1 count but the failing check is **V9c**
  where the historic red row was V9b. The rung exe was re-run through the same
  harness (`EXE=release/NifSkope.before_roads3.exe`,
  `logs/f4_vt_RUNG_control.txt`) and fails V9c with **digit-for-digit identical
  numbers** \u2014 E/W seam 188.074, interior 13.243, ratio 14.20, edge step 14.348.
  So it predates ROADS3. **NEW RED: V9c wants a lane** \u2014 the E/W chunk-seam step
  is 14.20x the interior against a bar of 3.20, and the interior control itself
  (13.243 / 12.182) is outside its own 1.20..2.20 window.""")

h = sub(h, u"""  **WHAT SHIPS (uncompiled): `--road-opacity A`, default 1.0**,""",
    u"""  **WHAT SHIPS: `--road-opacity A`, default 1.0**,""")

h = sub(h, u"""  **BUNGO'S CALL \u2014 the priced table** (simulated, both tiles):
  a = 1.000 \u2192 (-20,20) 99.05 / rise +29.96, (-8,8) 106.68 / +3.84;
  a = 0.830 \u2192 92.57 (+0.05 vs vanilla) / +23.48, 105.91 / +3.07;
  a = 0.500 \u2192 80.01 / +10.91, 104.40 / +1.56;
  a = 0.326 \u2192 73.38 (\u221219.15) / **+4.28, vanilla's own rise**, 103.61 / +0.77;
  a = 0.250 \u2192 70.48 / +1.39, 103.26 / +0.42.""",
    u"""  **BUNGO'S CALL \u2014 the table, both tiles; BAKED rows marked:**
  a = 1.000 BAKED \u2192 (-20,20) 99.05 / rise +29.96, (-8,8) 106.68 / +3.84;
  a = 0.830 BAKED \u2192 **92.43 (\u22120.09 vs vanilla's 92.52)** / +23.34, 105.89 /
  +3.05;
  a = 0.500 priced \u2192 80.01 / +10.91, 104.40 / +1.56;
  a = 0.326 BAKED \u2192 73.09 (\u221219.43) / **+4.01 against vanilla's +4.29**, 103.39 /
  +0.55;
  a = 0.250 priced \u2192 70.48 / +1.39, 103.26 / +0.42.""")

h = sub(h, u"""  the per-texel opacity the wash would need on the right-hand scale). **The
  candidate columns are SIMULATED, not baked, and say so on the picture** \u2014 the
  simulation is the generator's own composite arithmetic but does not pass
  through 8-bit quantisation or BC1, so it is right to about half a level. They
  are re-taken from real bakes when the build is spent.""",
    u"""  the per-texel opacity the wash would need on the right-hand scale). **Every
  panel in both pictures is a real bake or Bethesda's own sheet \u2014 nothing is
  simulated.** How good the pre-build pricing turned out to be, measured rather
  than guessed: it agrees with the bake on the AGGREGATES to **0.29** and
  **0.22** of a level and differs **per texel** by 1.58 levels on average, 7.34
  at p99 and **15.28** at worst, because the bake goes through 8-bit
  quantisation and BC1 and the simulation does not.

  **THE SKIRT IS GEOMETRY AND THE KNOB CANNOT REMOVE IT.** Correlating road
  luminance with the road mesh's own interpolated vertex alpha (ROADS2's
  `seam.py`, re-run on these bakes): vanilla **+0.001**, ours **\u22120.792** at
  a = 1, **\u22120.694** at 0.83, **\u22120.436** at 0.326 \u2014 and our unpainted ground's own
  floor on the same texels is **\u22120.325**. Opacity walks it toward the ground's
  floor, never to vanilla's zero. The feathered-boundary gradient (54 texels,
  vanilla 4.242) reads 3.979 / 3.352 / 2.979 at those three settings, inside
  vanilla's at all of them.""")

h = sub(h, u"""  **OWED:** the build itself, and with it gates F2 (off == rung bytes, `cmp`
  every file, compare shown able to fail), F3f/F3g/F3h and the whole F4 harness
  chain at GRADE1's baselines, plus re-taking the two pictures from real bakes.
  All of it is scripted in `scratchpad/roads3_20260911/PENDING.md`.""",
    u"""  **OWED:** bungo restarts his NifSkope window (new exe 04:10:38); nobody has
  looked at any of this in the game \u2014 the default's bytes did not move so there
  is nothing new to see at the default, and the two candidate settings have not
  been flown. `scratchpad/lodui1_20260911/BAKE_INSTRUCTION.md` needs **no
  change**: no default moved, so the instruction it carries still bakes the same
  bytes.""")
save('HANDOFF_BLOCK.md', h)
