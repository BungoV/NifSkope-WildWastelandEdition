# Lane LAND1 — TILING5 (Part A) + INCR1 (Part B), folded into one lane

Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, nothing committed.
Exe at launch `release/NifSkope.exe` 2026-09-12 06:31:05, 21,819,904 B (ROADS4's DONE exe).
**Exe at the end** 2026-09-12 **09:32:37, 21,951,488 B, sha1
`3e1914a0637b66f438d873e0230b1e8c04d7c806`** -- the fourth of this lane, and
which numbers came off which exe is stated where they are reported (A8, B8.7 and
the `docs/LODGEN_TERRAIN_VT.md` provenance block), never averaged.
Written incrementally; every number carries the floor or the ceiling it is judged against.

---

# Part A -- TILING5

## A0. Pre-registered gates, written 07:12, before the patch was applied

Nothing below had been baked, scored or even compiled when this section was written.
TILING4's frozen split and its instruments are imported UNCHANGED — the same
`pool.json`, the same `h1_sweep.score`, the same `t4_gates.decided`, the same
`f3_full.py` — so this lane cannot re-pick the ground it is graded on.

| gate | what it asks | floor / ceiling |
|---|---|---|
| **A0** | the patch applies with every anchor matched exactly (13 anchors, two of them at BOTH sampling sites); `src/lodgen.cpp`, `src/lodgen.h`, `src/nifcli.cpp` stay LF-only | 13 of 13, 0 CR bytes |
| **A1** | the macro scale is chosen BY MEASUREMENT, by TILING4's own pick rule, applied to this lane's candidates | eligible = swirl 7/7 **and** G2 7/7 **and** G1 inside ±20 % on the SELECTION seven; then rank by repeat-passing sheets, then lowest worst-sheet repeat, then lowest worst-sheet swirl |
| **A2** | does vanilla's `_msn` add anything the heightmap cannot give at these scales | the angular disagreement between the `_msn`'s macro azimuth and the heightmap's, against the floor of the heightmap's own disagreement between two adjacent scales |
| **A3** | **border continuity**: two adjacent dim-4 chunks baked as two separate region bakes vs as one region bake covering both | byte-identical colour sheets, every rule, every swept parameter; floor = a deliberately ring-anchored lattice shown to break it |
| **A4** | **F2 byte identity**: every new switch off == the rung's bytes on all fourteen sheets | `tiling4_20260912/f2_gate.py`, imported unchanged |
| **A5** | the swirl instrument's known-answer controls re-run on THIS lane's exe before any candidate is scored | A1 sinusoid ≈ 1.0; A2 isotropic ≈ its own twin; A3 monotone in strain up to ≈0.75; A4 notch moves 0.0000 over a 16× injected repeat |
| **A6** | **F3** on the fourteen sheets, selection and validation reported SEPARATELY, the product's own bakes | repeat ≤ 0.264 absolute (or the sheet's own control) **and** ratio ≤ 0.448; swirl ≤ 1.20 × that chunk's vanilla and ≤ 3.2846; G1 ±20 %; G2 ±20 % per sheet vs the rung; G2-band ≤ the rung's |
| **A7** | thread-count identity with the winning rule on | 1 chunk thread == 16 chunk threads, byte-identical |
| **A8** | the chain: exe newer than every changed file; the lodgen spells at their baselines; `lodgen_roads.sh` R5 recalibrated to the `--road-detail 1` default and the old and new bar both stated | every moved count explained by name |

**A candidate that trades repeat for swirl or blur is refused by its own numbers**, as
TILING4 refused H3. The default stays plain footprint sampling unless bungo rules.

### The five rules, registered before any of them was measured

| switch | what it does |
|---|---|
| `--land-guide drag:K` | the sample slides DOWNHILL by K × the macro normal's xy. A smooth field, so it has shear, and the shear is what the swirl instrument reads. |
| `--land-guide aspect:K` | the sampling frame is rotated by the downhill azimuth about the macro lattice cell's CENTRE, blended toward identity by the macro slope. Inside a cell the map is a similarity (uniform scale + rotation, zero shear); the price is a discontinuity at the lattice lines. |
| `--land-guide aspecthex:K` | the same rotation carried by the HEX lattice's three taps — each vertex rotates the plane about itself, and the barycentric variance-preserving blend that already joins the three offsets joins the three rotations. Seamless AND shear-free by construction. Needs `--land-hex`. |
| `--land-guide slopewarp:K` | TILING3's hash warp, amplitude × the macro slope's weight. |
| `--land-guide flatwarp:K` | the same, × (1 − that weight). |
| `--land-guide-scale UNITS` | the macro scale, 128..2048, default 1024. Bounded by the ONE-CELL RING, not by taste. |
| `--land-guide-slope TAN` | the reference slope the weight saturates at, default 0.5 (26.6°). |

### Two things registered as REFUTERS, not as results

- The brief says (a) "alone cannot hide the repeat on flat ground — measure that, do
  not assume it". The measurement is the per-sheet repeat on the flattest of the
  fourteen sheets, named by its own macro-slope median, beside the steepest.
- A rotation weighted on the ANGLE crosses `atan2`'s branch cut and turns it into a
  seam of up to 2π×weight. The code weights the MAP instead. The refuter is A3: if the
  weighting were on the angle, a chunk straddling a due-west slope would not be
  byte-identical between the split and the joint bake.

## A0b. The patch, and the one build Part A gets

`scratchpad/land1_20260912/a_patch.py` is a REFUSING patch script: it asserts
every anchor exact-once, `--check` writes nothing, and it measures the line
endings with Python byte counts before and after. It ran clean at **07:12:41**:

```
13 anchors, 13 found (two of them at BOTH sampling sites)
src/lodgen.cpp   523229 bytes  12016 LF  0 CR
src/lodgen.h      71894 bytes   1271 LF  0 CR
src/nifcli.cpp   317886 bytes   7003 LF  0 CR
```

**Gate A0 GREEN** — 13 of 13, 0 CR bytes in all three files.

Both `sampleLtex` sites are patched, not one: with `--vt` on — which is how every
region bake in this lane and in the product is run — the chunk sheet is assembled
from the pyramid's tiles and the stock per-chunk composite is never reached. A
change made at one site only does nothing on disk, which is the mistake TILING2
made. The stock site is patched too so that `--no-vt` and the byte-identity gates
stay honest.

