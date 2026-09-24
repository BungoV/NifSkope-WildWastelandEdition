## 2026-09-12 — far terrain gets ambient occlusion from placed objects, and a hydraulic erosion pass

Lane GROUND1, main tree. Two briefs in one lane: TERRAIN-AO1 and EROSION1, one
subsection each. Both shade the same bake texel from the heightmap and neither
reads the other's intermediate; the order they compose in and why it is a one-way
dependency is stated in `docs/LODGEN_TERRAIN_VT.md` sections 2.5h and 2.5i.

### Part A — far terrain ambient occlusion from the placed objects (`--terrain-object-ao`)

The far terrain has always occluded itself — an eight-direction horizon march
against its own heights — but nothing standing ON it cast any shade at all. A
town in the distance lit exactly like empty ground. `--terrain-object-ao` runs
that same march a second time against a height field built from the placed
objects and multiplies the two visibility fractions together.

* **Off by default, and off is the previous bake's bytes.** Not to a tolerance:
  where nothing is in reach the new term returns exactly `1.0f` by early return,
  and `vis * 1.0f` is bitwise `vis`. Gate A1 is a full-region `diff -r` against a
  bake from the rung exe: identical.
* **The height field** is a world-aligned lattice of 128-unit squares holding the
  maximum Z of each placement's **level-0 LOD mesh** — not its cluster sphere. A
  sphere the size of a church casts a hemisphere of shade the church does not.
  A base with no distant-LOD mesh is refused BY NAME into the census rather than
  silently skipped, because a thing that is not drawn at distance does not
  shadow at distance: 11,826 placements over 450 bases on the measured region.
* **The strength default is 0.5 and it is a measurement, not a taste.** The law
  as written is 1.0, and at 1.0 the term saturates: the AO byte falls from mean
  211.55 to 93.33 and **276,234 of 1,048,576 texels clamp flat to zero**, at
  which point "under a tree" and "under a tower" are the same byte. 0.5 is the
  largest sampled strength at which nothing clamps anywhere in the region (the
  darkest texel keeps 24 of 255). `--terrain-object-ao-strength 0..4` moves it.
  Caveat on the floor: ONE region, and a forested one (45,222 of 147,456 lattice
  squares occupied), four sampled points, not a curve.
* **The reach is 1,458 units and it was measured.** The march reads 56 world
  points; a texel none of whose 56 points lands on an occupied square cannot
  move. 860,624 darkened texels, every one with its occluder at 1,458 u or
  nearer and none farther. That is inside the one-cell widening the incremental
  ledger already applies, so `docs/LODGEN_LEDGER_FORMAT.md` carries it as row 9
  and `--incremental` needs no new invalidation.
* **It lands in BOTH sheet composites** — the stock chunk path and the pyramid tile
  baker — through one shared function, so the two cannot drift apart. It does NOT
  reach the per-vertex terrain channels: the chunk meshes are byte-identical
  with the switch on.
* **`--dump-object-ao FILE`** writes the lattice itself, so a gate asking "is the
  ground under a building darker than the same ground elsewhere" can get the
  footprint from somewhere other than the map it is testing.

**Red, and said plainly.** `--terrain-object-ao` together with `--lodl` writing
is **refused**, exit 2 with the reason named. The `.lodl`'s AO plane is computed
from the container's stored heights by the one function that also serves
`--refresh-ao`; putting objects into it would make a refreshed plane silently
differ from the written one. The brief asked for the plane; this lane refuses it
instead and says so. Consequence: ring 0 (from the `.lodl`) and the pyramid mask
disagree about object shade until that is settled. Bake the sheets with the
switch and write the `.lodl` in a separate run without it.

Harnesses on the shipped exe, every one at its baseline: `lodgen_roads` 11/0,
`lodgen_native` 18/0, `lodgen_native_baseline` 0 failures, `lodgen_terrain_vt`
41/1 (baseline 41/1), `lodgen_ground_cover` 29/5 (baseline 29/5),
`lod_generation` 116/0, `lodl_open` 23/0, `lodgen_terrain_pbrm` 14/0, `animws`
236/0 with 1 skip.

