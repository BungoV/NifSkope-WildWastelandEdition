## 2026-09-12 — Every bake setting the command line has is now a row in the LOD Generation panel

Lane PANEL1, main tree. bungo asked whether erosion was a knob in the generation
menu. It was not — and neither were fifty-five other settings the `lodgen`
command line had grown. "They should all be configurable in the gen menu,
anything else we're missing in that menu?" This is the answer to both halves: the
audit of what was missing, and the rows.

### The audit

Every `--switch` the `lodgen` sub-command parses was read out of
`src/nifcli.cpp` and classed, by a script that reads the parser rather than by
hand, so the table cannot drift: 150 spellings, 15 `--x` / `--no-x` pairs, 2
retired spellings → **133 settings**.

| class | settings | already a row | no row | stays on the command line |
|---|---|---|---|---|
| bake setting | 93 | 37 | 56 | 0 |
| path or source | 9 | 3 | 3 | 3 |
| diagnostic or driver | 31 | 0 | 0 | 31 |
| **total** | **133** | **40** | **59** | **34** |

The 31 diagnostics (`--dump-*`, `--probe*`, `--*-check`, the stress and fixture
drivers) answer a question and stop; they are not part of a bake and stay where
they are. Three paths stay too, each for a reason: the texture folder the panel
derives from the output mod folder, the unpacked data root the house rule
replaced with the game's own folders and archives, and MO2's `plugins.txt`,
which the MO2 source reads at its known location.

### The rows: 57 of them, covering 56 switches

Grouped under headings that match the switch families, one setting to a row,
label and control with the explanation in the tooltip and the switch named at the
end of it: **Terrain** (water subdivision), **Land detail** (18 rows: tiling,
sample rule, detail, hex, warp and its lattice and octaves, mip bias, the guide
rule with strength, scale and slope reference, the fine-detail source and the
vanilla LOD root, crevice shading, colour grade, quadrant edges and margin),
**Erosion** (strength, rounds, seed), **Sheets and cache**, **Ground cover**,
**Object modules** (distance ladder, occluder boxes, merge, atlas format),
**Run** (model threads, chunk threads), the pyramid's own six numbers under the
existing virtual-texture section, and four folding sections with their own
on/off tick: **Roads**, **Object occlusion in the terrain**, **Water bodies in
the landscape file** and **Aggregate impostor sheets**.

**No default moved.** Every row loads the value the code already held —
`src/lodgen.cpp`'s file statics, `src/lodgen.h`'s option structs,
`src/lodtfile.h`'s water options. The land guide still ships off, erosion still
ships at 0, the sheet format is still legacy. The one row that looks like a new
default is not: `Atlas format` is a three-way selector whose default, "Match the
target", is exactly the rule the panel already applied.

Three rows were ruled out rather than added. `--road-opacity` and
`--roads-legacy` are not offered — "we don't use that opacity at all, we render
roads at their full diffuse". The terrain and object identity switches keep the
rows they already have and their current defaults: legacy terrain bakes stay as
they were. `--incremental` is owed: the switch arms a flag, but the leg that acts
on it lives in the command-line driver and not in the run the panel starts.

### What proves the rows are wired

A row that exists and scrolls and remembers itself can still be connected to
nothing, so the gate is the bake, on one chunk — Sanctuary, (-20,24), at dim 4.

* **The same panel run, on the exe before this change and on the exe after it,
  wrote the identical tree**: 11 files, 47,645,750 bytes, the same SHA1. Fifty-
  seven new rows, every one at its default, and not one byte moved.
* **Then each row alone was moved off its default and the chunk baked again** --
  49 bakes -- and the panel's own self-test names the verdict row by row: **32
  rows move the bake's bytes, 2 leave them alone, 23 this one chunk cannot
  exercise** (each naming the spell that does). The two that leave the bytes
  alone are the two thread counts, which is the answer that was wanted: a bake
  whose output depends on how many cores ran it would be a bug, not a setting.
  Where a row hangs off another, the gate turns the parent on first -- and where
  the parent is a rule rather than a switch, sets it to the rule that reads the
  row -- so a quiet row is the row's verdict and not the setup's.
* **Finally the same chunk was baked from the command line with the same
  settings, and the two trees were compared file by file: 15 files, all
  byte-identical, 0 differing, 0 missing.** The panel and the command line now
  bake the same bytes rather than the same intentions. (`Commonwealth.lodb` is
  excluded and named as excluded: it is the generator's own ledger of its runs
  and differs between any two runs by design.)

### Seeing it

The settings column is now 2,897 px tall with every section open, which no dock
screenshot can hold, so the panel's self-test gained a second grab:
`WW_LODGEN_SHOT_FULL=<png>` photographs the scroll area's whole inner column
with every folding section opened for the picture and folded back afterwards
(the fold state is yours and persists, so it is clicked open and clicked shut).
`scratchpad/panel1_20260912/shots/panel_full_after.png` is that picture, 483 x
2,897, and it carries 55 of the 57 new rows -- the two it does not are
`Merge the chunk shapes` and `Atlas format`, which the panel hides under the FO4
Community Shaders target because they belong to the stock engine's chunk path.
The existing dock grab is unchanged and comes out byte-identical.

The panel's structural self-test went from 116 checks to 121, with its floors
raised to the measured counts (45 numbers, 10 headings, 14 selectors) — the old
exe fails all four.