| | |
|---|---|
| build started | 07:15:55 |
| `make -j2` exit code | **0** (gated on make's own `$?`, never on grep's) |
| exe | `release/NifSkope.exe` **2026-09-12 07:17:05, 21,828,608 B**, sha1 `9e2ae9cb255e3ee781e28dcf6d0f8350d71f1f35` |
| exe newer than | `src/lodgen.cpp`, `src/lodgen.h`, `src/nifcli.cpp` — all three, checked one at a time; `find src -newer` returns nothing |
| link-time copies | `res/style.qss` == `release/style.qss` |
| new warnings | **none**. The 14 warnings in the log are all at pre-existing lines (7140/7141 are ROADS4's `materials/Landscape/Ground/*` inside a comment, 121, 3751, 10049, 10885 predate this lane) |
| rung | `release/NifSkope.before_land1.exe` 07:12:36, 21,819,904 B, sha1 `1bb56678d872a612e0136eed54f6506d1b8c306e`, byte-identical to the 06:31:05 launch exe |

The unknown-rule refusal is BY NAME, on stderr, and the bake goes on with the
switch off — measured on the built exe, not asserted:

```
$ NifSkope.exe -no-gui lodgen <esm> --worldspace 3C --land-guide bogus
lodgen: --land-guide bogus is not one of off|drag|aspect|aspecthex|slopewarp|flatwarp; off stands
worldspace 0000003c 'Commonwealth'  cells 36864  grid [-96,-96]..[95,95]
```

## A5. The instrument's known answers, re-run in THIS lane before anything was scored

`scratchpad/land1_20260912/a5_controls.py`, importing `tiling4_20260912/t4_lib.py`
UNCHANGED. Run 07:19:29, before a single candidate had been baked.

| control | reading | wanted | |
|---|---|---|---|
| A5.1 pure sinusoid grating | **1.0000** | ≥ 0.90 | ok |
| A5.2 isotropic noise vs its own phase twin | **0.3041** vs 0.3021 | within 25 % | ok |
| A5.3 swirl vs injected strain (texels) | 0.00→0.1468  0.25→0.1530  0.50→0.1609  0.75→0.1676 | monotone | ok |
| A5.4 a **16×** 10.667-texel repeat injected, WITH the notch | 0.1468 → 0.1468, moved **+0.0000** | ≈ 0 | ok |
| A5.4 the same WITHOUT the notch | 0.1470 → 0.1710, moved **+0.0241** | — | this is what the notch is for |

**Gate A5 GREEN, 4 of 4.** A5.4's second row is the control that gives the first
row a meaning: the instrument *can* see that repeat, and the notch is what stops
it calling the repeat a swirl.

## A4. Every new switch OFF is the rung, byte for byte

`scratchpad/land1_20260912/a4_gate.py`. Both arms are the SAME command line —
`t5_bake.sh`, itself `t4_bake.sh` with only the out-dir changed — with
`--road-detail 1` on both sides and no new switch on either. All fourteen sheets
of TILING4's frozen split, every file of every tile, not just the colour sheet.

| | |
|---|---|
| tiles | 14 of 14 **identical** |
| files compared | **126** (9 or 10 a tile: the colour DDS, `_msn`, `_data`, the BTR, the BTO, the manifest, the two VT pyramid `.lodt`s, `Commonwealth.VT.lodm`) |
| the floor, arm B | one flipped byte → 1 differing, **RED as it must be**; that file removed → 1 only in rung, **RED as it must be** |

**Gate A4 GREEN.** The comparator was shown failing before its green was believed.


## A2. Does vanilla's `_msn` add anything the heightmap cannot?

bungo asked it directly: *"since we're reusing vanilla terain normals and slope
maps, might as well use them to guide this a bit"*. This is the measurement, not
an argument. `scratchpad/land1_20260912/a2_msn.py`, run 07:30:57.

Both azimuths on the same 512x512 grid at the same macro scale L: the heightmap's
is the SHIPPED rule (a Sobel 3x3 at a half-step of L/2 world units over the ring
height grid, read from `--dump-land`'s whole-worldspace VHGT dump, which is the
same data lodgen itself reads); the `_msn`'s is vanilla's own sheet decoded and
box low-passed to the same L. The number is the slope-weighted mean absolute
angle between them, in degrees, over texels where both macro slopes read
tan >= 0.05.

**The floor that gives it a meaning: the heightmap's own disagreement between
two ADJACENT scales, L and 2L.** If the `_msn` disagrees by no more than the
heightmap disagrees with itself one octave away, the `_msn` is carrying no macro
information of its own.

| macro scale | `_msn` vs heightmap | heightmap L vs 2L | |
|---|---|---|---|
| 256 | **10.56 deg** | 6.34 | adds something |
| 512 | **10.02 deg** | 8.34 | adds something |
| **1024** (the default) | **10.38 deg** | 10.83 | **adds nothing** |
| 2048 | **11.40 deg** | 15.02 | **adds nothing** |

**The answer: at the scale this lane steers by, no.** At 1024 and 2048 the `_msn`
sits inside the heightmap's own octave-to-octave uncertainty. At 256 and 512 it
holds about 3 degrees more than the heightmap does — and that residue is the
per-texel relief vanilla baked into the sheet, not macro shape.

Two further reasons the guide is built from the heightmap and not from the `_msn`,
both structural rather than aesthetic:

- the `_msn` is a **per-chunk sheet with edges**; the height grid is a ring with
  one whole cell (4,096 world units) of real data on every side, so a macro
  gradient read from it is continuous across chunk and region borders **by
  construction**. Gate A3 measures that.
- the `_msn` does not exist where vanilla ships none, and the generator must
  work on ground vanilla never baked.

### The measurement that was nearly believed, and what stopped it

The first run of A2 read **90 degrees at every chunk and every scale**. That is
not information, it is the signature of a **convention error** — two azimuths
that disagree by a right angle everywhere are not two opinions about the terrain.
The calibration was then run as a measurement rather than as a reading: all eight
sign/transpose combinations of the decoded `_msn`, against the heightmap, on
(-20,24) at scale 1024, with the heightmap's own octave floor of 9.63 beside them.

```
  rows as-is    ( E, N) 119.47   ( E,-N)  74.70   (-E, N) 105.30   (-E,-N)  60.53
  rows as-is    ( N, E)  88.65   ( N,-E)  80.91   (-N, E)  99.09   (-N,-E)  91.35
  rows flipped  ( E, N) 169.62   ( E,-N)  63.23   (-E, N) 116.77   (-E,-N)  10.38  <-- floor 9.63
  rows flipped  ( N, E)  91.36   ( N,-E)  84.74   (-N, E)  95.26   (-N,-E)  88.64
```

Exactly one collapses to the floor, and it is the one the physics names
independently: a surface normal is `(-dz/dx, -dz/dy, 1)`, so `dz/dx = -R/up` and
`dz/dy = -B/up`, and the sheet's row 0 is NORTH while the height grid's row 0 is
SOUTH. The wrong reading was caught by its own shape before it reached a verdict;
it is in `MISTAKES_ENTRIES.md` as a trap, because a mean near 90 degrees is a
diagnosis and the next lane should not have to rediscover it.

## A1. The sweep, and what it found

Every candidate is a REAL BAKE by the real exe, never a prototype: TILING4
measured its offline prototype off by up to 0.27 on the repeat in BOTH directions,
so this lane made every parameter a CLI switch, built ONCE, and swept with the
product. A dim-4 region bake is about 5 seconds; the selection seven cost about
40 seconds a variant. 119 bakes in the two stages, every one rc=0.

The instruments are TILING4's, imported unchanged (`h1_sweep.score`,
`t4_gates.decided`); the floor for G2 and G2-band is `ls_rung`, this lane's own
rung bake with `--road-detail 1`, not TILING4's (that one was baked by a
pre-ROADS4 exe where the road-detail default was 0).

### Stage 1 — the five rules on their own, macro scale 1024, on the SELECTION seven

| variant | repeat | swirl | G1 | G2 | G2-band |
|---|---|---|---|---|---|
| rung (the floor) | 0/7 | 6/7 | -22.7 % | 7/7 | 7/7 |
| `drag:85` | 0/7 | 6/7 | -21.7 % | 7/7 | 4/7 |
| `drag:171` | 0/7 | 6/7 | -21.7 % | 7/7 | 4/7 |
| `drag:341` | 0/7 | 6/7 | -21.9 % | 7/7 | 3/7 |
| `aspect:0.5` | 0/7 | 6/7 | -25.5 % | 7/7 | 2/7 |
| `aspect:1.0` | 2/7 | 6/7 | -25.9 % | 7/7 | 2/7 |
| `slopewarp:1.0` (`--land-warp 341`) | 0/7 | 6/7 | -22.2 % | 7/7 | 1/7 |
| `flatwarp:1.0` (`--land-warp 341`) | 1/7 | 6/7 | -22.2 % | 7/7 | 1/7 |
| `aspecthex:1.0 --land-hex 256` | **4/7** | 6/7 | -25.6 % | 7/7 | 2/7 |
| `--land-hex 256` alone (no rule) | **4/7** | 6/7 | -20.4 % | 7/7 | 2/7 |

**Stage 1's finding, stated plainly: the HEX LATTICE is what moves the repeat, and
the guide rules are passengers on it.** Every smooth rule on its own scores 0 to
2 of 7; hex alone scores 4 of 7; the rotation carried by the hex lattice scores
the same 4 of 7. That is not the result the brief's ordering of the candidates
expects, and it is the result.

### The registered refuter, measured on the flattest sheet beside the steepest

The brief registered it before anything was baked: rule (a) alone "cannot hide the
repeat on flat ground — measure that, do not assume it." The macro slope of each
sheet was measured, not eyeballed (median tan at scale 1024, from the same VHGT
dump; the full ranking is in `a_slopes.json`):

| | flattest **(-20,20)** tan 0.0760 (4.35 deg) | steepest **(-36,-20)** tan 0.3991 (21.76 deg) |
|---|---|---|
| ceiling | 0.366 | 0.264 |
| rung | 1.278 | 1.501 |
| `drag:85` | 1.245 (−2.6 %) | 1.379 (−8.1 %) |
| `drag:171` | 1.247 (−2.4 %) | 1.299 (−13.5 %) |
| `drag:341` | 1.126 (**−11.9 %**) | 1.010 (**−32.7 %**) |

**REFUTED, with numbers.** Drag does about three times as much work on the steep
sheet as on the flat one — which is what a downhill drag should do — and on
neither does it bring the repeat within a factor of three of the ceiling. Rule (a)
alone is not a candidate for shipping, and it is reported rather than quietly
dropped.

### Stage 2 — every survivor on top of TILING4's own pick, and the macro-scale sweep

The base arm is `--land-sample stochastic --land-hex 256 --land-mip-bias -0.22`,
which is what the product would actually be judged against.

