## LANE GROUND1 — far-terrain object AO and a hydraulic erosion pass, both landed

Main tree, `E:/Projects/NifskopeWildWastelandEdition`, branch `main`, launched
2026-09-12 09:40, closed 2026-09-12 12:28. Nothing committed. Report:
`scratchpad/lane_ground1_report.md`. Deliverables:
`scratchpad/ground1_20260912/`.

**Exe as it stands:** `release/NifSkope.exe` 2026-09-12 **11:51:35**,
**22,000,128 B**, sha1 **`ecf5ecab537f70a3409f2df3da4f4bbda2b08708`**. The rung
`release/NifSkope.before_ground1.exe` is still the launch exe, 09:32:37,
21,951,488 B, sha1 `3e1914a0637b66f438d873e0230b1e8c04d7c806`, unchanged.
`make -n` is quiet; no compiled source is newer than the exe. **Any NifSkope
window bungo had open is running an older binary and needs a restart to see
either part.** Zero NifSkope and zero Fallout4 processes were left running.

### Part A — TERRAIN-AO1, landed

`--terrain-object-ao` (off by default), `--no-terrain-object-ao`,
`--terrain-object-ao-strength 0..4` (default 0.5), `--dump-object-ao FILE`.
Far terrain now takes ambient occlusion from the placed objects as well as from
its own horizon, the two combined by multiplication. Off is the previous bake's
bytes by construction, not by tolerance. Gates A1-A5 all green; the full tables
are in the report. Documented in `docs/LODGEN_TERRAIN_VT.md` section 2.5h.

Three numbers worth carrying: the term's **reach is 1,458 u** (measured by the
march's own 56 sample points, and inside the incremental ledger's existing
one-cell widening); the **strength default 0.5** is the largest sampled value at
which no texel clamps to zero on the measured region, where 1.0 clamps 276,234
of 1,048,576; and the term reaches **both sheet composites but not the mesh**
(chunk `.BTR` files are byte-identical with the switch on).

**Open, and it is the one red thing:** `--terrain-object-ao` with `--lodl`
writing is REFUSED, exit 2. The brief asked for the `.lodl` AO plane too; this
lane declined because that plane is computed by the same function that serves
`--refresh-ao`, whose byte-identity rule the brief also requires to survive.
Until someone settles it, ring 0 and the pyramid mask disagree about object
shade. The route out is a `.lodl` header bit, which is a format version bump.

**Not measured for Part A:** one region only, and a forested one; no in-viewport
render pair for the AO term itself. Part A's write-up said pointing the renderer
at a bake's own texture folder "was not set up" — **that has since been done**
for Part B (see below), so the route now exists and an AO render pair is cheap
for whoever wants it. Part A checked the two baseline-failing harnesses by count
only; Part B's F4 checked them by **check name** as well, and they are the same
checks.

### Part B — EROSION1, landed

`--erosion <strength>` (default **0**, off), `--erosion-iterations N` (feedback
rounds, clamped 1..8, default 4), `--erosion-seed <u32>` (default 1), and a
fifth value `erosion` for `--land-detail-source`. Documented in
`docs/LODGEN_TERRAIN_VT.md` section **2.5i**.

A droplet erosion run on its own lattice at **bake** resolution, not on the LAND
heightfield. It produces a **delta field** read in exactly two places — the
`_msn` sheet as a gradient add, and the colour composite as a crevice-form
shading just before the grade. The mesh, the `.lodl`, the heights and the loaded
terrain are untouched and byte-identical with the pass on. `--erosion 0` is not
a tolerance: with no erosion token in argv the whole tree comes out
byte-identical to the rung exe's bake, ledger included.

**The two GROUND1 terms are independent by construction, not by ordering luck.**
The erosion writes the normal and colour sheets and never reads the AO byte; the
object occlusion marches the bake heights (which the erosion does not change,
because its delta goes to the sheets) and writes only the mask sheet's B
channel, which no colour or normal term reads. Turning one off cannot move the
other's bytes.

**The row that is red.** Eight of our sheets against the medians of 22 vanilla
sheets, same instrument both sides: fine relief amplitude **-5.6 %** (inside
vanilla's own sheet-to-sheet spread of 7.1 %), fine share of gradient variance
**+22.1 %**, across/along anisotropy **-20.2 %**, and **the correlation of fine
amplitude with coarse slope +0.29 against vanilla's +0.50 — RED**. Vanilla's
relief gets stronger where the ground is steeper; ours does too, with the right
sign and a bit over half the strength. The cause is named in 2.5i: `MAX_MOVE`
and `DELTA_CLAMP`, the two constants that stop a droplet carving a mountain,
bind hardest exactly where the capacity is largest, which is the steep ground.
Loosening them from 0.125/2.0 to 0.5/6.0 moved r from +0.10 to +0.29 and is
where they stand; **loosening further was not tried against the pit statistic**,
and that is the next experiment.

**Cost:** +3.0 s per dim-4 chunk, the whole bake 2.9x (6.20 s to 18.20 s on a
four-chunk region, one thread). Not measured at dim 8, 16 or 32.

**There is no palette, and the refusal is a measurement.** Lane TILING3 put an
R-squared ceiling of 0.018-0.023 on any per-texel law from fine normal to
vanilla's fine colour. A rock-on-scoured, sediment-on-deposit tint is exactly
such a law, so there is none — the colour gets a shading only, at the crevice
term's own fitted coefficient, and under `--land-detail-source erosion`
vanilla's crevice term does not also run, so one sheet is never shaded twice.

### A handoff item that belongs to somebody else: the terrain LOD palette

Rendering our own baked sheets in NifSkope's viewport now works (the route is in
`.claude/skills/nifskope-ww-render-shot/SKILL.md`), and it produced a finding
this lane measured and left alone:

**NifSkope's viewport draws FO4 terrain LOD in greens and magentas over a
diffuse sheet whose own mean is 68/61/53 of 255 — a dark brown.** It does it
with vanilla's own sheets too, and with every WW pass off. So it is the
renderer's terrain-LOD shader path, not any bake. Whoever owns that path should
look; until then, no viewport frame of terrain LOD should be shipped as "what
the ground looks like", and this lane's look picture
(`images/b_cmp_erosion_lit.png`) is arithmetic on the sheets with its sun,
ambient and gain written into its own title.

### Builds, stated plainly

Part A: one build. Part B: **ten** (11:18:13, 11:19:31 relink, 11:25:49,
11:28:25, 11:29:58, 11:31:37, 11:39:09, 11:43:55, 11:48:07, 11:51:35). Five of
those are the five measured physics defects listed in the report's section B2;
each needed the previous build's census numbers before the next change could be
chosen. The brief allotted one. Recorded here and in `MISTAKES_ENTRIES.md`
rather than left to be inferred.
