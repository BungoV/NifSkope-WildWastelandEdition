## 2026-09-11 — The road seam: one colour a material, and the families vanilla does not paint

bungo looked at the far terrain and said *"look at the roads, there is a visible
seam while vanilla doesn't have it"*. The seam was measured before anything was
changed, on chunk (-20,20) — vanilla's own `Commonwealth.4.-20.20.DDS` grid, 512
texels at 32 world units a texel, no resampling on either side — and it turned
out to be three separate things. Two of them are now fixed and the third is
named with a number.

**What the seam actually was.** Not a compositing bug at the piece joins, which
was the obvious guess. It is the **road diffuse's own texture pattern printed at
footprint scale**: the far bake sampled each road material's diffuse at the
texel's UV, and at 32 world units a texel a 256-world-unit UV repeat lands every
8.01 bake texels, so the material's own light and dark stripes are printed
straight into the sheet as a regular banding. The numbers that separate that
from every other explanation: within-material luminance correlation along the
road **0.852 for ours against 0.016 for vanilla**; a phase-fit against the UV
repeat explains R² **0.140** of our road luminance against **0.013** for vanilla
and a 0.022 floor; local 5×5 SD on the road **10.33 for ours against 6.62 for
vanilla**, while off the road the two agree (5.52 vs 5.38). Three other
candidate mechanisms were refuted with numbers and are written up in the lane
report: **0 of 474** road materials set `bAlphaBlend` (6 of 512 shapes do blend,
through their own `NiAlphaProperty`); the median covering-Z spread at a road
texel is 12.299 units, so z-fighting between pieces is not it; and **0 of 474**
shapes are mip-clamped.

**The fix, and the rule vanilla implies.** Vanilla's far road measures as **one
flat colour a material** — its own diffuse average under the sheet's grading —
with no internal pattern at all. So the rasteriser gained a detail knob and it
defaults to zero: `--road-detail 0` (the default) lerps the diffuse sample all
the way to the texture's own average, `--road-detail 1` prints the footprint
sample, which is what banded the road. A second knob, `--road-composite`,
selects how overlapping pieces combine: `max-z` (the default) lets the topmost
triangle win the texel, `blend` paints the pieces in order — ascending mean
world Z, non-decal before decal — and composites `dst = lerp(dst, src,
srcAlpha)`.

**The composite default was decided by the harness, against the plan.** The
blend was expected to win and it lost. On the road-presence metric in
`tests/spells/lodgen_roads_metric.py` — road mask taken from VANILLA's own
colour, eroded once to a centreline, scored as the fraction of centreline texels
within 16/255 per channel — the four variants read: the old pipeline **0.3061**,
**max-z + detail 0 = 0.3404**, blend + detail 1 = 0.2481, blend + detail 0 =
0.2669, against pre-registered bars of 0.2694 and 0.3228. Only max-z clears
both. It is also better at the piece boundaries (5.996 against blend's 6.204,
vanilla 5.271). Blend does win three of the five measures — local 5×5 SD, the
phase R², and mean road colour error — so it is kept behind the flag rather than
deleted, and the report names every number on both sides.

**Two families come out of the paint, each on its own number.**

*Raised roads.* Vanilla does not paint highway decks or bridges into the far
sheet; they are drawn as objects at distance, and they carry their own Distant
LOD meshes. On chunk (-8,8) downtown, clearance above a displaced-mask floor
(tie-averaged brightness) for the elevated road family reads **−0.009 for
vanilla** and **+0.314 for the old pipeline** — we were painting a bright
interchange vanilla has nothing of. `--no-road-raised` is now the default and
refuses a road base that carries a Distant LOD mesh, plus anything under
`Landscape\Roads\HighwayOverpass\` or `…\Bridge\`. After it, the same clearance
reads **+0.001**. Downtown road texels fall 163,586 → 40,436.

*Pavements.* Chunk (-8,8) carries 17,801 projected sidewalk texels, 15,696 of
them more than two texels from any flat road. On those, vanilla's brightness
clearance is **−0.102** — vanilla paints no pale kerb there at all — against
**+0.284** for ours, and mean luminance **86.5 for vanilla against 128.4 for
ours**, a 42-unit error, the worst family error in the sheet against 18.5 for
the tile as a whole. On the same tile the flat road family reproduces vanilla's
own clearance to **0.001** (+0.101 against +0.100). So the roads stay and the
pavements come out: `--no-road-sidewalks` is the default, `--road-sidewalks`
puts them back.

**One token is the whole way back.** `--roads-legacy` restores all four —
`max-z`, full detail, the raised families and the pavements — and a bake made
with it is **byte-identical to the bake before any of this existed, 9 of 9
files, on both regions tested**. Anything named explicitly on the same command
line still wins, so the flag can also be used to move exactly one thing away
from the old behaviour. `--no-roads` is likewise still byte-identical to the
road-free bake, 9 of 9 files.

**A tree-filename clause got narrower.** `lodgenIsTreeModel()` matched any path
with a `trees` component anywhere in it, which also caught
`SetDressing\TreeSwing01.nif`, `TreeNoose01_Branch.nif` and five siblings —
swings and gallows props, not trees. The clause is now scoped to `Landscape\`.
Over every placed base in the Commonwealth census: **137 bases classify as
trees under both the old and the new rule** (135 under `Landscape\Trees`, 2
under `Landscape\Plants`), **7 flip out** (82 placements, **none of which
carries a distant LOD mesh**, so no impostor or card ever came of them), and
**none flips in**. 36 of the 137 kept bases carry a distant LOD mesh, which is
exactly the 36 lines `--list-impostor-candidates --candidates trees` prints for
the whole worldspace — so the re-typed classifier and the shipped C++ agree
base for base.

**What this changed for the seam, measured on the shipped exe.** Mean luminance
gradient on chunk (-20,20): all piece boundaries **9.752 → 5.718** against
vanilla's 5.271; the 54 feathered boundaries **11.837 → 3.988** against
vanilla's 4.242; local 5×5 SD on the road **10.375 → 4.369** against vanilla's
5.533. `lodgen_roads.sh` goes **11/0** with the metric at 0.3431 against bars
0.2694 and 0.3231, and the mean colour error against vanilla on the road
centreline falls from 38.11 with no road to **22.37** with one.

**What is still not vanilla, and is said out loud.** Our road interior is now
*too smooth* — local SD 4.369 against vanilla's 5.533, having been 10.375 — and
its mean luminance is still about 9 units bright (101.6 against 92.18). Laid
beside vanilla at Sanctuary, vanilla's cul-de-sac is a soft desaturated
blue-grey blob and ours is a crisp warm pale-grey ribbon with a visible kerb
line: the geometry is right, the hue and the crispness are not. And the ground
under everything is about 16 luminance units darker than vanilla's, which is the
splat tiling and the grading, not the road. Pictures with the numbers burned in:
`scratchpad/roads2_20260911/images/cmp_seam.png`, `cmp_highway.png`,
`cmp_sidewalk.png`, `cmp_sanctuary_road_v2.png`.

**The whole harness chain is at its baseline** — `lodgen_terrain.sh` 26/0,
`lodgen_terrain_vt.sh` 41/1 (`V9b`, red on the rung too), `lodgen_ground_cover.sh`
29/5 (red on the rung too), `lodgen_terrain_pbrm.sh` 14/0, `lodgen_native.sh`
18/0, `lodgen_panel_run.sh` 125/0, `lod_generation.sh` 116/0, `ui_align.sh`
11/0, `water_ui.sh` 82/0 — and `lodgen_roads.sh` moved from 11/1 back to
**11/0**.