| variant | repeat | swirl | G1 | G2 | G2-band | worst-sheet repeat | worst-sheet swirl |
|---|---|---|---|---|---|---|---|
| rung (the floor) | 0/7 | 6/7 | -22.7 % | 7/7 | **7/7** | 1.278 | 2.223 |
| `stochastic` (TILING4's pick) | 4/7 | 6/7 | -13.9 % | 7/7 | 5/7 | 0.714 | 2.153 |
| + `aspecthex:1.0` scale 1024 | **5/7** | 6/7 | -19.2 % | 7/7 | 4/7 | 0.661 | 2.231 |
| + `aspecthex:0.5` scale 1024 | **5/7** | 6/7 | -18.4 % | 7/7 | **6/7** | 0.676 | 2.185 |
| + `aspecthex:1.0` **scale 256** | **5/7** | 6/7 | -18.6 % | 7/7 | 5/7 | **0.660** | **2.195** |
| + `aspecthex:1.0` scale 512 | **5/7** | 6/7 | -18.9 % | 7/7 | 4/7 | 0.673 | 2.235 |
| + `aspecthex:1.0` scale 2048 | 4/7 | 6/7 | -19.2 % | 7/7 | 5/7 | 0.675 | 2.238 |
| + `flatwarp:1.0 --land-warp 341` | 4/7 | 6/7 | -14.5 % | 7/7 | 4/7 | 0.684 | 2.257 |
| + `drag:171` | **5/7** | 6/7 | -13.9 % | 7/7 | 5/7 | 0.690 | 2.128 |

**The macro scale was chosen by measurement and the measurement says it barely
matters**: 256 → 0.660, 512 → 0.673, 1024 → 0.661, 2048 → 0.675 on the worst
sheet. That spread is 0.015 and it is not monotone, so it is noise, not a trend.
The default stays 1024 and 256 wins the tie-break by 0.001; that is stated as a
tie-break, not as a discovery.

### Gate A1 applied exactly as registered

TILING4's pick rule: eligible = **swirl 7/7 AND G1 inside ±20 % AND G2 7/7**,
then rank by repeat-passing sheets, then lowest worst-sheet repeat, then lowest
worst-sheet swirl.

**No candidate is eligible, and neither is the rung.** Swirl 7/7 is unreachable on
this split: chunk **(-20,20)** reads swirl **2.223 on the RUNG** against its own
vanilla-derived ceiling of **2.021** — the floor fails the gate before this lane
touches anything, so no change to the sampler can produce a 7/7. G1 ±20 %
likewise excludes the rung itself, at -22.7 %.

Applying the ranking to the candidates that pass the two gates that ARE reachable
(G1 inside ±20 %, G2 7/7) gives, in order: `aspecthex:1.0 --land-guide-scale 256`
(5/7, worst repeat 0.660, worst swirl 2.195), then `aspecthex:1.0` at 1024 (5/7,
0.661, 2.231), then `drag:171` (5/7, 0.690, 2.128).

**THE WINNER IS `--land-guide aspecthex:1.0` with `--land-guide-scale 256`, on top
of `--land-hex 256`** — and the size of what it wins is one sheet of seven and
0.054 of worst-sheet repeat over the hex tiling it rides on. That is the honest
size of it.

**The default stays plain footprint sampling.** Nothing in this lane changes what
a bake does unless a switch is given, and gate A4 proves that in bytes.

## A3. Border continuity — the gate this design exists to pass

`scratchpad/land1_20260912/a3_gate.py`, on bakes made by `a3_bake.sh`. The
question is not "do the sheets look joined" but **is a sheet a function of WORLD
position alone, or of the region rectangle the baker happened to be handed?**
Four rectangles, three of which contain chunk (-20,20) and three of which contain
its eastern neighbour (-16,20), so the ring origin is different in every one:

```
  split20   r:-20,20,-17,23     the chunk alone
  split16   r:-16,20,-13,23     the neighbour alone
  joint     r:-20,20,-13,23     both in ONE bake
  joint4    r:-24,20,-13,23     both again from a THIRD origin
```

| rule | chunk | rectangles | verdict |
|---|---|---|---|
| no rule (hex 256 + mip bias) | (-20,20) / (-16,20) | split+joint+joint4 | identical |
| `aspecthex:1.0` | (-20,20) / (-16,20) | split+joint+joint4 | identical |
| `drag:171` | (-20,20) / (-16,20) | split+joint+joint4 | identical |
| `aspect:1.0` | (-20,20) / (-16,20) | split+joint+joint4 | identical |
| `slopewarp:1.0` | (-20,20) / (-16,20) | split+joint+joint4 | identical |
| `flatwarp:1.0` | (-20,20) / (-16,20) | split+joint+joint4 | identical |

**72 file comparisons across the rectangles, 0 RED. Gate A3 GREEN.**

**The floor is not synthetic, and it fired.** A guide that reads nothing would
pass the table above by inaction, so the same chunk was baked at two macro
scales:

```
  --land-guide-scale 1024 vs 512, aspecthex:1.0   1 of 3 files differ
      Commonwealth.4.-20.20.DDS   3528053ff333 vs f797d6325d8b
```

Exactly the right one moved: the COLOUR sheet, and not `_msn` or `_data`. The
guide is demonstrably reading the heightmap, and it is still continuous across
three different region origins.

Why it holds, by construction rather than by luck: the macro gradient reads the
**ring height grid**, which carries one whole cell (4,096 world units) of real
data on every side; every term is a pure function of WORLD position; the ASPECT
lattice anchors on `floor(world / L)`, never on a ring-local coordinate; and
`--land-guide-scale` is REFUSED outside 128..2048 so the Sobel half-step (L/2)
plus the VT tile's 256-unit border can never reach the grid's clamped edge
(the safe macro reach is 3,840 units).

**The second registered refuter, discharged here.** A rotation weighted on the
ANGLE crosses `atan2`'s branch cut and turns a due-west slope into a seam of up
to 2π×weight. The code weights the **MAP** instead — `p + w(R(p) − p) =
((1−w)I + wR)p`, which is a similarity (uniform scale + rotation, **zero shear**)
and is continuous across the cut. If the weighting were on the angle, a chunk
straddling a due-west slope would not be byte-identical between the split and the
joint bake. It is, on all four rectangles and all six rules.

## A7. Thread-count identity

The joint bake `r:-20,20,-13,23` with the winning rule on, at 1 chunk thread
against 16: **15 files compared, 0 differ. Gate A7 GREEN.** The sampler is
deterministic in world position, not in traversal order.

## A6. Gate F3 on all fourteen sheets — NOT MET, and here is by how much

TILING4's `f3_full.py` grading, its instruments imported unchanged, the two sets
reported separately as the frozen split requires. The validation seven were not
opened until the winner had been picked on the selection seven.

### SELECTION seven

| arm | repeat | swirl | G1 | G2 | G2-band |
|---|---|---|---|---|---|
| rung (the floor) | 0/7 | 6/7 | -22.7 % | 7/7 | 7/7 |
| `stochastic` | 4/7 | 6/7 | -13.9 % | 7/7 | 5/7 |
| **the winner** `aspecthex:1.0` scale 256 | **5/7** | 6/7 | -18.6 % | 7/7 | 5/7 |

### VALIDATION seven

| arm | repeat | swirl | G1 | G2 | G2-band |
|---|---|---|---|---|---|
| rung (the floor) | 0/7 | 7/7 | -37.0 % | 7/7 | 7/7 |
| `stochastic` | 5/7 | 7/7 | -31.0 % | 7/7 | 6/7 |
| **the winner** `aspecthex:1.0` scale 256 | **5/7** | 7/7 | -34.1 % | 7/7 | 4/7 |
| `aspecthex:1.0` scale 1024 | 5/7 | 7/7 | -34.1 % | 7/7 | 4/7 |

Per-sheet repeat on the validation seven (ceiling 0.264 on every one):

| chunk | rung | `stochastic` | the winner |
|---|---|---|---|
| (-24,-24) | 1.160 | 0.161 | 0.218 |
| (-12,-20) | 1.344 | **0.287 RED** | **0.308 RED** |
| (4,-24) | 1.349 | **0.321 RED** | **0.314 RED** |
| (20,-24) | 0.916 | 0.205 | **0.152** |
| (-20,4) | 0.961 | 0.173 | 0.166 |
| (-8,4) | 1.038 | 0.132 | 0.150 |
| (12,8) | 0.541 | 0.118 | **0.081** |

**GATE A6 IS NOT MET, on both sets, and the reasons are structural and inherited
rather than caused by this lane:**

- **repeat 5/7 and 5/7, not 7/7.** The two sheets that refuse are the same pair on
  each set, and the winner is not uniformly better than the base arm on them —
  it is worse on (-12,-20) by 0.021 and better on (4,-24) by 0.007.
- **G1 is outside ±20 % on both sets, and so is the RUNG** (-22.7 % selection,
  -37.0 % validation). The floor fails this gate before the sampler is touched,
  so no candidate can pass it. This is TILING4's finding a second time (it read
  -19.9 % / -44.4 %).
- **swirl 6/7 on the selection set is also the rung's failure**: chunk (-20,20)
  reads 2.223 on the rung against its own ceiling of 2.021. On the validation set
  every arm is 7/7.
- **G2-band goes DOWN, 7/7 → 5/7 and 7/7 → 4/7.** This is the one place where the
  winner is measurably worse than the floor, and it is not hidden: a rotated
  sampling frame redistributes energy between the radial bands, which is exactly
  what G2-band was written to notice. `aspecthex:0.5` keeps 6/7 of it for 0.015
  of worst-sheet repeat, and that is the trade if bungo wants it.

**A candidate that trades repeat for swirl or blur is refused by its own numbers**,
as this lane registered before it measured anything. The winner does not trade
swirl (6/7 and 7/7, same as the rung) and does not trade grain (G2 7/7 on both
sets); it trades **G2-band**, and that is stated as the price rather than left
out of the table.

**Consequence: the switch ships, the DEFAULT DOES NOT CHANGE.** `--land-guide`
defaults to `off`, which gate A4 proves is the rung's bytes on all fourteen
sheets. Whether the switch is worth turning on is bungo's call over the pictures,
not a verdict this lane is entitled to reach from a 5-of-7.

## The pictures

Both under `scratchpad/land1_20260912/images/`, made by `a_pics.py` (its output
is `logs/a_pics.txt`), every panel a real DDS off disk written by
`release/NifSkope.exe`, every bake carrying `--road-detail 1`. Nothing in either
picture is modelled.

Six panels each, the same ground in all six: **vanilla**, **plain** (the rung —
`--road-detail 1` and nothing else, which is what still ships), **best of (a)**
`drag:341`, **best of (b)** `aspecthex:1.0 --land-guide-scale 256` on the hex
tiling (the winner), **best of (c)** `flatwarp:1.0 --land-warp 341` on the hex
tiling, and **the 683-unit warp** — TILING3's shipped preset, the one bungo
called too strong. The window is `warp_sweep.py`'s own (x 300, y 40, 128 texels,
3:1 nearest neighbour) so it lines up with the picture he has already looked at,
and the WHOLE 512-texel sheet sits at 1:1 underneath each crop, because a swirl
is a 30-to-100-texel feature and a 128-texel crop can hide one. Each panel
carries its own repeat / grain / swirl numbers and its ratio to vanilla's.

| file | size | chunk | why this chunk |
|---|---|---|---|
| `a_land_guide_flat.png` | 3156 x 1158 | **(-20,20)** | the FLATTEST of the selection seven, macro tan 0.0760 (4.35 deg) |
| `a_land_guide_slope.png` | 3156 x 1158 | **(20,-24)** | the STEEPEST of the validation seven, macro tan 0.3467 (19.12 deg) |

The pair was chosen by the measured macro slope, not by eye, so together they
answer the question a guide rule has to answer: what does it do where there is no
slope to steer by, and what does it do where there is a lot.

What the panels read (repeat / grain / swirl r, straight off `logs/a_pics.txt`):

| panel | flat (-20,20) | steep (20,-24) |
|---|---|---|
| vanilla — the target | **0.032** / 5.459 / 1.684 | **0.065** / 2.140 / 2.397 |
| plain, the rung | 1.278 / 4.790 / 2.223 | 0.916 / 2.650 / 0.977 |
| (a) `drag:341` | 1.126 / 4.815 / 2.177 | 0.477 / 2.688 / 1.039 |
| **(b) the winner** | **0.660** / 4.921 / 2.195 | **0.152** / 2.792 / 1.571 |
| (c) `flatwarp:1.0` | 0.684 / 5.017 / 2.257 | 0.225 / 2.944 / 1.247 |
| the 683-unit warp | 0.693 / **5.789** / 2.285 | 0.280 / **3.890** / 2.282 |

Three things these pictures say that the gate tables do not:

- **On the steep sheet the winner beats the 683-unit warp on BOTH axes at once**
  — repeat 0.152 against 0.280, at a grain of 2.792 against the warp's 3.890
  where vanilla's is 2.140. **The warp's grain is 82 % above vanilla's**, and
  that over-sharpening is what bungo was looking at when he said the warp is too
  strong. The winner is 30 % above, which is the same direction and a third of
  the distance.
