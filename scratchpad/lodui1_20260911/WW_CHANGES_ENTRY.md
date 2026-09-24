## 2026-09-11 — LOD Generation panel: the five .lod types under FO4CS, Trees only, 512 px, the native row and the four stage times (lane LODUI1)

bungo's four rulings of that morning, in the panel.

**The five .lod types (06:4x, verbatim: "we should only have those 5 .lod types
in fo4 community shaders target").** Under **FO4 Community Shaders** the panel
offers `.lodl`, `.lodt`, the new `.lodo`/`.lodi` pair and the `.lodm` sidecars
that ride with the texture and card arrays. Four legacy rows are hidden whole:
the `.btr` section (with its terrain-texture bake, its ground-cover rows and its
identity channels), the `.bto` head, "Pack an object texture atlas" and "Chunk
textures from the pyramid". The **Stock engine** target shows all four again and
loses the native head instead. Counted both ways in `WW_LODGEN_TEST`, 4 of 4
hidden and 4 of 4 shown, with the hidden flag of each row printed beside its
name in the log.

The object SETTINGS section did not split in two. Its header now carries two
check boxes and exactly one is offered at a time (`LodgenSection::addHeaderCheck`):
`.bto` under the stock engine, `.lodo`/`.lodi` under FO4CS. The rows beneath
describe the object PASS — the same placements, channels, cull, simplification
and cards — and only the file they end in differs.

**Stated plainly and owed to bungo:** the object pass still writes the `.BTO`
chunk files under FO4CS, and the summary line says so in words. They are what
the pair, the texture arrays, the card arrays, the shape merge and the far-ring
cut are all built from, and what FO4CS's Improved LOD module reads today. The
row is gone as a choice; the file is not.

**The native row.** `LodgenNativeCheck`, "Native object files (.lodo/.lodi)",
default ON under FO4CS, `LodGeneration/native`. It arms the emitter in
`startChunks()` the way the CLI's region driver does and writes the pair in
`step()`'s tail, disarming on every path including a cancel. A GUI-driven
one-chunk Sanctuary run writes `Terrain\Commonwealth.lodo` **9,657,316 B** and
`Terrain\Commonwealth.lodi` **37,248 B** (`tests/spells/lodgen_panel_run.sh`).
This closes the item NATIVE1a and NATIVE1b both carried: *"the LOD panel has no
`.lodo`/`.lodi` row at all"*.

**Trees only (07:0x–07:2x).** `LodgenTreesOnlyCheck`, ON by default,
`LodGeneration/treesOnly`, reaching the bake as
`LodgenObjectOptions::treesOnly`. ON: only a base the chunk builder's own tree
test calls a tree may stand on a card. OFF: any base whose ring slot is EMPTY
may — the old "missing" rule and nothing else. The bake counts its refusals and
says so: *"N placements refused a card: not a tree (Trees only)"*. On a
Sanctuary region the candidate list is **19 with Trees only on and 33 with it
off**, and the floor is named: `00033794
Architecture\Shacks\ShackBalconyFloor03.nif`, present in one list and absent
from the other. `--candidates all` is **retired** and refuses by name in
`nifcli.cpp` and in `tools/bake_impostor_cards.sh`. The three path tests now
have ONE definition (`lodgenIsTreeModel`); the inline copy at the repetition
breaker is gone.

**512 px (07:3x, "Add it, why not").** In `LodgenCardResBox` and in the bake
driver's `TILE` list. The cost line is computed and moves: `2048 x 2048 sheets
at 256 x 256 a frame: 18.67 MB` becomes `4096 x 4096 sheets at 512 x 512 a
frame: 74.67 MB`. Measured through the bake hook on `TreeMapleForest1.nif` at
`OCT=8`: **256 px → a 1152 x 2048 sheet, 512 px → 2176 x 4096**. The long side
is the row's number times the frame count; the short side narrows with the
tree's own silhouette, which is what the row's tooltip has always said.

**"Cards from ring" is now "Tree cards from ring" (07:4x)**, tooltip one
sentence, and it is tree-only in effect in both states of the Trees-only row: a
non-tree with an authored mesh is never replaced by a quad.

**The four stage times.** landscape / meshes / textures / impostors, in the
panel's new result line (`LodgenResultLabel`, under the summary in the pinned
bar) and on the command line, from one formatter (`lodgenStageTimeLine` in
`src/lodgen.cpp`) so the two cannot word them differently. Each is accumulated
from the calls that belong to that stage and nothing else, so a stage that did
not run reads exactly `0.0 s` — which is what the gate rests on. Measured on the
one-chunk Sanctuary region: a GUI object run reads `landscape 0.0, meshes 3.8,
textures 0.2, impostors 0.0`; a GUI heightmap run reads `landscape 1.1, meshes
0.0, textures 0.0, impostors 0.0`. The CLI reads the same four.

**Gates.** New `tests/spells/lodgen_stage_times.sh` **16/0 PASS** (the four
stages both ways, the `all` refusal, the trees list both ways with a named
floor) and `tests/spells/lodgen_panel_run.sh` **125/0 PASS** (the panel driven
to completion twice). `tests/spells/lod_generation.sh` **116/0 PASS**, floor
raised 74 → 116. Baselines held: `lodgen_native.sh` 18/0, `lodgen_terrain.sh`
26/0, `lodgen_terrain_vt.sh` 41/1 (V9b, red on the rung too), `lodgen_roads.sh`
11/0, `lodgen_ground_cover.sh` 29/5, `lodgen_terrain_pbrm.sh` 14/0.

Pictures in `scratchpad/lodui1_20260911/images/`: `panel_fo4cs.png`,
`panel_stock.png`, `result_line.png`. One-page bake instruction for the full
Commonwealth: `scratchpad/lodui1_20260911/BAKE_INSTRUCTION.md`.
