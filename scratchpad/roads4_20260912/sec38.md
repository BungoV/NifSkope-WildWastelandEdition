## 3. Gates

Every row is a number read off a file in `scratchpad/roads4_20260912/logs/` or
`scratchpad/roads2_20260911/seam_r4_*.json`, named in the row. "Before" is the
rung `release/NifSkope.before_roads4.exe` (built 05:48:33, the `--road-detail`
default still 0); "after" is `release/NifSkope.exe` built 06:31:05 by this lane.

| gate | as the brief wrote it | before | after | verdict |
|---|---|---|---|---|
| G0 | a default bake == a `--road-detail 1` bake byte for byte; `--road-detail 0` reproduces ROADS3's `new_default` byte for byte | n/a (the rung has no such default) | 0 of 9 files differ on (-20,20), 0 of 10 on (-8,8), for both halves | **GREEN** |
| G1 | vertex-alpha correlation | -0.792 (ROADS3, `seam.py`) | ours **-0.0345**, vanilla **-0.0359** on (-20,20); ours **+0.0404**, vanilla **+0.0766** on (-8,8) | **REFUSED** -- the gate's own number is an instrument artefact, see 1.1 |
| G2 | cross-road max second difference; bars: vanilla 1.31 on (-20,20), 1.25 on (-8,8) | 1.034 / 1.711 (vanilla, re-read on this lane's axis) | ours **4.474** on (-20,20), **1.820** on (-8,8) | **RED on (-20,20)** |
| G3 | ROADS2's feathered boundary not worse than 3.979, the solid-boundary control unchanged | feathered **3.988**, solid **5.750** | feathered **5.080**, solid **9.599** | **RED**, and it is item 0's flip that made it red |
| G4 | road MASK width within 1 texel of vanilla's | 9.64 / 4.22 | painted **9.70** vs projected **10.24** on (-20,20) (0.54 short); **4.26** vs **5.64** on (-8,8) (1.38 short) | **GREEN on (-20,20), RED on (-8,8)**, against a SUBSTITUTED reference -- see below |
| G5 | F2 byte identity | n/a | `--no-roads` identical to ROADS3's `rung_noroads`, 9 and 10 files, 0 differing; default identical to `rung_detail1` | **GREEN** |
| G6 | ROADS3's F4 harness rows unchanged (`lodgen_roads.sh` 11/0) | 11 checks, 0 failures | **11 checks, 1 failure** -- R5 after 0.3078 against bar 2 = 0.3223, short by **0.0145** | **REFUSED with a number** |

Two harnesses beyond G6 were run on the 06:31:05 exe at 06:43 and are green:
`lodgen_terrain.sh` 26 checks / 0 failures, `lod_generation.sh` 116 / 0.
`scratchpad/roads4_20260912/logs/after_*.txt`.

### G0 and G5, what was actually compared

`diff -rq` over the whole output directory with `bake.log` excluded, because
that file records the command line and the wall clock and must differ. Nine
files on (-20,20) and ten on (-8,8) -- the colour sheet, the `_msn` normal
sheet, the mesh, the manifest and the meta report among them. Zero differ in
all eight comparisons. This is the strongest result in the lane: the default
flip changes the DEFAULT and nothing else, and `--road-detail 0` still
reproduces ROADS3's bake exactly, so the old look is one switch away.

### G2, and the axis it is read on

The second difference depends on which texels the signed-distance axis is built
from. Read on the painted mask (`logs/gp.json`, the axis the whole variant
table uses) ours is 4.474 and vanilla 1.034 on (-20,20). Read on the painted
mask intersected with the projection (`logs/gates.json`) ours is 5.191 and
vanilla 0.964. Both readings say the same thing and neither is near the brief's
bar of 1.31: **our road has a profile that still ramps where vanilla's is
flat.** `--road-opacity 0.326` brings it to 1.976 -- still not 1.31, and it
costs R5 (see 2).

### G3 is red because of item 0, not because of item 2

The rung, with the old `--road-detail 0` default, read feathered 3.988 against
vanilla 4.242 and a displaced floor of 4.061 -- that is where the brief's 3.979
comes from. bungo's ruling moved the default to detail 1, and detail 1 is what
makes the feathered boundary hotter: 5.080 against the same vanilla 4.242, and
the solid control 9.599 against 5.291. G3 as written cannot be met while the
ruling stands. It is reported, not negotiated: **the ruling outranks G3**, and
G3's replacement is the number above with the ruling named beside it.

### G4's reference is substituted, and here is the substitution

The gate wants "vanilla's mask width". There is no way to measure it: a painted
mask is the difference between a road bake and a `--no-roads` bake of the same
generator, and Bethesda shipped one sheet with no such pair. The substitute is
the **projected road width** -- the mean inward distance over the texels the
ESM's road geometry actually covers, which is a world fact and therefore the
same number for vanilla and for us: 10.24 texels on (-20,20), 5.64 on (-8,8).
Ours paints 9.70 and 4.26 of them. The (-8,8) shortfall of 1.38 texels is the
one that misses the band, and it is a shortfall in the same direction as the
projection/paint gap already reported in 1 (197 projected texels on (-8,8)
carry no paint at all).

### What was NOT measured

* **In the game.** Nothing in this lane was seen in Fallout 4. Every picture is
  a bake read off disk.
* **Any chunk but two.** (-20,20) Sanctuary and (-8,8). No DLC worldspace, no
  far ring, no dim 8/16/32.
* **The `--roads-legacy` path**, deliberately: it is pinned by G5's byte
  identity and was not otherwise exercised.
* **Sidewalks and raised roads as separate classes.** `--road-sidewalks` and
  `--road-raised` were left at their defaults throughout.
* **Whether bungo prefers the opacity trade.** Section 2 prices it; the choice
  is his and the lane did not make it.

## 4. Pictures

All three are in `scratchpad/roads4_20260912/images/`, drawn by
`scratchpad/roads4_20260912/r4_pics.py`. **Every panel is a real bake** written
by the 06:31:05 exe, or Bethesda's shipped sheet read off disk; nothing is
recoloured or synthetic.

| file | what it shows |
|---|---|
| `road_ground_look.png` (2108x1088) | the Sanctuary window at 4:1, four panels: VANILLA, our default (detail 1), `--road-ground-paint 0` (refuted), `--road-opacity 0.326` (not shipped). Under each: road L, terrain L, surface L, the seam gradient and the two-tone step. The window is the 96x96 with the MOST terrain-material texels in it, at (143,187) -- chosen by the measurement, not by eye. Whole sheets underneath with the window boxed. |
| `road_ground_where.png` (2084x1094) | our default bake beside the same sheet with the terrain-material texels in red and the road surface in blue. This picture IS the finding: the red is the verge and the junction fill along the Sanctuary loop, modelled inside the road NIFs and materialled from `materials/Landscape/Ground/`. |
| `road_ground_profile.png` (1100x620) | the cross-road profile, mean luminance against signed distance to the painted edge, all five curves on the same texels. Vanilla flat inside the road (2nd difference 1.034), our default ramping (4.474), `--road-ground-paint 0` worst (11.027), `--road-opacity 0.326` (1.694), our ground under everything (1.023). |

The profile chart is the brief's "profile chart re-drawn". The brief also asked
for "vanilla | rung (detail 1) | winner" in the crop; the rung at detail 1 is
byte-identical to our default (G0), so that panel and the default panel would be
the same image -- the third and fourth panels are spent on the two candidates
instead, which is where the information is.

## 5. Documents

Four files, written by this lane, for the overseer to splice:

| file | for |
|---|---|
| `scratchpad/roads4_20260912/WW_CHANGES_ENTRY.md` | `WW_CHANGES.md`, heading `## 2026-09-12 -- ...` with an em dash |
| `scratchpad/roads4_20260912/HANDOFF_BLOCK.md` | `HANDOFF.md`, a new top block |
| `scratchpad/roads4_20260912/MISTAKES_ENTRIES.md` | `MISTAKES.md` at the repo root, three entries, newest first |
| `scratchpad/roads4_20260912/LODGEN_TERRAIN_VT_1a5.md` | the amendment to `docs/LODGEN_TERRAIN_VT.md` section 1a.5 |

Also written directly, not left for a splice:

* `scratchpad/lodui1_20260911/BAKE_INSTRUCTION.md` -- a dated note at the end
  saying what changed under it (the default flip, the new switch, and that a
  bake made before 06:31 on 2026-09-12 is a `--road-detail 0` bake).
* `scratchpad/uinotes1_20260912/RESUME_BY_ROADS4.md` -- item -1's two chain
  tables, written for lane UINOTES1b.
* `.claude/skills/ww-material-folder-classify/SKILL.md` -- the repeatable
  procedure this lane found twice (see 8).

## 6. Build

| | |
|---|---|
| exe | `release/NifSkope.exe` |
| timestamp | **2026-09-12 06:31:05** |
| size | **21,819,904 bytes** |
| builds | **one** |
| relinks | **zero** |
| sources changed | `src/lodgen.h`, `src/lodgen.cpp`, `src/nifcli.cpp` -- all three CR count 0 (LF-only) by Python byte counts, before and after |
| rung | `release/NifSkope.before_roads4.exe`, the 05:48:33 exe, md5 `980e64c1aa4e5478b5833d83ebea9655` |
| game | `Fallout4.exe` was up (pid 48328) from 05:42:58; every bake, every harness and the build itself happened after it went down. `tools/ww_build.sh` refuses while it is up and was not overridden. |
| copies | `style.qss` and the shaders copied at link time by `QMAKE_POST_LINK`, checked in step |

The exe is newer than all three sources. `tools/ww_build.sh` gated `make -j2` on
its own exit code, BUILD-RC=0.

**bungo's open NifSkope window needs a restart to pick this exe up.**

## 7. Mistakes

Three, written out in full in `scratchpad/roads4_20260912/MISTAKES_ENTRIES.md`:

1. **A zero-initialised instrument produced a finding.** ROADS3's `seam.py`
   allocated its vertex-alpha buffer with `np.zeros` and never wrote the ones
   for shapes that have no vertex alpha, so "luminance correlates -0.792 with
   vertex alpha" was a correlation with *which shapes have an alpha channel*,
   not with alpha. Mine, inherited and believed for the first hour; found by
   re-deriving the buffer from the NIFs with ones as the default. This lane's
   G1 exists only because of that number.
2. **The brief's premise was not checked before it was built on.** "The road
   meshes carry terrain-shaped skirt geometry; the far bake must not paint it as
   road" -- skirt-only texels are **0** on both chunks. A classifier by vertex
   alpha was written and run before the count that refutes it was taken. The
   count is two lines and should have been line one of the lane.
3. **An inherited chain was run after this lane's own source edits landed.**
   Item -1 says run UINOTES1b's chains FIRST; I edited `src/lodgen.h`,
   `src/lodgen.cpp` and `src/nifcli.cpp` at 06:07-06:08 and ran the chains at
   06:10, so two harnesses reported "the exe is newer than every source this
   answer depends on" as RED when the only cause was my own uncommitted edits.
   The two rows are reported as unreadable rather than as regressions. The rule:
   **an inherited chain runs against the tree it was written for, before any of
   the new lane's edits touch disk.**

A fourth thing that is not a mistake but is owed to the record: six
segmentation faults in `lodl_open.sh`'s headless render path arrived with the
05:48:33 exe (it was 23/0 on the 04:10:38 one). That is UINOTES1b's territory
and is reported to it in `RESUME_BY_ROADS4.md`, not fixed here.

## 8. Finished-work skill review

**`ww-spec-gate-audit` earned its place three times.** "Run the gate on the OLD
binary first" is what turned G3 from a failure of this lane's change into a
failure of bungo's ruling -- the rung reads 3.988 and the flip alone moves it to
5.080, with item 2 not yet in the picture. "Print the TABLE, not the count" is
what made the candidate family collapse: `--road-ground-paint 0.75` looked like
progress on (-8,8) until the (-20,20) column was printed beside it and the seam
was monotonically worse. "Is the approximation ONE-SIDED" is exactly what the
zeroed alpha buffer was: an error that can only ever point one way.

**`ww-control-calibration`'s displaced floors are what stopped a second false
finding.** The seam gradient of 15.387 means nothing until the same 2,847 texels
displaced five ways read 6.077 on the same sheet; the gap is the signal.

**`ww-prototype-is-not-the-product` is why the knob ships at 1.0.** The
candidate was built, measured, and refuted, and the instrument that refuted it
is what ships -- as a switch with its measured numbers in its own help text, so
the next person does not have to re-derive them.

**A new skill is owed and is written:**
`.claude/skills/ww-material-folder-classify/SKILL.md`. The procedure that
repeated: *classify a shipped asset by the FOLDER its material lives in, never
by the material's file name*. A name-stem list put the two biggest contributors
in this lane (`CommonwealthDefault01.bgsm`, 7,558 texels, and `SancSW01.BGSM`,
5,317) in an "unclassed" bucket and hid the whole finding; the folder rule found
36.1 per cent of the road plane in one run. The skill carries the four path
prefixes shipped NIFs use, the normalisation that keys on the LAST `materials/`,
and the rule that the same discriminator must exist in BOTH the C++ and the
offline Python or the two measurements are not comparable.