- **On the flat sheet nothing gets near vanilla.** Vanilla reads 0.032 and the
  best rule reads 0.660 — a factor of twenty. That is the registered refuter
  holding in a picture: with no macro slope there is nothing for a
  terrain-guided rule to steer by, and what is left doing the work is the hex
  lattice, not the guide.
- **The swirl number moves in opposite directions on the two sheets** (flat
  2.223 -> 2.195, steep 0.977 -> 1.571), which is why swirl is reported per sheet
  and never pooled.

## A8. The harness chain on this exe

`release/NifSkope.exe` 2026-09-12 **07:42:22**, 21,861,376 bytes, sha1
`902223bd99dba4bfaf5d621fe36eb12fc4cf0272`. Run by `a8_chain.sh` at 07:44:09,
finished 07:51:15, through MSYS2 with the interpreter NAMED
(`C:/Users/bungo/.../Python39/python.exe`) — MSYS2's own `python` has no numpy
and a missing module reads exactly like a render regression.

| harness | this exe | expected | segfaults | verdict |
|---|---|---|---|---|
| `lodgen_terrain` | **26 / 0** | 26 / 0 | 0 | as expected |
| `lod_generation` | **116 / 0** (floor 116) | 116 / 0 | 0 | as expected |
| `lodgen_terrain_vt` | **41 / 1** | 41 / 1 (V9c) | 0 | as expected, inherited |
| `lodgen_ground_cover` | **29 / 5** | 29 / 5 | 0 | as expected, inherited |
| `lodgen_terrain_pbrm` | **14 / 0** | 14 / 0 | 0 | as expected |
| `lodgen_native` | **18 / 0** | green | 0 | as expected |
| `lodgen_roads` | **11 / 1 -> 11 / 0** | the inherited red | 0 | **FIXED, see below** |
| `lodl_open` | **23 / 0** | 23 / 2 with six segfaults | **0** | **not reproduced on this exe** |
| `animws` | **224 / 0 / 1 skip** | 224 / 0 / 2 skips | 0 | green, one skip fewer |

Three rows need a sentence each, and none of the three is mine to fix:

- **`lodl_open` reads 23 / 0 with ZERO segfaults on this exe.** The director's
  note said 23/2 with six segfaults on the 05:48:33 exe and 23/0 on an exe whose
  lodgen objects were rebuilt after ROADS4's `lodgen.h` edit. This exe is the
  second kind — its lodgen objects were rebuilt at 07:17:05 by my own build,
  which touched `lodgen.h` — and it reads the clean number. **Measured, not
  fixed**: nothing in this lane addressed a segfault, so the reading is evidence
  about which objects were stale, not evidence of a repair. UINOTES1's defect
  belongs to lane UINOTES2 and the row is reported for it.
- **`animws` is 224 checks / 0 failures / 1 skip**, against the director's
  expected 224 / 0 / **2** skips. The counts and the failure count match exactly;
  the skip count is one lower. The skip present is
  `10mmPistol.nif has no NiControllerSequence to test with`. I did not
  investigate and did not touch the file — it is UINOTES2's — but the difference
  is stated rather than rounded to "as expected".
- **`lodgen_terrain_vt` 41/1 and `lodgen_ground_cover` 29/5** are the inherited
  reds the brief named, unchanged in both count and failure by this lane's work.

**The exe-newer-than-sources sweep is clean against UINOTES2's three copied-in
files** (`src/animworkspace.cpp`, `src/animworkspacetest.cpp`,
`tests/spells/animws.sh`) — and that sweep was NOT taken on trust. See the
mistakes entry: the sweep passed at 07:17:05 while the exe was genuinely stale,
because the copied sources carry PRESERVED older mtimes (06:43 and 06:31) and
their objects were older still (04:35 and 05:48). The tell was the build log's
compile lines, not the timestamps. A counted relink at **07:42:22** fixed it, and
that relink was then PROVEN NEUTRAL on lodgen's output: `z_relink_off` and
`z_relink_win` re-bakes are `diff -r --brief` IDENTICAL to the 07:17:05 exe's
`ls_off` and `g_ahs_s256`. Part A's sweep numbers therefore stand on an exe whose
relink changed nothing they measure.

## A9. The inherited red: R5 recalibrated to the `--road-detail 1` default

**OLD BAR 0.3223. NEW BAR 0.2901. `after` is 0.3078 and now passes by 0.0177.**
`tests/spells/lodgen_roads.sh` goes **11 / 1 to 11 / 0**, re-run at 07:54 on this
exe.

**The method IS recorded**, contrary to what the brief allowed for — it is in the
docstrings of both `tests/spells/lodgen_roads_metric.py` and
`scratchpad/roads1_20260911/road_metric.py`:

```
PASS = after >= 2 x floor  AND  after >= 0.8 x reference
```

So **0.3223 was never a stored constant.** It is 0.8 times the SAME BAKE's own
non-road background agreement, recomputed live every run. The method already
self-adapts to anything that moves the pipeline — and it still failed, which is
the interesting part: the thing it cannot follow by itself is a change to the
DEFAULT it was calibrated against.