### Part B — a hydraulic erosion pass for the far terrain (`--erosion`)

bungo, over a vanilla/ours `_msn` comparison: *"We lose all the fluvial, erosion
features and other topographical features"*. Our LOD terrain is reconstructed
from a 128-unit height grid, and a bake texel is 32 units, so everything finer
than four texels — which is where vanilla's channels and gullies live — simply
does not exist in the input. This pass grows some of it.

* **Off by default, and off is the previous bake's bytes.** `--erosion 0` is the
  built default; with no erosion token in argv the whole tree comes out
  byte-identical to the rung exe's bake, ledger included.
* **It is a delta field, not a heightfield edit.** The droplets run on their own
  lattice at bake resolution and produce a delta that is read in exactly two
  places: the `_msn` sheet as a gradient add, and the colour composite as a
  crevice-form shading just before the grade. The chunk mesh comes out
  byte-identical with the pass on, and so does the `.lodl`.
* **The pass runs in feedback ROUNDS, and that is the fluvial claim.** Droplets
  traced on a field that never changes lay independent scribbles — measured as
  an across/along anisotropy of 0.81 to 0.99 against vanilla's 1.479: the right
  amount of relief pointing nowhere. Round 2's water finds round 1's grooves.
  Inside a round every droplet reads the field as it stood at the round's start
  and writes into a separate accumulator, so visiting order cannot reach a byte:
  1 thread against 16, and one chunk alone against the same chunk inside four,
  are byte-identical with the pass ON.
* **A high pass, for a reason that is not taste.** The local mean of the delta
  over 8 cells is subtracted from it, so the net height change over any patch
  wider than 17 cells is exactly zero. The full-resolution cells are never
  eroded, so anything this pass added at a scale the eye carries across the LOD
  boundary would be a seam. Measured: the pass makes the chunk boundary LESS
  exceptional, not more — the edge step sits at the 72nd percentile of the
  interior steps with the pass on, against the 89th with it off.
* **There is no palette, and the refusal is a measurement.** Lane TILING3 put an
  R-squared ceiling of 0.018–0.023 on any per-texel law from the fine normal to
  vanilla's fine colour: about 98 per cent of vanilla's fine colour is not a
  function of its fine normal. A rock-on-scoured, sediment-on-deposit tint is
  exactly such a law, so there is none. The colour gets a shading only, at the
  crevice term's own fitted coefficient, computed from this pass's own relief —
  and vanilla's crevice term is skipped under `--land-detail-source erosion`, so
  one sheet is never shaded twice from two different surfaces.
* **Five measured defects changed the physics, in this order**, each with the
  number that forced it: world height in the lattice (mean |delta| 13,327 world
  units), runaway droplet speed (452), a cliff handing one droplet hundreds of
  units of capacity (44.4), a pit that a high pass keeps because a one-cell
  spike is what a high pass keeps (26.2), and finally no fluvial alignment at
  all (14.9, and the rounds). Every constant carries its measurement in the
  source.

**The fit, and the row that is red.** Eight of our sheets against the medians of
22 vanilla sheets, measured with the same code that measured vanilla: fine relief
amplitude **-5.6 %** (inside vanilla's own sheet-to-sheet spread of 7.1 %), fine
share of gradient variance **+22.1 %**, across/along anisotropy **-20.2 %**, and
the correlation of fine amplitude with coarse slope **+0.29 against vanilla's
+0.50 — RED**. Vanilla's relief gets stronger where the ground is steeper; ours
does too, with the right sign and a bit over half the strength. The cause is
named in `docs/LODGEN_TERRAIN_VT.md` section 2.5i and the next experiment is
named with it.

**What it costs:** +3.0 s per dim-4 chunk, the whole bake 2.9 times as long. Not
measured at dim 8, 16 or 32.

`--erosion-iterations N` sets the rounds, clamped 1..8 (a round widens the
lattice by 32 cells on every side; round 8 already carries a 264-cell border).
`--erosion-seed <u32>` moves the noise. Every bake with the pass on prints a
census line naming the cells, the moved cells, the mean |delta| in world units
and the largest cut and fill, and the ledger carries the same numbers.
