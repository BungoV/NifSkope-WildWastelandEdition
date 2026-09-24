## 2026-09-11 — ROADS1 — a difference was attributed to roads on a tile that has no roads

**What was done.** The handoff block for lane TERRAIN-R, and the picture
`scratchpad/terrain_r_20260911/images/ours_vs_vanilla_tile.png` it points at,
say of chunk (-20,24): *"mean difference 19.96/255, and most of it is the roads
and a rubble patch vanilla carries and we do not"*. ROADS1's brief inherited
that sentence and named that picture as its target.

**What was true instead.** Chunk (-20,24) contains **zero road triangles**.
Projecting every `Landscape\Roads\*` and `Landscape\Sidewalks\*` placement in
cells -24..-12 x 16..30 onto the dim-4 chunk grid puts 86,770 road triangles in
chunk (-20,20), 63,703 in (-16,24), 284,532 in (-16,16) — and none at all in
(-20,24). The Sanctuary loop road is one chunk SOUTH of the tile that was
photographed. The 19.96 mean difference on that tile is real, but its cause is
the splat grading and the 17-grid blockiness, not roads.

**How it was found.** ROADS1's first step was to locate the road geometry before
measuring anything, so the tile could be chosen by the roads rather than by
inheritance. The per-chunk road-triangle histogram is four lines of the lane's
own placement walk.

**The rule.** *Before attributing a difference to a THING, project the thing and
check it is in the frame.* A named cause is a measurement (CONSTITUTION 4, the
second of the three rules of 2026-09-04 21:33), and "most of it is the roads" is
a cause. One histogram over the candidate tiles costs seconds and would have
moved the picture one chunk south.

## 2026-09-11 — ROADS1 — a census field that could not move was written before it was caught

**What was done.** The road census shipped a `refused_nomodel` counter — "road
bases whose MODL is empty" — and the code incremented it after the road test had
already run.

**What was true instead.** A placement is recognised as a road BY ITS MODEL
PATH. A road base with no model can never be recognised as a road, so the branch
was unreachable and the field would have been a permanent zero.

**How it was found.** Re-reading the new code against the first of the three
rules of 2026-09-04 21:33 (no census field ships without a test that it is
written AND that it MOVES) before the build, not after.

**The rule.** *Ask of every new counter: name the input that makes it non-zero.*
If the answer needs the code to be in a state the code cannot reach, the field
is decoration and it does not ship. The field was removed rather than left in at
zero.

## 2026-09-11 — ROADS1 — the road pass inherited a material resolver that cannot open an absolute Bethesda build path

**What was done.** The first build's road pass took each shape's diffuse and
decal flag from `lodgenLoadModel`, whose material fix-up prepends `materials/`
to any path that does not already start with it.

**What was true instead.** Every `Landscape\Roads\Country\*` and
`Landscape\Roads\Alley\*` piece names its material as an ABSOLUTE build path —
`C:\Projects\Fallout4\Build\PC\Data\materials\Landscape\Roads\
AsphaltAndSWEdgeDecals01.BGSM` — and carries an EMPTY texture set, because the
material is meant to supply the textures. Prepending `materials/` to that gives
`materials/C:/Projects/...`, which resolves to nothing, so the shape ended with
no diffuse at all. On the Sanctuary loop-road chunk that was 65 of 270 road
shapes, and it was EVERY decal among them.

**How it was found.** The census. The first run read `decalshapes=0` and
`refused_notexture=85`; an independent Python census of the same window said 69
of 305 road shapes have `bDecal` true in their material and that every one of
those materials exists on disk. Two instruments, one of them not ours, and the
disagreement was the finding.

**The rule.** *A refusal counter is a finding, not noise.* `refused_notexture=85`
alongside `decalshapes=0` was the defect announcing itself in the first gate run;
believing the `0` would have shipped a road pass that silently drops every
country-road decal. Read the refusals before reading the successes.

**Still open.** The fix is in the ROAD pass only (`lodgenRoadMaterialPath`). The
shared `lodgenLoadModel` still cannot resolve such a path, so the OBJECT bakes
still drop those textures — widening it there would move output the byte-identity
gates pin, so it is reported for bungo rather than taken here.