**Measured on MY exe, not quoted from ROADS4** (`a9_r5.sh`, four bakes of the
harness's own chunk (-20,20), `logs/a9_r5.txt`):

| | `after` | `reference` | ratio | old bar 0.8 x ref | verdict then |
|---|---|---|---|---|---|
| `--road-detail 0` (what the 0.8 was calibrated on) | 0.3435 | 0.4039 | **0.8505** | 0.3231 | passed by 0.0204 |
| `--road-detail 1` (bungo's default since ROADS4) | 0.3078 | 0.4029 | **0.7640** | 0.3223 | **failed by 0.0145** |

The floor (0.1354) and the ceiling (1.0000) are identical in both, as they must
be — the floor is a `--no-roads` bake and detail does not reach it. **Not one
line of road code differs between those two rows.** Land detail is added to the
road texels as well as to the ground and it costs the ROAD more, because the road
is the flat, low-variance population: centreline colour error goes 22.34 to
23.64 while the whole-tile error goes 20.80 to 20.95.

**How the new margin was derived, and the honest note about the old one.** The
0.8 has **no recorded derivation** — ROADS1 pre-registered it as a margin and
said so, and I could not find one. What CAN be recovered is the headroom it
expressed, which is the part worth preserving: `0.8 / 0.8505 = 0.9407`, so the
bar sat at **94.07 % of what the shipped default itself achieved**. Carrying that
same headroom onto today's shipped default gives `0.9407 x 0.7640 = 0.7187`, and
**0.72** is that to the two decimals the old margin was written in, rounded UP —
the tighter of the two neighbours.

**A margin loosened until it passes is worthless, so here are the three checks
that it still binds**, all in the metric's own docstring so the next lane does
not have to take my word:

- **bar 2 still decides this row.** 0.2901 is ABOVE bar 1 (2 x floor = 0.2708),
  so bar 2 has not been quietly demoted under the other bar and left for show.
- **it still refuses the null.** The `--no-roads` floor's own ratio is
  `0.1354 / 0.4029 = 0.336`, less than half of 0.72.
- **the guard is tight.** Today's default clears it by 0.0177, so any regression
  that loses more than **5.8 %** of the centreline agreement fails this row.

And it is not a one-way ratchet: at `--road-detail 0` the recalibrated bar reads
`0.3435 >= 0.2908 ok`, so the old configuration still passes too. The constant
now lives as a named `MARG = 0.72` with the whole derivation above it, rather
than as a bare `0.8` repeated in three places.

## A10. Finished-work review against the skills this part was told to use

Each skill is answered with what it demanded, what was done, and -- where they
differ -- the gap, named rather than smoothed.

**`ww-prototype-is-not-the-product`.** The demand is that nothing is reported on
a stand-in for the thing that ships. Met: all 119 sweep bakes and every gate bake
were made by `release/NifSkope.exe` itself, writing real `.DDS` sheets to disk,
and every panel of both pictures is one of those files read back -- no synthetic
sheet, no modelled rule, no scorer output standing in for a bake. The switch
ships in the same exe that was measured.

**`ww-control-calibration`.** The demand is that an instrument is shown to give
the known answer before it is trusted on an unknown one. Met and it mattered:
lane TILING4's scorer was imported UNCHANGED and re-run in this lane on its own
known cases (A5) before a single candidate was scored, and the `_msn` comparison
was run against **the heightmap's own octave floor** (9.63 degrees) rather than
against zero -- which is the only reason the 10.38-degree result could be read as
"carries nothing new" instead of as a disagreement. The convention sweep in A2 is
the same principle applied to a suspicious constant: a mean of exactly 90 degrees
is a transposed axis, not a measurement.

**`ww-spec-gate-audit`.** The demand is that gates are registered before
candidates are scored and that the validation set is not opened early. Met: the
gates are written at A0 with a 07:12 timestamp, ahead of the patch at A0b, and
the seven validation sheets were not opened until the winner had been chosen on
the other seven. **The audit's verdict is a failure and is reported as one**:
gate F3 is NOT MET on either set (repeat 5/7 and 5/7 against a 7/7 bar), and two
of the three failing rows are the rung's own -- no change to the sampler can pass
a gate whose own floor fails. That is why the switch ships `off`.

**`ww-texel-picture`.** The demand is real texels, a stated window, and a
comparison a person can check. Met: both pictures use the window
`warp_sweep.py` already uses (x0=300, y0=40, 128 texels, 3:1 nearest neighbour)
so they line up with a picture bungo has already looked at, with the whole sheet
at 1:1 underneath because a swirl is a 30-100 texel feature that a 128-texel crop
can hide. Vanilla's own sheet is one of the six panels, and the two sheets were
chosen by **measured macro slope** (the flattest of the selection seven and the
steepest of the validation seven), not by eye.

**`nifskope-ww-build-verify`.** The demand is that a harness verdict is read next
to the exe that produced it, and that the exe's objects are checked rather than
`exe -nt src`. Met: A8 stamps the exe's timestamp, size and sha1 beside every
harness line, and the object timestamps were read. **Gap: the chain was run with
the interpreter named** precisely because MSYS2's python has no numpy and an
empty number reads like a regression -- the ROADS1 trap, met and avoided.

**`nifskope-ww-lodgen`.** Met on the mechanical rules: `--road-detail 1` on every
bake and every picture; region bakes only; no UI file touched; nothing committed;
patch scripts written with the Write tool because heredocs eat backslashes and
apostrophes (met four times, and broken once anyway -- see the Qt6 entry in
`MISTAKES_ENTRIES.md`).

---

# Part B -- INCR1

## B1. The dependency map, written BEFORE the code

The brief is explicit that this comes first, and the reason is the whole risk of
the lane: an incremental bake is only worth having if it is **byte-identical to
the full bake it replaces**, and the only way to be sure of that is to know, in
advance and in writing, every input each output actually reads. A dependency map
written after the code is a description of the code's bugs.

This map was read out of `src/lodgen.cpp`, `src/lodgen.h`, `src/nifcli.cpp` and
`src/lodgenchunkpass.h` at the anchors named. Nothing in it is assumed from the
flag names.

### B1.1 What one chunk's outputs read

A chunk job is `LodgenChunkJob{ dim, cx, cy }` and covers `dim x dim` cells from
`(cx,cy)`. The pass is `lodgenRunChunkPass( jobs, ... )`, fanned over
`lodgenThreadCount()` workers and **retired on the calling thread in JOB ORDER**
(`nifcli.cpp:3584-3623`) -- which is the property the whole of Part B rests on,
because a filtered job list is still in job order.

| output | what it reads | reach beyond the chunk's own cells |
|---|---|---|
| `<ws>.4.cx.cy.btr` (terrain mesh) | every covered cell's `LAND` VHGT, VNML, VCLR, VTXT and the cell's `LTEX` rows | **1 cell on every side** -- `LODGEN_TERRAIN_RING_CELLS = 1`, so the seam normals and the skirt are continuous |
| `<ws>.4.cx.cy.bto` (objects) | every covered cell's placed `REFR`s, their `STAT`/`SCOL` bases, the models and materials those name, through the resource stack | **1 cell on every side** for the AO skirt (`aoSkirtCells = 1`, `lodgen.h:540`), plus `cullMargin = 128.0f` units of geometry margin (`lodgen.h:572`), which is inside the cell |
| `<ws>.4.cx.cy.DDS` + `_msn` + `_data` (the colour sheets) | **with `--vt` on, NOT baked per chunk at all** -- assembled from the pyramid's staging (`LodgenVtOptions::btrTexDir`, `lodgen.h:1117-1120`). Without `--vt`, the stock composite reads the same cells' `LTEX` and the land textures | see B1.2; without `--vt`, the land-guide macro reach of `--land-guide-scale / 2` <= 1024 units and the tiling footprint |
| the vanilla reuse (`--land-detail-source vanilla`) | Bethesda's shipped sheet for **this chunk only**, as a loose file under `--vanilla-lod-root` | none -- it is a per-chunk file copy |
| `<chunk>.bto.manifest.txt` | nothing new; it is a printout of the `.bto` build | the `.bto`'s |

**The widening rule that falls out of this table**: the largest per-chunk reach
is **one cell** (4,096 world units), from the terrain ring and the AO skirt
independently. The land-guide macro gradient reaches at most
`--land-guide-scale / 2 = 1,024` units and the VT tile border 256 units, both
well inside it. So:

> **A changed cell dirties its own chunk and every chunk within ONE CELL of it.**

At `--dim 4` that means a change in a chunk's interior dirties one chunk, and a
change in a cell on a chunk's edge dirties two (or four at a corner). The
widening is by CELL, not by chunk, and it is applied before the queue is built.

### B1.2 What is NOT per chunk, and therefore what incremental cannot quietly skip

This is the part that decides whether the feature is honest, so it is stated
before the code rather than discovered by a failing gate.

| stage | when it runs | why a filtered chunk list breaks it |
|---|---|---|
| **the VT pyramid** (`lodgenBakeTerrainVt`, `lodgen.cpp:9960`) | **BEFORE the chunk pass**, over the whole region at once (`nifcli.cpp:3560`) | the pyramid is per WORLDSPACE by construction (`lodgen.h:1121-1126`); its tiles do not correspond to chunks, and with `--vt` the chunk sheets are ASSEMBLED from its staging. Skipping chunks does not skip pyramid work, and skipping pyramid work changes the sheets |
| **the atlas** (`--atlas`) | after the chunk pass | consumes `writtenBto` **in list order** |
| **the texture arrays** (`--arrays`) | after the chunk pass | consumes `writtenBto` **in list order** |
| **the merge** | after the chunk pass | consumes `writtenBto` **in list order** |
| **the far-ring simplify** (`lodgenSimplifyFarRings`) | LAST | consumes `writtenBto` **in list order** |

A shortened `writtenBto` is a **different input** to all four. An incremental run
that filtered the job list and then let these stages run would produce an atlas
built from four chunks instead of sixteen -- and it would not crash, it would
quietly ship a wrong atlas. **This is exactly the class of bug the byte-identity
gate exists to catch, and the design refuses it up front instead**: see B1.4.

### B1.3 What the ledger has to record, item by item

An input digest has to change when and only when the output would. Recording
too little silently skips a rebake that was needed; recording too much rebakes
everything and the feature is worthless. Per chunk:

| field | why it is in the ledger | what it would miss if left out |
|---|---|---|
| `cells` | the covered cells plus the one-cell ring, each with a digest of its `LAND` and `LTEX` rows | a height edit, a texture-paint edit, a vertex-colour edit |
| `refs` | the form IDs, base IDs, positions, rotations, scales of every LOD-bearing `REFR` in those cells, in a canonical order | a moved, added, deleted or rescaled object |
| `assets` | the models, materials and textures those refs resolved to, each with the **path AND the bytes' digest AND which resource-stack entry supplied it** | a mesh edited in place; a loose file that started overriding an archived one |
| `switches` | every switch that reaches the bake, as a canonical string | the operator changing `--land-guide` and getting yesterday's sheets |
| `outputs` | each written file's path and digest | a hand-edited or half-written output |
| `vanilla` | the vanilla sheet's digest under `--vanilla-lod-root`, when the reuse is on | a change to the vanilla corpus |

**`switches` is the one most easily got wrong and it is the one that matters
most**, because it is global: if any switch that reaches a sheet changes, EVERY
chunk is dirty, and there is no partial answer. The ledger therefore carries a
single whole-run switch digest at file level, not per chunk, and a mismatch
there is a **refusal to be incremental at all**, by name.

### B1.4 Refusal to full-bake, by name, and why that is the design and not a gap

The brief asks for refusal by name. The map above says what has to be refused:

1. **no ledger, or a ledger from a different worldspace / dim / region** -- there
   is nothing to diff against;
2. **the switch digest differs** -- every chunk is dirty, so an incremental run
   is a full run with extra bookkeeping;
3. **a whole-region consumer is requested** (`--atlas`, `--arrays`, the merge,
   `--impostors`) -- a filtered `writtenBto` would corrupt it;
4. **an output named in the ledger is missing or its digest does not match** --
   the tree is not the tree the ledger describes.

In every one of those cases `--incremental` **prints the reason and exits
non-zero WITHOUT baking**. It does not silently promote itself to a full bake:
an operator who typed `--incremental` and got a 40-minute full bake with no
explanation has been lied to, and an operator whose incremental run quietly did
half the work has been lied to worse. The way forward is always printed in the
same message.

**`--vt` is the interesting case and it is NOT refused.** The pyramid runs over
the region either way and is deterministic in world position, so re-running it
costs time but cannot change bytes; what `--incremental` saves there is the
per-chunk mesh and object work, which is the expensive half. That claim is a
measurement, not an argument, and B5 measures it.

### B1.5 The one refuter registered against this whole design

**If the ledger's digests are computed from anything the bake does not actually
read, or miss anything it does, the byte-identity gate passes by luck on the
edits I happened to choose and fails on bungo's.** The guard is that the gate's
edit kinds are chosen to hit DIFFERENT rows of the B1.3 table -- a height
(`cells`), a moved object (`refs`), an edit on the region's own border, and a
deleted output -- and that every arm carries a FLOOR: `incr == full` is
trivially true for an edit that reached nothing, so each arm separately asserts
that the edit moved the full bake's bytes at all, and an arm whose floor is
empty is printed VACUOUS, never PASS. A gate that cannot fail is not a gate.

> **What this section claimed and did not deliver, stated plainly.** B1 promised
> a floor arm built by halving the AO reach in the digest and watching the gate
> go red. That arm was NOT built: it needs a second exe compiled with a
> deliberately wrong constant, and this lane had one build slot for Part B. The
> refuter is therefore **only partly retired**. What stands in its place is
> weaker and is not pretended otherwise: the per-arm floor above (which proves
> each edit reached the output), the census line beside every verdict (which
> proves the incremental run did not secretly do a full bake), and the fact that
> the gate caught two real defects this lane would otherwise have shipped -- see
> B3. **The `assets` row of the dependency map has no gate arm at all** and is
> listed under "what was not measured".

### B1.6 CORRECTION -- two claims above did not survive the code

B1 was written before the implementation, deliberately. Two of its statements
are wrong and are corrected here rather than edited out of the text above.

1. **The refusal list in B1.4 names the merge. The merge is not refused.**
   `lodgenMergeChunkShapes` and `lodgenSimplifyFarRings` are both
   `for ( path : btoPaths )` loops that open one `.BTO`, rewrite it and save it
   with no state carried between files, so a filtered list gives each rebaked
   chunk exactly the treatment a full run would. The merge is **on by default**,
   so shipping that list would have made `--incremental` refuse every command
   anybody would ever type -- and it did, until gate B3's first arm failed. Only
   `--atlas`, `--arrays` and `--impostors` are refused. Full write-up in
   `MISTAKES_ENTRIES.md`.

2. **B1.4 item 4 calls a missing output a refusal. It is not.** A file the
   ledger claims that is absent or edited marks that **chunk** dirty and it
   rebakes; the run proceeds. Refusing there would mean a user who deleted one
   `.BTO` could never use the fast path again. Gate arm `A/lost` measures
   exactly this: one deleted `.BTO`, `1 output lost, 5 by neighbour`, 6 of 25
   dirty, and the file comes back byte for byte.

## B2 The refusals -- gate `b2_refusals.sh`, RESULT PASS, 5 arms, 0 failures

A refusal is worth something only if it (1) names itself, (2) exits non-zero and
(3) **bakes nothing**. The third is the one a reader takes on trust and the one
that matters: a refusal that had already written half a chunk would leave an
output tree nobody could reason about. So every arm runs against an empty
out-dir and asserts it is still empty afterwards.

| arm | rc | files written | the sentence it printed |
|---|---|---|---|
| `no-ledger` | 1 | **0** | `--incremental has nothing to diff against -- no ledger at <dir>/Commonwealth.lodb` ... `run the same command once WITHOUT --incremental; every bake writes the ledger` |
| `wrong-shape` | 1 | **0** | `<ledger> describes ... and this run is a different shape` ... `an incremental run must cover exactly the region its ledger covers` |
| `switches` | 1 | **0** | `the switches differ from the ones the ledger was written with, so EVERY chunk is dirty and an incremental run would be a full run with extra bookkeeping` |
| `whole-region` | 1 | **0** | `--atlas, --arrays and --impostors each build ONE region-wide product out of the whole written .BTO list` ... `do them in a separate full pass` |
| `merge-ok` **(negative control)** | **0** | 149 | `incremental: 0 of 25 chunks dirty (0 inputs moved, 0 not in the ledger, 0 output lost, 0 by neighbour)` |

The fifth arm is the one that makes the other four mean anything. The merge is
**on by default**, so without a control proving the default command *runs*, all
four refusal arms would pass just as happily on a build that refused everything
-- which is exactly the build this lane had at 08:30. That is not hypothetical:
see `MISTAKES_ENTRIES.md`.

### B2.1 Two ways this gate was passing and failing for the wrong reasons

Both were caught by one cheap habit -- **printing the refusal sentence beside
every verdict instead of only PASS/FAIL** -- and both are worth more than the
verdict they corrected.

1. **Three arms failed on a stale region.** `REG` was still the 3x3 shape from
   before gate B3's regions were enlarged to 5x5, so every arm tripped the
   *wrong-shape* refusal before reaching the refusal it was testing. The run
   read `5 arms, 3 failures`, which looks like a broken feature and was a broken
   gate.
2. **The `whole-region` refusal was UNREACHABLE, and the arm was passing
   anyway** -- first on the wrong-shape message, then on the switches message.
   `--atlas` is not on the switch-digest skip list (correctly: it changes the
   output), so asking for `--incremental --atlas` against a ledger baked without
   `--atlas` refuses for the *switch* reason and the whole-region check is never
   reached. The arm now bakes a ledger **with** `--atlas` first -- which is the
   path a person would actually walk -- so the digests agree and the only thing
   standing between the run and a quarter-sized atlas is the refusal under test.

An arm that passes for a reason it did not ask about is not evidence. Two of the
five were doing that, and the gate said `PASS` for one of them.

---

## B3 Byte identity -- `b3_identity.sh`, 8 arms, 8 PASS, 0 FAIL

The promise is **the same bytes, not nearly the same bytes**. Every arm bakes
the same edited plugin twice -- once in full, once incrementally against the
previous ledger -- and compares the two output trees file by file and byte by
byte, **including `Commonwealth.lodb` itself**, with no exceptions (an exception
is where a bug would live).

Two regions of 5x5 chunks at dim 4, both with `--cover --roads --road-detail 1`:

* **region A** `-24 16 -5 35`, land guide **off** (the shipping default);
* **region B** `-16 0 3 19`, with Part A's winner **on**
  (`--land-guide aspecthex:1.0 --land-guide-scale 256`) -- so the gate covers
  both switch states of this lane's other feature.

| arm | dirty | inputs moved | output lost | by neighbour | floor: files the edit moved | verdict |
|---|---|---|---|---|---|---|
| `A/null` | 0 of 25 | 0 | 0 | 0 | 0 *(required: control)* | **PASS** |
| `A/land` | 9 of 25 | 1 | 0 | 8 | 5 (`.BTO`, `.BTR`, `.lodb`, 2 sheets) | **PASS** |
| `A/refs` | 9 of 25 | 1 | 0 | 8 | 3 (`.BTO`, manifest, `.lodb`) | **PASS** |
| `A/border` | 16 of 25 | 4 | 0 | 12 | 9 | **PASS** |
| `A/lost` | 6 of 25 | 0 | **1** | 5 | 0 *(required: inputs untouched)* | **PASS** |
| `B/null` | 0 of 25 | 0 | 0 | 0 | 0 *(required: control)* | **PASS** |
| `B/land` | 9 of 25 | 1 | 0 | 8 | 5 | **PASS** |
| `B/refs` | 9 of 25 | 1 | 0 | 8 | 3 (`.BTO`, manifest, `.lodb`) | **PASS** |

The edits are made by `b_esmedit.py` **in a real copy of `Fallout4.esm`**, not
by a loose-file override -- rows 1, 2, 3, 5 and 7 of the dependency map live in
the ESM and nothing else can reach them. `land` raises a gradient byte of a
**zlib-compressed** `LAND` record (37,019 of 37,020 are compressed) and fixes up
the GRUP size chain; `refs` moves a `REFR` 512 units in X; `border` edits four
cells on the region's own outer edge; `lost` deletes one finished `.BTO` from
the tree.

### B3.1 Every arm carries a floor, and one arm's floor was a lie

`incr == full` is **trivially true** for an edit that reached nothing, so each
arm separately asserts the edit moved the full bake's bytes at all, and an arm
whose floor is empty prints **VACUOUS**, never PASS.

That mechanism fired. The first run's two `refs` arms passed with a floor of
**exactly one file: `Commonwealth.lodb` itself**. The moved reference was not
drawn in LOD -- most are not, because an empty MNAM slot drops a ref at every
ring -- so the only thing the edit moved was the input digest, which is the
thing under test. **An arm whose only witness is the artefact being tested is
not a witness**, and it would have been reported as a pass by any reading of the
verdict line.

The fix was to move a reference the bake **demonstrably draws**: `b_pickref.py`
intersects the base bake's own `.BTO.manifest.txt` -- the list of references
that actually reached a chunk -- with the plugin's REFRs in an interior cell, and
`b_esmedit.py moveid` moves that form id. Re-run, both arms now move a real
`.BTO` and its manifest, and still come back byte-identical. The weak plugins are
kept beside the good ones as `*_refs_weak.esm`.

The `null` and `lost` arms invert the floor on purpose: `null` changes nothing,
so a full bake that *moved* would be a **BROKEN CONTROL**; `lost` deletes an
output without touching an input, so the full bake must also be unmoved and what
is on trial is whether the missing file comes back byte for byte. For `lost` the
non-vacuity is carried by the census line -- `1 output lost, 5 by neighbour` --
not by the file diff, and that is said here because the printed floor of `0`
would otherwise read as a vacuous arm.

### B3.2 The census line is what separates "it worked" from "it cheated"

Printed every run, beside every verdict:

```
incremental: 9 of 25 chunks dirty (1 inputs moved, 0 not in the ledger, 0 output lost, 8 by neighbour)
  (-20,20) inputs 0f0440cde767 -> 7bf6b0e2c603
```

`25 of 25` would mean the run is a full bake with extra bookkeeping -- still
correct, still byte-identical, and **not what you asked for**. Byte identity
alone cannot tell those apart; the census can, and it is how this lane found the
defect where the ledger's output digests were taken **before** the merge rewrote
every `.BTO`, so every subsequent run reported all its outputs "lost" and
silently rebaked everything while passing byte identity.

---

## B4 The ledger is deterministic -- `b4_ledger.py`, RESULT PASS, 0 failures

The whole gate rests on this: if the ledger carried a timestamp, a thread id or
an absolute path, B3 would have had to *except* it from the comparison.

* **Two independent full bakes of the same tree, minutes apart, write a
  byte-identical ledger** -- region A 13,402 B, region B 13,341 B. This is B3's
  null arm doing double duty and it is the load-bearing evidence.
* Header checks: `LODB` magic, version 1, `jsonLen + 16 == file size`, reserved
  word zero.
* **Chunk rows sorted by `(cy, cx)`**, not by the order the pass retired them --
  which is thread order and would differ run to run. 25 rows each.
* **Every output path relative**, none absolute, none escaping the ledger's own
  directory except the one documented `../tex/` hop, so a mod folder can be
  moved or copied without invalidating it. 148 output rows for region A, 150 for
  region B.
* No field whose name contains `time`, `date` or `thread`.

A check that fell out of the counts and is worth keeping: region A wrote **149
files and 148 output rows**, region B **151 and 150**. The difference is exactly
one -- the ledger, which is not its own output -- so **every file each bake
wrote is named in the ledger**. Nothing is tracked that was not produced and
nothing produced is untracked.

---

## B5 What it costs -- `b5_times.py` over the gate's own logs

No extra bakes were made for this table: gate B3 already ran a full bake and a
dirty rebake of the same region for every arm.

| arm | full (stage s) | incremental (stage s) | chunks | saved |
|---|---|---|---|---|
| A/base | 29.3 | -- | 25 rows | *(the full bake)* |
| A/null | 24.5 | 0.0 | 0 of 25 | +24.5 s (100 %) |
| A/land | 24.7 | 12.7 | 9 of 25 | +12.0 s (49 %) |
| A/refs | 29.2 | 11.0 | 9 of 25 | +18.2 s (62 %) |
| A/border | 24.0 | 19.1 | 16 of 25 | +4.9 s (20 %) |
| A/lost | 24.3 | 9.5 | 6 of 25 | +14.8 s (61 %) |
| B/base | 78.4 | -- | 25 rows | *(the full bake)* |
| B/null | 45.2 | 0.0 | 0 of 25 | +45.2 s (100 %) |
| B/land | 46.7 | 15.9 | 9 of 25 | +30.8 s (66 %) |
| B/refs | 54.8 | 18.6 | 9 of 25 | +36.2 s (66 %) |

**Read the column heading.** Those are *stage* seconds -- landscape, meshes,
textures, impostors -- and they do **not** include reading the plugin, indexing
the archives, the merge, the far-ring simplify or writing the ledger. `100 %` in
the null row means 100 % of the staged work, not of the clock, and reporting it
without this paragraph would have been the wall-clock lie this feature refuses
to tell.

So the clock was measured directly, region A, same exe, same command:

* **a full bake: 32 s wall** (27.1 s of it staged);
* **a null incremental run of the same region: 1 s wall** -- the price of
  parsing the plugin, reading the ledger and recomputing 25 input digests.

That 1 s is the **fixed cost every incremental run pays**, and it is also close
to the fixed cost every *ordinary* bake now pays, because the ledger is written
whether or not anybody ever uses it. Caveat stated rather than buried: the OS
file cache was hot from dozens of prior bakes of this same region, so 1 s is a
warm-cache number and a cold first run will be slower.

**The saving tracks the dirty fraction, which is the honest headline**: 9 dirty
chunks of 25 is 36 % of the work and buys back about half to two thirds of the
staged time. On a real worldspace a one-cell edit is a few chunks out of
hundreds, and the same arithmetic is worth hours -- but that is an extrapolation
from a region bake, and no whole-Commonwealth incremental run was made.

## B6 The picture

`scratchpad/land1_20260912/images/b_incremental.png` (2004 x 854), drawn by
`b_pics.py` from the gate's own tree -- it bakes nothing.

**Top row, five panels, each a real `.DDS` off disk** for Commonwealth chunk
(-20,20) dim 4, the chunk gate arm `A/land` moved:

| panel | what it shows |
|---|---|
| `base` | the region as it was, sha1 `54201bfe79a2fa305c8a5ea0` |
| `full bake` | the edited plugin, all 25 chunks rebaked, sha1 `217cc48bfecd3a81e9583434` |
| `\|full - base\| x8` | **the floor** -- not black. The edit reached the output: max delta 20 of 255 |
| `dirty rebake` | the edited plugin, 9 chunks of 25, sha1 `217cc48bfecd3a81e9583434` -- **the same sha1 as the full bake** |
| `\|incr - full\| x8` | **the promise** -- black, `max delta = 0 of 255` |

The x8 gain on both difference panels is the point of them: a difference too
small to see at 1x would still be a difference, so the gain is the honest way to
look at one. The picture is evidence; the sha1s printed under the panels and the
tree-wide byte comparison in B3 are the proof.

**Bottom row: the dirty set of all eight arms**, 5x5 chunk grids. Bright green =
the chunk whose input actually moved (or whose output was deleted); dim green =
dirtied by the one-chunk widening; grey = left exactly as the previous bake wrote
it.

Those grids are **computed, not illustrated, and they are a second check on the
diff.** The first draft shaded `dirty` cells outward from the centre and called
the shading illustrative -- which was the wrong call, because a reader looks at
the picture, not at the disclaimer, and a diamond of nine chunks is not what a
one-cell edit dirties. The grids now come from the seed chunks the exe itself
printed, widened by the rule in `nifcli.cpp:3791`, and the script **refuses to
draw** if its computed set does not have the size the exe reported. It drew on
all eight arms, so the widening rule and the exe's own count agree independently.

Part A's two pictures are unchanged: `images/a_land_guide_flat.png` and
`images/a_land_guide_slope.png`. All three were baked with `--road-detail 1`.

---

## B7 What is red, and what was not measured

**Red in the gates: nothing.** B2 5/5, B3 8/8, B4 0 failures. The inherited red
(`lodgen_roads.sh` R5) was cleared earlier in the lane and the suite reads 11
checks / 0 failures.

**This paragraph was written before the harness chain ran, and the chain
disagreed.** It found two reds this part had introduced -- `lodgen_roads.sh` R1
and `lodgen_native.sh` check 5 -- neither of which any gate of mine could see,
because every one of them compares bakes made by the same command. Both are
diagnosed, fixed and re-verified at **B8**, and the sentence above is left
standing rather than quietly widened: a section headed *what is red* that was
written before the last measurement is exactly the kind of claim that needs its
date attached.

Not measured, each named rather than left to inference:

1. **The `assets` row of the dependency map has no gate arm.** Rows 4, 6 and 7 --
   an `LTEX` texture set's bytes, a ref base's LOD `.nif` bytes, a `SCOL` part's
   models -- are digested through `lodgenReadAsset()`, and the LTEX row was added
   this session after reasoning about what a loose-file override could reach.
   **None of them was exercised by an edit.** An arm that drops an overriding
   loose `.dds` or `.nif` into a resource folder and re-runs would close this,
   and it is the single most valuable arm the next lane could add.
2. **No floor arm with a deliberately wrong constant.** B1 promised one -- halve
   the AO reach in the digest and watch the gate go red. It needs a second exe
   built with a wrong constant and this lane had one build slot for Part B, so
   the refuter is only partly retired (B1.5).
3. ~~**No `--vt` arm.**~~ **Corrected at B8.** This said `--vt` was reasoned
   into the switch digest and never baked. It was baked, by a harness this
   section had not thought to consult: `lodgen_roads.sh` R1 compares two
   `--no-roads` bakes that differ only in their `--vt` directory, and it went
   red. `--vt`'s token is in the digest and its path is not, and R1 is now the
   arm that holds that open. What is STILL not measured is an `--incremental`
   run across a change of `--vt` state, and no arm asserts the new `--native`
   whole-region refusal fires -- that refusal is reasoned from the collection
   sites, not gated.
4. **Two regions of one worldspace.** Both are 5x5 chunks of Commonwealth at
   dim 4. No far ring (dim 16/32), no second worldspace, and **no
   whole-Commonwealth incremental run** -- the brief's region-bakes-only rule
   stands and the B5 extrapolation to "hours" is arithmetic, not a measurement.
5. **The 1 s fixed cost is a warm-cache number.** The OS file cache held this
   region after dozens of bakes; a cold first run will be slower and was not
   measured.
6. **The ledger has never been read back after a NifSkope version change.** It
   carries `version 1` and the reader checks it, but no upgrade path has been
   exercised because there is nothing yet to upgrade from.

---

## B8 The harness chain, the two reds it found, and what they were

The chain is `scratchpad/land1_20260912/b8_chain.sh`, run through the MSYS2
shell with `PY` **named** -- MSYS2's own python has no numpy and an empty number
reads exactly like a render regression (`docs/MISTAKES.md`, ROADS1). Logs under
`logs/hb_*.txt` (first run) and `logs/hc_*.txt` (after the fix).

### B8.1 First run, on the 08:42:33 exe -- two reds, and both were mine

| harness | Part A's exe (07:42:22) | Part B's exe (08:42:33) | verdict |
|---|---|---|---|
| `lodgen_identity` | PASS | PASS | unchanged |
| `lodgen_terrain` | 26 / 0 | 26 / 0 | unchanged |
| `lod_generation` | 116 / 0 (floor 116) | 116 / 0 | unchanged |
| `lodgen_terrain_vt` | 41 / 1 | 41 / 1 | **inherited**, identical failing line |
| `lodgen_ground_cover` | 29 / 5 | 29 / 5 | **inherited**, identical five lines |
| `lodgen_terrain_pbrm` | 14 / 0 | 14 / 0 | unchanged |
| `lodgen_native` | 18 / 0 | **18 / 1** | **NEW RED** |
| `lodgen_roads` | 11 / 0 *(A cleared R5)* | **11 / 1** | **NEW RED** |
| `lodl_open` | 23 / 0 | 23 / 0 | unchanged |
| `animws` | rc 0 | rc 0 | unchanged |

The two inherited reds were compared **line by line**, not by count: the same
four lines in `lodgen_terrain_vt` (V9c, the E/W seam) and the same five in
`lodgen_ground_cover` (C1, C2 x3, C6a...), so nothing of this lane's is hiding
behind a matching total.

```
FAIL R1 two --no-roads runs are byte-identical (1 of 10 differ)
     differs: ./obj/Commonwealth.lodb
```
```
DIFFER Commonwealth.lodb
26 stock files compared, 1 differ
FAIL the stock bake is byte-identical with and without --native
```

### B8.2 One root cause, and it was a sentence I wrote in the docs

`--incremental` writes `<out-dir>/<WS>.lodb` on **every** bake. That is the
right design -- you cannot diff against a ledger nobody wrote -- and it silently
changed the contract of every gate in the tree that says *two bakes of this are
byte-identical*. Both reds are the ledger's `switches` field:

* `lodgen_roads.sh` R1 bakes `--no-roads` twice, into `roadOff/` and
  `roadOff2/`. The `bake()` helper gives each run its own directory, so the two
  argument vectors differ in `--out-dir`, `--tex-dir` and **`--vt`**. The first
  two are on the digest's skip list. `--vt` was taken OFF it earlier this
  session.
* `lodgen_native.sh` check 5 bakes the same region with and without
  `--native <dir> --native-mesh-report <file>` and asserts the stock outputs are
  identical. They are -- except for a ledger recording whether a `.lodo` was
  written beside them.

Both follow from one sentence I put in `docs/LODGEN_LEDGER_FORMAT.md` section 3:
*"A flag that changes the **output** belongs in the digest even when it does not
change the **inputs**."* It reads like conservatism, and over-rebaking really is
free of correctness risk, which is what makes it seductive. It is not free: it
is paid in false refusals nobody can explain, and here it was paid in two
harness reds that look exactly like a broken bake.

The test is narrower than the sentence. The ledger tracks the per-chunk
`.BTO`/`.BTR`/`.DDS` files it lists and nothing else, so the question is **can
this flag make a TRACKED CHUNK stale?**

* `--vt` can -- the sheets come from the pyramid instead of the stock per-chunk
  composite, a different picture from the same inputs. Its **argument** cannot:
  it is a place to put the pyramid, exactly like `--out-dir`'s. One skip list
  could only take both or neither, so there are two lists now, and `--vt` keeps
  its token and loses its path.
* `--native` cannot. The pair goes to its own directory. It leaves the digest
  with `--native-mesh-report`.

### B8.3 The fix found a defect the gate had not

Taking `--native` out of the digest raised the question of what *should* happen
to `--incremental --native`, and the answer was ugly: `lodgenNativeActive()`
collects one `NativePlacement` per drawn reference and one lighting sample per
vertex **inside the chunk pass** (`lodgen.cpp:3784` and `:4069`). A filtered
chunk list therefore writes a `.lodo`/`.lodi` pair holding only the chunks that
happened to be dirty.

That is the `--atlas` case word for word, with one difference that matters: **a
quarter-sized atlas looks like a quarter-sized atlas.** A pair built from a
quarter of a region loads, decodes, passes
`--native-verify --native-verify-corpus`, matches its own three staleness hashes
and is simply missing most of the worldspace. It was reachable until now.
`--native` joins `--atlas`, `--arrays` and `--impostors` on the whole-region
refusal, with its own sentence.

Gate B1's dependency map had this wrong in both directions and both corrections
are in B1.6: it named the merge as whole-region when the merge is a per-file
loop, and it did not name `--native` at all.

### B8.4 Second run, on the 09:21:04 exe -- both cleared

| harness | before | after |
|---|---|---|
| `lodgen_native` | 18 checks / **1** failure, RESULT FAIL | 18 checks / **0**, **RESULT PASS** |
| `lodgen_roads` | 11 checks / **1** failure, RESULT FAIL | 11 checks / **0**, **RESULT PASS** |

Everything else on the chain is byte-for-byte the same verdict as B8.1,
inherited reds included: `lodgen_identity` PASS, `lodgen_terrain` 26/0,
`lod_generation` 116/0, `lodgen_terrain_vt` 41/1, `lodgen_ground_cover` 29/5,
`lodgen_terrain_pbrm` 14/0, `lodl_open` 23/0, `animws` rc 0, zero segfaults
anywhere.

### B8.5 The sweep the reds should have prompted BEFORE the build

Two harnesses found this. Nothing guaranteed they were the only two, so the
question was asked properly rather than left to the chain:
`grep -c "byte-identical|cmp -s" tests/spells/*.sh` names every gate in the tree
that compares two trees, and four of them bake with lodgen and are not on my
chain. All four were run (`logs/hd_*.txt`):

| harness | result | ledger seen |
|---|---|---|
| `lodgen_farring` | RESULT PASS | no |
| `lodgen_texture_arrays` | RESULT PASS | no |
| `lodgen_native_baseline` | **RESULT FAIL** | **yes** |
| `lod_channel_preview` | RESULT PASS | no |

`lodgen_native_baseline` is a different animal and its red is not a defect:

```
NEW      region/Commonwealth.lodb
25 files in the baseline, 26 baked, 1 differ
baseline exe 664e0de4... 2026-09-10T03:57:46; this exe 943b52db... 2026-09-12T09:21:04
```

**Zero hashes differ. One file is new**, and it is new because the feature
writes it. The tempting fix is `--write`, and it is wrong: that baseline is a
checked-in list of hashes from ONE NAMED BUILD of 2026-09-10, and re-freezing it
from a mid-lane exe would silently bless every other lane's drift since, which
is the one thing its own header forbids.

The harness already made this call once for the same reason -- the `.BTR` of the
region run is not hashed, "terrain has its own lanes and its own gates" -- so
`*.LODB` joins `*.BTR` in the excluded set, with the reasoning in the header.
The ledger has its own gates and they are stricter than this one: B4 checks its
magic, version, sort order, relative paths and determinism, and B3 compares it
**byte for byte, by name, on all eight arms**. Hashing it here would turn every
future ledger version bump into a red about the stock vertex writer. The
`exclude=BTR` profile string is deliberately **unchanged**, because `--check`
refuses a baseline written under a different profile and changing the string
would reject the checked-in file it is meant to compare against.

### B8.6 A stale object that the build gate could not see

Found while re-checking the tree after a merge landed in it mid-lane, and worth
writing down because it is the `exe -nt src` trap wearing a new costume.

`tools/ww_build.sh` gates on **the exe being newer than the sources**. Six files
arrived in the tree from another lane with their **mtimes preserved from the
source tree** (08:26-08:46), older than the 09:21:04 exe. So the gate passed,
`make` had nothing to say, and the objects told the truth:
`GeneratedFiles/.obj/animdopesheet.o` was **04:34:58** against an
`animdopesheet.cpp` of 08:32:23. `make -n` wanted six translation units.

**An exe newer than a source file is not an exe built from it.** The object
timestamps are the check -- which is what this lane has been saying since the
first Part B build -- and `make -n` is the two-second version of it. A copy that
preserves timestamps defeats every mtime gate in the chain at once, and a lane
that reads only the exe's own stamp will measure the wrong binary and never
know.

### B8.7 The shipping exe, re-verified

The rebuild at B8.6 produced the exe that ships (09:32:37, 21,951,488 B, sha1
`3e1914a0637b66f438d873e0230b1e8c04d7c806`). It carries **no lodgen change** the
09:21:04 exe did not -- `nifcli.o` is 09:20:58 and `lodgen.o` 08:32:06 in both --
but "it should be the same" is the claim this lane exists to distrust, so the
five harnesses that could speak to it were re-run on it (`logs/he_*.txt`):

| harness | on the shipping exe |
|---|---|
| `lodgen_identity` | **RESULT PASS** |
| `lodgen_roads` | 11 checks / 0 failures, **RESULT PASS** |
| `lodgen_native` | 18 checks / 0 failures, **RESULT PASS** |
| `lodgen_native_baseline` | 25 files in the baseline, 25 baked, **0 differ**, RESULT PASS |
| `animws` | 236 checks / 0 failures / **1 skip** |

`lodgen_native_baseline` reading `25 baked` rather than `26` is the exclusion
doing its job: the ledger is still written, and is still compared byte for byte
by gate B3 -- it is only this frozen stock-vertex guard that no longer hashes it.

One number does not match what was handed to me: the `animws` baseline was
quoted as 236 / 0 / **2 skips** and this run skips **1**. A skip is never a
pass, so a skip that turns into a real check is the harmless direction, and the
remaining one names itself (`10mmPistol.nif has no NiControllerSequence`). It is
another lane's harness and another lane's number; it is reported, not adjusted.

Zero segfaults on every run of every chain in this part.

---

## B9 Finished-work review against the skills this part was told to use

**`ww-prototype-is-not-the-product`.** Met, and it is the spine of Part B. The
edits are made **in a real copy of `Fallout4.esm`** by `b_esmedit.py` -- a
compressed `LAND` height raised with the GRUP size chain fixed up, a `REFR`
moved -- not by a loose-file override, because rows 1, 2, 3, 5 and 7 of the
dependency map live in the ESM and nothing else can reach them. Every comparison
is between two output trees the shipping exe actually wrote. **Where the skill
was nearly broken**: the first `refs` arm moved a reference the bake does not
draw, so the "test" compared a plugin edit that never reached the product. That
is the prototype trap wearing a real-plugin costume, and the per-arm floor is
what caught it.

**`ww-control-calibration`.** Met in three places, and each earned its keep. The
`null` arm is the instrument's known answer -- nothing changed, so the full bake
must not move and the incremental run must rebake nothing *and still match*; if
it moved, the arm reports **BROKEN CONTROL**, not PASS. The `merge-ok` arm in B2
is a negative control proving the default command does **not** refuse, and
without it all four refusal arms would have passed on the build that refused
everything. And B4's determinism check is two independent full bakes compared to
each other, not a claim about the writer.

**`ww-spec-gate-audit`.** Met on the ordering that matters: the dependency map
(B1) was written **before** the code, and the gate's verdicts are reported
including the two arms that were passing for reasons they had not asked about
(B2.1) and the arm whose floor was a lie (B3.1). **The audit's honest finding is
that two of B1's own claims did not survive the code** -- the merge is not a
whole-region pass, and a missing output is not a refusal -- and both are
corrected in place at B1.6 rather than edited out of the text above them.

**`ww-texel-picture`.** Met: real `.DDS` texels off disk, the window and the x8
gain both stated, sha1s beside the panels so the picture is never the proof, and
the dirty grids computed from logged data with a refusal if they disagree with
the exe.

**`nifskope-ww-build-verify`.** Met, and then found to be not enough. Every exe
this lane built is stamped with its **objects** checked -- not `exe -nt src`,
which a link triggered by the other translation unit satisfies just as well --
and without that check at least two of this lane's gate runs would have measured
a previous exe. The shipping exe is **09:32:37, 21,951,488 B, sha1
`3e1914a0637b66f438d873e0230b1e8c04d7c806`**, with `nifcli.o` 09:20:58 and
`lodgen.o` 08:32:06; gates B2-B5 and the picture were measured on 08:42:33
(`1e4e2c5c...`) and that is said in the provenance block rather than blurred.

**Where the skill's own rule proved insufficient (B8.6).** The gate compares the
exe's timestamp against the sources' -- and six files arrived in the tree from
another lane with their **mtimes preserved**, older than the exe, so the gate
passed over six stale objects. `make -n` is the two-second check that actually
answers the question, and it is now in `MISTAKES_ENTRIES.md` as the ninth entry.
A rule that reads timestamps can always be defeated by a copy that preserves
them.

**`nifskope-ww-lodgen`.** Met: `--road-detail 1` on every bake and picture;
region bakes only; Fallout4.exe confirmed down before every build and every exe
launch; no UI file touched and `E:/Projects/NifskopeWWE_ui` never entered;
nothing committed and `git stash` never run; patch scripts written with the Write
tool rather than heredocs, and the one place a heredoc was tried anyway
(`b_addmoveid.py`, first attempt) failed on exactly the backslash rule the skill
names.
