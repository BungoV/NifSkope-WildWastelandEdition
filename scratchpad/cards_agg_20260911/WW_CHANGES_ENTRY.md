## 2026-09-11 — Aggregate ring-3 impostors: one card set per forested cell

`--aggregate` (off by default) gives every FORESTED cell a single impostor card
set, composited from that cell's own trees' card sheets at the rotation and
mirror the repetition breaker gives them, photographed from eight horizon
azimuths. bungo asked for it on 2026-09-11 08:2x–08:3x ("1 sounds good") and the
count of forested cells was printed before a line of it was written.

**The census, from `Fallout4.esm` alone, no bake.** The whole Commonwealth holds
**64,662 tree placements with a LOD base over 3,685 cells**; at the default
threshold of 8 trees a cell, **2,631 cells are forested and hold 60,605 of them
(93.7 percent)**. The typical forested cell is a dozen to three dozen trees
(p50 14, max 92 — no cell reaches 128). Another **99,274** tree placements have
no LOD model at all and are not aggregatable; they vanish at every ring today.
The 9-chunk Sanctuary region: 97 forested cells, 3,423 trees.

**What it writes.** Three BC3 sheets and a `kind: "aggregate"` `.lodm` a cell at
`Data\Textures\Lodgen\Aggregate\<ws>\<cx>_<cy>_agg.*` — colour + coverage,
normal + height + sway, mask; **no emissive** (a forest emits nothing;
`emissiveScale` 0). The `.lodi` goes to **version 4** and gains a 48-byte
aggregate row a cell plus a u32 blob naming exactly which instances each
aggregate stands for. The instances are not removed — this format has no
per-ring list — they are SUPPRESSED past a projected size, and the header
carries the threshold (`aggSwitchPx` 96 reference pixels) and the cross-fade
band (`aggBandRatio` 1.2), which at the contract's reference projection is
48,800…58,500 units, about 2.4 cells wide.

**One identity per aggregate**, not the dominant tree's, in a space disjoint
from the instance indices (`0x80000000 | index`): once a cell's trees are one
card they are one shadow caster, and sharing the dominant tree's identity would
make the far-shadow pass exclude the wrong pixels.

**Measured on the 9-chunk Sanctuary region:** 97 aggregates, **3,414 trees
photographed** (+ 9 refused, their bases having no card set) **= the census's
3,423 exactly, from two instruments that share no code**; `.lodi` 128,256 →
152,920 B; 388 sheet files, 8.4 MB at tile 64.

**What it costs, and the call it leaves.** A per-tree card is paid once per TREE
TYPE — all 36 Commonwealth tree types are about 28 MB, measured on disk. An
aggregate is paid once per CELL: 135.6 MB for the whole worldspace at tile 64 /
threshold 8, removing 57,974 quads from the far band; 571.9 MB at tile 128;
29.5 MB at tile 64 / threshold 32. Both are switches.

**The photograph is an orthographic COMPOSITE, not a viewport render**, and the
reason is a number: the render hook is 21,048 photographs and over seven hours
of sleep for the Commonwealth, while the card sheets have been orthographic and
metric since 2026-09-10, so compositing them is an exact resample. A card whose
sidecar does not say `projection ortho` is refused by name.

**Gates.** Count identity three ways per cell (row, `.lodm`, blob) 97/97 and
3,414/3,414/3,414, with a floor — 135 cells under the threshold, none with an
aggregate. The calibrated picture gate reads a silhouette-mass error of
**0.0525 mean / 0.1160 worst against a ceiling of 0.0138 and a floor of 0.3142**
over 128 cell-views, with two known answers printed above it. The height channel
moves (span 124..156 of 255, sd 8.89, correlation 0.773 with the cluster's
size). `--aggregate` off is **byte-identical to the rung over 27 of 27 files**,
with the comparator shown red first on a flipped byte and a missing file. A
standalone fixture gates the v4 layout with 13 mutations, each re-signed so the
ROW RULE refuses and each required to name it. Every baseline held:
`lodgen_native` 18/0, `lod_generation` 116/0, `lodgen_panel_run` 125/0,
`lodgen_terrain` 26/0, `lodgen_stage_times` 16/0, `lodgen_roads` 11/0,
`ui_align` 11/0, `water_ui` 82/0, `lodgen_terrain_vt` 41/1 (the carried V9b
red). `lodgen_octahedral` fails one check (`F1`, worst 1.88) **on the rung exe
too** — pre-existing, control log kept.

**Found by the gate and fixed:** the composite normalised each tree's layer by
the NUMBER of samples that landed in a texel rather than by the share of the
texel's AREA, so the aggregate carried 94 percent more coverage than the cluster
it stood for. One counted relink.

New files `src/lodgenaggregate.{h,cpp}`,
`.claude/skills/ww-downsample-gate/SKILL.md`,
`.claude/skills/ww-module-off-is-identical/SKILL.md`. Contract sections:
`docs/LODGEN_LODM_FORMAT.md` §3a, `docs/LODGEN_CARD_SHEETS.md` §10,
`docs/LODGEN_NATIVE_LODO_LODI.md` §4.6 with Deviations 12 and 13.
