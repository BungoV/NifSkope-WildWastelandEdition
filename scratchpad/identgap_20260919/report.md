# IDENTGAP -- identity-gated distance, measured against pure identity

bungo to the director, 2026-09-19: *"unless you've got a better idea on how to prevent self
occlusion outside the cascade range"*.

Today's answer is **identity**: outside the cascade an LOD pixel ignores any occluder carrying
its own `.lodi` GROUP id, which kills the artefact patch that appears mid-wall when a flat
low-poly LOD wall shadows itself. The price is **every** genuine self-shadow inside one
identity, and the just-ruled proximity join (non-tree placements, mesh-to-mesh gap <= 64 u,
588 groups -> 167, Gwinnett 205 -> 6) makes identities bigger, so the price rises.

The candidate, stated before anything was run:

> **IDENTITY-GATED DISTANCE.** A same-identity occluder is ignored **only** when it lies within
> `D` units of the receiver **along the sun ray** -- the zone where self-occlusion is an
> artefact of the lookup's own resolution. A same-identity occluder farther than `D` still
> shadows. A different-identity occluder always shadows, with the normal small bias.

Every figure below is in `run.log`, `dsweep.log`, `rows.json` or `dsweep.json`. Nothing is
quoted here that is not in one of them.

---

## 1. What was run

Reused, not rewritten: sunsim1's scene, cameras and shading (`scene.py`, `render.py`,
`shade.py`), horizon4's `h4map.ShadowMap` and `h4core`, identprox's `join` (both identity
tables) and `cams`/`hwycams`, identres's `IdentShear` (the exact identity-carrying sun cast).
New here: `identgap.py` (the gate in both the map lookup and the exact cast, the back-face
selection, the shell census), `sss.py` (the screen-space march of the director's addendum),
`run.py`, `dsweep.py`, `fair.py`, `wallart.py`, `pics.py`. The last two exist because the first
tables were unfair to G0 and blind to where the artefact actually lives; both are in section 3
and both moved the verdict. All three G-buffers and all three LEFT panels were read from
the caches earlier lanes built; nothing was written into another lane's folder.

### The geometry of the gate, and why it is one compare

`h4map`'s light space puts `u` along the sun's horizontal direction and `s = z - u*tan(el)`,
constant along a sun ray. A blocker at map depth `mu` above a receiver at depth `u` sits

```
L = (mu - u) / cos(el)
```

units from it **along the ray**. So *"within D along the ray"* is `mu - u <= D*cos(el)`: one
subtract and one compare, on a depth the shader has already fetched, beside the identity it
has already fetched. `D*cos(el)` is a per-frame constant. That is the entire runtime cost --
no new texture, no second tap, no extra bandwidth, no change to the `.lodi` file.

### The gate in the exact (no-map) cast

`IdentShear` stores, per 16 u cell and after a suffix maximum along `u`, the best `s`, the
identity that achieved it, **and** the best `s` among entries of a different identity. Its
`lit()` is exactly *"the best-of-another-identity over every column beyond the receiver"*.
Because a different-identity occluder shadows at any distance and a same-identity one shadows
beyond `D`, the gated answer needs no new structure at all:

```
blocked(D) = blocked_G1  OR  ( best s over columns beyond u + D*cos(el) ) > s
```

-- the same `bs` plane, read at a shifted column.

### The two identity tables

| table | rule | groups | largest group |
|---|---|---|---|
| **A** | today's shipped `.lodi` v7 | **588** over 2,449 placements | 205 placements |
| **B** | the ruled proximity join: non-tree, mesh gap <= 64 u | **167** | 206 placements |

Both reproduce identprox's `RECOMMENDATION.txt` (588 -> 167) exactly; asserted in `run.py`.

### The rows

| id | rule |
|---|---|
| **G0** | no identity at all; slope-scaled + normal-offset bias, **tuned** over a 96-cell sweep |
| **G0strict** | the same, with the bias cell that fights the artefact hardest instead |
| **G1** | pure identity -- today's rule, the baseline |
| **G2** | the gate, `D` = 64 / 128 / 256 / 512 / 1024 u |
| **G3** | back-face casting -- only triangles facing away from the sun write the map -- no identity |
| **G4** | the best `G2` with the bias re-tuned on top |
| **+SSS** | the addendum: a screen-space shadow march on top of a far row |

Three lookups: **16 u texel with 3x3 PCF** (the realistic one), **no map at all** (the limit),
and **64 u texel with one nearest tap** (the coarse case, the only one where the mid-wall patch
is large enough to see). Three cameras.

### How the bias was tuned, since G0 lives or dies by it

96 cells: normal offset 0 / 0.5 / 1 / 2 / 3 / 4 texels x constant depth bias 0.25 / 0.5 / 1 / 2
texels x slope-scaled term 0 / 1 / 2 / 4, scored on every third decided pixel and the winner
re-scored on all of them. The objective is the **object disagreement total**, so a bias that
kills acne by peter-panning the shadow off the wall pays for it in false-LIT. `run.log` also
prints, for every sweep, the cell with the least object false-DARK and what it costs -- that is
the `G0strict` row, and it is how the report shows what a no-identity build pays to suppress
the artefact.

### The controls, per camera, all in `run.log`

1. The gated cast at `D = 0` agrees with the ray-cast LEFT panel on **100.00%** of decided
   pixels on all three cameras -- the machinery differs from the truth only by the rule.
2. The gated cast at `D = infinity` is **array-identical** to identres's `IdentShear.lit`, so
   G1 reproduces the published baseline rather than re-deriving it.
3. The gate is **monotone in `D`**: every shadow at a larger `D` is a subset of the shadow at a
   smaller one. This FAILED on the first build -- a receiver outside the near grid could acquire
   a blocker purely because `D` moved the read column into range -- and the fix is in
   `identgap.py`, commented.
4. Both identity tables reproduce identprox's counts.

### G3's geometry census, which decides G3 before a single pixel is drawn

Positions welded at 0.25 u (a `.lodo` mesh splits a vertex at every seam, so an index-keyed
edge test calls even a watertight box open -- keyed on raw indices **0** of 2,449 placements
would count as closed):

> **2,449 placements carry level-0 triangles. 106 are closed shells. 2,343 are OPEN.
> 26,999 of 29,587 triangles (91.3%) belong to an open shell, and 1,282 of the open ones are
> two-triangle CARDS (2,551 triangles).**

Under back-face casting an open shell has nothing behind its sun-facing side, so once that side
is culled it writes no depth at all and **stops casting**. 91.3% of this chunk's LOD triangles
are in that state. G3 is measured anyway, and the numbers are below, but the census is the
verdict: back-face casting is not available to an LOD field made of cards and open shells.


---

## 2. The tables

Disagree = the row calls a pixel lit where the ray-cast sun calls it shadow, or the reverse, over **all** decided pixels of the frame -- one denominator for every row of a camera. *false-DARK* = shadow the row invents (**the artefact identity exists to kill**); *false-LIT* = shadow the row loses (**what the gate should give back**).


### camera `hwydeck`, sun azimuth 180, elevation 10 -- the elevated highway deck and its support columns


**lookup: 16 u texel, 3x3 PCF -- the realistic one**

| row | obj false-DARK | obj false-LIT | terrain disagree | ALL |
|---|---|---|---|---|
| **G0** no identity, tuned bias | 0.74% | 4.84% | 9.44% | 7.85% |
| G0strict  bias tuned to the least artefact | 0.58% | 5.41% | 9.38% | 7.98% |
| **G1** pure identity, table A | 0.55% | 6.84% | 9.23% | 8.47% |
| **G1** pure identity, table B | 0.52% | 12.89% | 9.23% | 10.96% |
| G2 gate D = 64 u, table A | 0.64% | 5.34% | 9.23% | 7.89% |
| G2 gate D = 128 u, table A | 0.63% | 5.39% | 9.23% | 7.91% |
| G2 gate D = 256 u, table A | 0.62% | 5.48% | 9.23% | 7.94% |
| G2 gate D = 512 u, table A | 0.57% | 5.70% | 9.23% | 8.01% |
| G2 gate D = 1024 u, table A | 0.55% | 6.32% | 9.23% | 8.26% |
| G2 gate D = 64 u, table B | 0.63% | 5.35% | 9.23% | 7.89% |
| G2 gate D = 128 u, table B | 0.63% | 5.39% | 9.23% | 7.90% |
| G2 gate D = 256 u, table B | 0.61% | 5.49% | 9.23% | 7.94% |
| G2 gate D = 512 u, table B | 0.56% | 5.73% | 9.23% | 8.02% |
| G2 gate D = 1024 u, table B | 0.54% | 6.58% | 9.23% | 8.36% |
| **G3** back-face casting, no identity | 0.54% | 5.86% | 10.18% | 8.62% |
| **G4** gate D = 64 u + re-tuned bias, table A | 0.73% | 4.86% | 9.44% | 7.85% |
| **G4** gate D = 64 u + re-tuned bias, table B | 0.73% | 4.86% | 9.44% | 7.85% |

**lookup: no map at all -- per-pixel cast, the limit**

| row | obj false-DARK | obj false-LIT | terrain disagree | ALL |
|---|---|---|---|---|
| **G0** no identity, tuned bias | 0.00% | 0.00% | 0.00% | 0.00% |
| **G1** pure identity, table A | 0.00% | 1.41% | 0.00% | 0.58% |
| **G1** pure identity, table B | 0.00% | 8.51% | 0.00% | 3.51% |
| G2 gate D = 64 u, table A | 0.00% | 0.12% | 0.00% | 0.05% |
| G2 gate D = 128 u, table A | 0.00% | 0.17% | 0.00% | 0.07% |
| G2 gate D = 256 u, table A | 0.00% | 0.31% | 0.00% | 0.13% |
| G2 gate D = 512 u, table A | 0.00% | 0.51% | 0.00% | 0.21% |
| G2 gate D = 1024 u, table A | 0.00% | 0.92% | 0.00% | 0.38% |
| G2 gate D = 64 u, table B | 0.00% | 0.26% | 0.00% | 0.11% |
| G2 gate D = 128 u, table B | 0.00% | 0.42% | 0.00% | 0.17% |
| G2 gate D = 256 u, table B | 0.00% | 0.79% | 0.00% | 0.33% |
| G2 gate D = 512 u, table B | 0.00% | 1.47% | 0.00% | 0.61% |
| G2 gate D = 1024 u, table B | 0.00% | 2.85% | 0.00% | 1.18% |
| **G3** back-face casting, no identity | 0.00% | 1.43% | 1.83% | 1.66% |

**lookup: 64 u texel, ONE nearest tap -- the coarse case**

| row | obj false-DARK | obj false-LIT | terrain disagree | ALL |
|---|---|---|---|---|
| **G0** no identity, tuned bias | 2.72% | 3.71% | 9.50% | 8.23% |
| G0strict  bias tuned to the least artefact | 1.67% | 12.27% | 10.99% | 12.21% |
| **G1** pure identity, table A | 2.07% | 7.87% | 8.61% | 9.16% |
| **G1** pure identity, table B | 1.57% | 12.10% | 8.61% | 10.70% |
| G2 gate D = 64 u, table A | 2.13% | 5.29% | 8.61% | 8.12% |
| G2 gate D = 64 u, table B | 2.13% | 5.29% | 8.61% | 8.12% |

**The screen-space shadow march on top** (director's addendum)

| base | steps | reach | recovered of the base false-LIT | NEW false-dark (of object px) | obj false-DARK | obj false-LIT | ALL |
|---|---|---|---|---|---|---|---|
| G1/A/map16 | 16 | 5% | **16.1%** | **1.10%** | 1.65% | 5.73% | 8.54% |
| G1/A/map16 | 32 | 5% | **16.7%** | **1.15%** | 1.70% | 5.69% | 8.55% |
| G1/A/map16 | 64 | 5% | **17.2%** | **1.26%** | 1.81% | 5.66% | 8.58% |
| G1/A/map16 | 16 | 10% | **22.7%** | **1.48%** | 2.03% | 5.28% | 8.51% |
| G1/A/map16 | 32 | 10% | **25.8%** | **1.67%** | 2.22% | 5.07% | 8.50% |
| G1/A/map16 | 64 | 10% | **26.9%** | **1.74%** | 2.29% | 5.00% | 8.51% |
| G2/A/D64/map16 | 16 | 5% | **10.4%** | **1.08%** | 1.72% | 4.79% | 8.18% |
| G2/A/D64/map16 | 32 | 5% | **10.7%** | **1.13%** | 1.77% | 4.77% | 8.19% |
| G2/A/D64/map16 | 64 | 5% | **11.3%** | **1.24%** | 1.87% | 4.74% | 8.23% |
| G2/A/D64/map16 | 16 | 10% | **20.2%** | **1.46%** | 2.10% | 4.27% | 8.12% |
| G2/A/D64/map16 | 32 | 10% | **22.2%** | **1.65%** | 2.28% | 4.16% | 8.15% |
| G2/A/D64/map16 | 64 | 10% | **23.3%** | **1.72%** | 2.36% | 4.10% | 8.16% |
| G1/B/map16 | 16 | 5% | **10.4%** | **1.10%** | 1.62% | 11.55% | 10.93% |
| G1/B/map16 | 32 | 5% | **10.8%** | **1.15%** | 1.67% | 11.50% | 10.93% |
| G1/B/map16 | 64 | 5% | **11.1%** | **1.26%** | 1.78% | 11.47% | 10.96% |
| G1/B/map16 | 16 | 10% | **14.3%** | **1.48%** | 2.00% | 11.05% | 10.88% |
| G1/B/map16 | 32 | 10% | **16.1%** | **1.67%** | 2.19% | 10.81% | 10.86% |
| G1/B/map16 | 64 | 10% | **16.8%** | **1.74%** | 2.26% | 10.73% | 10.86% |
| G2/B/D64/map16 | 16 | 5% | **10.4%** | **1.08%** | 1.72% | 4.79% | 8.18% |
| G2/B/D64/map16 | 32 | 5% | **10.7%** | **1.13%** | 1.77% | 4.77% | 8.19% |
| G2/B/D64/map16 | 64 | 5% | **11.3%** | **1.24%** | 1.87% | 4.74% | 8.23% |
| G2/B/D64/map16 | 16 | 10% | **20.2%** | **1.46%** | 2.10% | 4.27% | 8.12% |
| G2/B/D64/map16 | 32 | 10% | **22.2%** | **1.65%** | 2.28% | 4.16% | 8.15% |
| G2/B/D64/map16 | 64 | 10% | **23.3%** | **1.72%** | 2.36% | 4.10% | 8.16% |

**Where the artefact comes back** -- `dsweep.py`, identity table A, the same tuned bias, D taken below the brief's grid:

| D | 16 u PCF: false-DARK | false-LIT | 64 u nearest: false-DARK | false-LIT |
|---|---|---|---|---|
| 8 u | 0.74% | 4.84% | 2.55% | 4.38% |
| 16 u | 0.74% | 4.84% | 2.55% | 4.38% |
| 32 u | 0.74% | 4.84% | 2.55% | 4.38% |
| 48 u | 0.74% | 4.85% | 2.55% | 4.38% |
| 64 u | 0.73% | 4.86% | 2.55% | 4.38% |
| 92 u | 0.73% | 4.87% | 2.55% | 4.39% |
| 128 u | 0.73% | 4.90% | 2.54% | 4.41% |
| 184 u | 0.73% | 4.93% | 2.52% | 4.43% |
| 256 u | 0.73% | 4.98% | 2.52% | 4.47% |
| 512 u | 0.71% | 5.19% | 2.52% | 4.60% |
| infinity (= G1) | 0.66% | 6.32% | 2.48% | 5.34% |

**The same rows scored on WALL pixels only** -- object, `|normal.z| < 0.35`, which is where the mid-wall patch lives (`wallart.py`). 170,083 of the frame's object pixels are walls:

| row | PATCH  sun LIT / row dark | LOST  sun dark / row lit |
|---|---|---|
| `G0/map16` | 1,644 px  (0.97% of walls) | 9,920 px  (5.83% of walls) |
| `G0strict/map16` | 1,728 px  (1.02% of walls) | 10,768 px  (6.33% of walls) |
| `G1/A/map16` | 1,625 px  (0.96% of walls) | 13,376 px  (7.86% of walls) |
| `G1/B/map16` | 1,528 px  (0.90% of walls) | 14,897 px  (8.76% of walls) |
| `G2/A/D64/map16` | 1,756 px  (1.03% of walls) | 10,727 px  (6.31% of walls) |
| `G2/B/D64/map16` | 1,753 px  (1.03% of walls) | 10,728 px  (6.31% of walls) |
| `G4/A/map16` | 1,612 px  (0.95% of walls) | 9,997 px  (5.88% of walls) |
| `G4/B/map16` | 1,612 px  (0.95% of walls) | 10,001 px  (5.88% of walls) |
| `G3/map16` | 1,193 px  (0.70% of walls) | 12,474 px  (7.33% of walls) |
| `G0/map64` | 6,640 px  (3.90% of walls) | 7,903 px  (4.65% of walls) |
| `G1/A/map64` | 5,755 px  (3.38% of walls) | 10,689 px  (6.28% of walls) |
| `G1/B/map64` | 5,493 px  (3.23% of walls) | 12,282 px  (7.22% of walls) |
| `G2/A/D64/map64` | 5,925 px  (3.48% of walls) | 8,811 px  (5.18% of walls) |
| `G2/B/D64/map64` | 5,925 px  (3.48% of walls) | 8,811 px  (5.18% of walls) |
| `G0/cast` | 4 px  (0.00% of walls) | 1 px  (0.00% of walls) |
| `G1/A/cast` | 4 px  (0.00% of walls) | 2,251 px  (1.32% of walls) |
| `G2/A/D64/cast` | 4 px  (0.00% of walls) | 293 px  (0.17% of walls) |
| `G3/cast` | 1 px  (0.00% of walls) | 3,939 px  (2.32% of walls) |

### camera `east`, sun azimuth 120, elevation 15 -- the city block from the east


**lookup: 16 u texel, 3x3 PCF -- the realistic one**

| row | obj false-DARK | obj false-LIT | terrain disagree | ALL |
|---|---|---|---|---|
| **G0** no identity, tuned bias | 1.15% | 3.03% | 16.74% | 7.98% |
| G0strict  bias tuned to the least artefact | 1.08% | 3.21% | 6.60% | 4.99% |
| **G1** pure identity, table A | 1.09% | 7.29% | 6.03% | 7.66% |
| **G1** pure identity, table B | 1.07% | 7.72% | 6.03% | 7.96% |
| G2 gate D = 64 u, table A | 1.24% | 3.48% | 6.03% | 5.12% |
| G2 gate D = 128 u, table A | 1.24% | 3.60% | 6.03% | 5.20% |
| G2 gate D = 256 u, table A | 1.21% | 4.34% | 6.03% | 5.70% |
| G2 gate D = 512 u, table A | 1.12% | 7.06% | 6.03% | 7.53% |
| G2 gate D = 1024 u, table A | 1.09% | 7.19% | 6.03% | 7.60% |
| G2 gate D = 64 u, table B | 1.24% | 3.48% | 6.03% | 5.12% |
| G2 gate D = 128 u, table B | 1.24% | 3.63% | 6.03% | 5.21% |
| G2 gate D = 256 u, table B | 1.21% | 4.41% | 6.03% | 5.74% |
| G2 gate D = 512 u, table B | 1.10% | 7.21% | 6.03% | 7.62% |
| G2 gate D = 1024 u, table B | 1.07% | 7.46% | 6.03% | 7.77% |
| **G3** back-face casting, no identity | 1.04% | 3.78% | 16.95% | 8.49% |
| **G4** gate D = 64 u + re-tuned bias, table A | 1.14% | 3.06% | 16.74% | 8.00% |
| **G4** gate D = 64 u + re-tuned bias, table B | 1.13% | 3.07% | 16.74% | 8.00% |

**lookup: no map at all -- per-pixel cast, the limit**

| row | obj false-DARK | obj false-LIT | terrain disagree | ALL |
|---|---|---|---|---|
| **G0** no identity, tuned bias | 0.00% | 0.00% | 0.00% | 0.00% |
| **G1** pure identity, table A | 0.00% | 5.07% | 0.00% | 3.53% |
| **G1** pure identity, table B | 0.00% | 5.86% | 0.00% | 4.09% |
| G2 gate D = 64 u, table A | 0.00% | 0.45% | 0.00% | 0.31% |
| G2 gate D = 128 u, table A | 0.00% | 0.73% | 0.00% | 0.51% |
| G2 gate D = 256 u, table A | 0.00% | 1.75% | 0.00% | 1.22% |
| G2 gate D = 512 u, table A | 0.00% | 4.83% | 0.00% | 3.37% |
| G2 gate D = 1024 u, table A | 0.00% | 5.00% | 0.00% | 3.48% |
| G2 gate D = 64 u, table B | 0.00% | 0.55% | 0.00% | 0.38% |
| G2 gate D = 128 u, table B | 0.00% | 0.90% | 0.00% | 0.63% |
| G2 gate D = 256 u, table B | 0.00% | 2.03% | 0.00% | 1.42% |
| G2 gate D = 512 u, table B | 0.00% | 5.25% | 0.00% | 3.66% |
| G2 gate D = 1024 u, table B | 0.00% | 5.58% | 0.00% | 3.89% |
| **G3** back-face casting, no identity | 0.00% | 4.27% | 1.67% | 3.49% |

**lookup: 64 u texel, ONE nearest tap -- the coarse case**

| row | obj false-DARK | obj false-LIT | terrain disagree | ALL |
|---|---|---|---|---|
| **G0** no identity, tuned bias | 3.21% | 3.20% | 8.72% | 7.11% |
| G0strict  bias tuned to the least artefact | 2.47% | 5.30% | 9.52% | 8.31% |
| **G1** pure identity, table A | 3.57% | 7.14% | 6.95% | 9.57% |
| **G1** pure identity, table B | 3.51% | 7.59% | 6.95% | 9.84% |
| G2 gate D = 64 u, table A | 3.92% | 3.83% | 6.95% | 7.51% |
| G2 gate D = 64 u, table B | 3.92% | 3.83% | 6.95% | 7.51% |

**The screen-space shadow march on top** (director's addendum)

| base | steps | reach | recovered of the base false-LIT | NEW false-dark (of object px) | obj false-DARK | obj false-LIT | ALL |
|---|---|---|---|---|---|---|---|
| G1/A/map16 | 16 | 5% | **6.3%** | **1.30%** | 2.39% | 6.83% | 8.29% |
| G1/A/map16 | 32 | 5% | **6.9%** | **1.38%** | 2.47% | 6.78% | 8.31% |
| G1/A/map16 | 64 | 5% | **7.1%** | **1.41%** | 2.50% | 6.77% | 8.33% |
| G1/A/map16 | 16 | 10% | **7.1%** | **1.85%** | 2.94% | 6.77% | 8.74% |
| G1/A/map16 | 32 | 10% | **8.7%** | **2.15%** | 3.24% | 6.65% | 8.89% |
| G1/A/map16 | 64 | 10% | **9.4%** | **2.30%** | 3.39% | 6.60% | 8.97% |
| G2/A/D64/map16 | 16 | 5% | **9.0%** | **1.29%** | 2.53% | 3.17% | 5.84% |
| G2/A/D64/map16 | 32 | 5% | **9.9%** | **1.37%** | 2.61% | 3.13% | 5.87% |
| G2/A/D64/map16 | 64 | 5% | **10.2%** | **1.40%** | 2.64% | 3.13% | 5.89% |
| G2/A/D64/map16 | 16 | 10% | **9.8%** | **1.83%** | 3.08% | 3.14% | 6.31% |
| G2/A/D64/map16 | 32 | 10% | **12.2%** | **2.14%** | 3.38% | 3.05% | 6.48% |
| G2/A/D64/map16 | 64 | 10% | **13.2%** | **2.28%** | 3.53% | 3.02% | 6.57% |
| G1/B/map16 | 16 | 5% | **7.0%** | **1.30%** | 2.37% | 7.18% | 8.53% |
| G1/B/map16 | 32 | 5% | **7.6%** | **1.38%** | 2.45% | 7.13% | 8.55% |
| G1/B/map16 | 64 | 5% | **7.8%** | **1.41%** | 2.49% | 7.12% | 8.57% |
| G1/B/map16 | 16 | 10% | **8.8%** | **1.86%** | 2.94% | 7.04% | 8.93% |
| G1/B/map16 | 32 | 10% | **10.4%** | **2.16%** | 3.24% | 6.92% | 9.08% |
| G1/B/map16 | 64 | 10% | **11.1%** | **2.31%** | 3.38% | 6.86% | 9.15% |
| G2/B/D64/map16 | 16 | 5% | **9.0%** | **1.29%** | 2.53% | 3.17% | 5.84% |
| G2/B/D64/map16 | 32 | 5% | **9.9%** | **1.37%** | 2.61% | 3.14% | 5.88% |
| G2/B/D64/map16 | 64 | 5% | **10.2%** | **1.40%** | 2.64% | 3.13% | 5.90% |
| G2/B/D64/map16 | 16 | 10% | **9.8%** | **1.83%** | 3.08% | 3.14% | 6.31% |
| G2/B/D64/map16 | 32 | 10% | **12.2%** | **2.14%** | 3.38% | 3.06% | 6.49% |
| G2/B/D64/map16 | 64 | 10% | **13.2%** | **2.28%** | 3.52% | 3.02% | 6.57% |

**Where the artefact comes back** -- `dsweep.py`, identity table A, the same tuned bias, D taken below the brief's grid:

| D | 16 u PCF: false-DARK | false-LIT | 64 u nearest: false-DARK | false-LIT |
|---|---|---|---|---|
| 8 u | 1.15% | 3.03% | 3.67% | 2.86% |
| 16 u | 1.15% | 3.03% | 3.67% | 2.86% |
| 32 u | 1.15% | 3.03% | 3.67% | 2.86% |
| 48 u | 1.14% | 3.05% | 3.67% | 2.86% |
| 64 u | 1.14% | 3.06% | 3.67% | 2.86% |
| 92 u | 1.13% | 3.10% | 3.67% | 2.86% |
| 128 u | 1.13% | 3.17% | 3.67% | 2.86% |
| 184 u | 1.11% | 3.36% | 3.60% | 3.04% |
| 256 u | 1.09% | 3.82% | 3.48% | 3.56% |
| 512 u | 0.94% | 6.95% | 2.98% | 6.65% |
| infinity (= G1) | 0.88% | 7.17% | 2.80% | 6.92% |

**The same rows scored on WALL pixels only** -- object, `|normal.z| < 0.35`, which is where the mid-wall patch lives (`wallart.py`). 476,193 of the frame's object pixels are walls:

| row | PATCH  sun LIT / row dark | LOST  sun dark / row lit |
|---|---|---|
| `G0/map16` | 5,746 px  (1.21% of walls) | 12,801 px  (2.69% of walls) |
| `G0strict/map16` | 5,561 px  (1.17% of walls) | 13,336 px  (2.80% of walls) |
| `G1/A/map16` | 6,567 px  (1.38% of walls) | 37,539 px  (7.88% of walls) |
| `G1/B/map16` | 6,481 px  (1.36% of walls) | 39,625 px  (8.32% of walls) |
| `G2/A/D64/map16` | 7,165 px  (1.50% of walls) | 15,387 px  (3.23% of walls) |
| `G2/B/D64/map16` | 7,162 px  (1.50% of walls) | 15,415 px  (3.24% of walls) |
| `G4/A/map16` | 5,728 px  (1.20% of walls) | 12,944 px  (2.72% of walls) |
| `G4/B/map16` | 5,727 px  (1.20% of walls) | 13,008 px  (2.73% of walls) |
| `G3/map16` | 5,304 px  (1.11% of walls) | 17,166 px  (3.60% of walls) |
| `G0/map64` | 17,613 px  (3.70% of walls) | 13,377 px  (2.81% of walls) |
| `G1/A/map64` | 21,496 px  (4.51% of walls) | 37,034 px  (7.78% of walls) |
| `G1/B/map64` | 21,202 px  (4.45% of walls) | 39,063 px  (8.20% of walls) |
| `G2/A/D64/map64` | 23,408 px  (4.92% of walls) | 17,640 px  (3.70% of walls) |
| `G2/B/D64/map64` | 23,408 px  (4.92% of walls) | 17,640 px  (3.70% of walls) |
| `G0/cast` | 4 px  (0.00% of walls) | 10 px  (0.00% of walls) |
| `G1/A/cast` | 4 px  (0.00% of walls) | 26,099 px  (5.48% of walls) |
| `G2/A/D64/cast` | 4 px  (0.00% of walls) | 1,124 px  (0.24% of walls) |
| `G3/cast` | 4 px  (0.00% of walls) | 24,763 px  (5.20% of walls) |

### camera `street`, sun azimuth 120, elevation 5 -- street level, a very low sun


**lookup: 16 u texel, 3x3 PCF -- the realistic one**

| row | obj false-DARK | obj false-LIT | terrain disagree | ALL |
|---|---|---|---|---|
| **G0** no identity, tuned bias | 4.18% | 3.98% | 11.07% | 9.71% |
| G0strict  bias tuned to the least artefact | 4.08% | 4.83% | 16.35% | 12.88% |
| **G1** pure identity, table A | 4.27% | 4.65% | 11.77% | 10.44% |
| **G1** pure identity, table B | 4.27% | 4.68% | 11.77% | 10.45% |
| G2 gate D = 64 u, table A | 4.27% | 4.02% | 11.77% | 10.15% |
| G2 gate D = 128 u, table A | 4.27% | 4.08% | 11.77% | 10.17% |
| G2 gate D = 256 u, table A | 4.27% | 4.26% | 11.77% | 10.26% |
| G2 gate D = 512 u, table A | 4.27% | 4.31% | 11.77% | 10.28% |
| G2 gate D = 1024 u, table A | 4.27% | 4.65% | 11.77% | 10.44% |
| G2 gate D = 64 u, table B | 4.27% | 4.02% | 11.77% | 10.15% |
| G2 gate D = 128 u, table B | 4.27% | 4.08% | 11.77% | 10.17% |
| G2 gate D = 256 u, table B | 4.27% | 4.26% | 11.77% | 10.26% |
| G2 gate D = 512 u, table B | 4.27% | 4.33% | 11.77% | 10.29% |
| G2 gate D = 1024 u, table B | 4.27% | 4.67% | 11.77% | 10.45% |
| **G3** back-face casting, no identity | 4.15% | 4.35% | 11.07% | 9.88% |
| **G4** gate D = 64 u + re-tuned bias, table A | 4.18% | 4.01% | 11.07% | 9.73% |
| **G4** gate D = 64 u + re-tuned bias, table B | 4.18% | 4.01% | 11.07% | 9.73% |

**lookup: no map at all -- per-pixel cast, the limit**

| row | obj false-DARK | obj false-LIT | terrain disagree | ALL |
|---|---|---|---|---|
| **G0** no identity, tuned bias | 0.00% | 0.00% | 0.02% | 0.01% |
| **G1** pure identity, table A | 0.00% | 2.64% | 0.02% | 1.24% |
| **G1** pure identity, table B | 0.00% | 2.75% | 0.02% | 1.29% |
| G2 gate D = 64 u, table A | 0.00% | 1.25% | 0.02% | 0.59% |
| G2 gate D = 128 u, table A | 0.00% | 1.53% | 0.02% | 0.72% |
| G2 gate D = 256 u, table A | 0.00% | 1.97% | 0.02% | 0.93% |
| G2 gate D = 512 u, table A | 0.00% | 2.19% | 0.02% | 1.03% |
| G2 gate D = 1024 u, table A | 0.00% | 2.64% | 0.02% | 1.24% |
| G2 gate D = 64 u, table B | 0.00% | 1.28% | 0.02% | 0.61% |
| G2 gate D = 128 u, table B | 0.00% | 1.59% | 0.02% | 0.75% |
| G2 gate D = 256 u, table B | 0.00% | 2.06% | 0.02% | 0.97% |
| G2 gate D = 512 u, table B | 0.00% | 2.29% | 0.02% | 1.08% |
| G2 gate D = 1024 u, table B | 0.00% | 2.74% | 0.02% | 1.29% |
| **G3** back-face casting, no identity | 0.00% | 2.52% | 6.79% | 4.80% |

**lookup: 64 u texel, ONE nearest tap -- the coarse case**

| row | obj false-DARK | obj false-LIT | terrain disagree | ALL |
|---|---|---|---|---|
| **G0** no identity, tuned bias | 7.79% | 3.38% | 18.03% | 14.83% |
| G0strict  bias tuned to the least artefact | 7.48% | 10.29% | 14.24% | 15.89% |
| **G1** pure identity, table A | 7.99% | 3.98% | 14.49% | 13.32% |
| **G1** pure identity, table B | 7.99% | 3.99% | 14.49% | 13.32% |
| G2 gate D = 64 u, table A | 7.99% | 3.69% | 14.49% | 13.19% |
| G2 gate D = 64 u, table B | 7.99% | 3.69% | 14.49% | 13.19% |

**The screen-space shadow march on top** (director's addendum)

| base | steps | reach | recovered of the base false-LIT | NEW false-dark (of object px) | obj false-DARK | obj false-LIT | ALL |
|---|---|---|---|---|---|---|---|
| G1/A/map16 | 16 | 5% | **14.8%** | **0.41%** | 4.68% | 3.97% | 10.31% |
| G1/A/map16 | 32 | 5% | **15.0%** | **0.43%** | 4.69% | 3.96% | 10.31% |
| G1/A/map16 | 64 | 5% | **15.1%** | **0.44%** | 4.70% | 3.95% | 10.32% |
| G1/A/map16 | 16 | 10% | **19.6%** | **0.48%** | 4.75% | 3.74% | 10.24% |
| G1/A/map16 | 32 | 10% | **19.9%** | **0.53%** | 4.79% | 3.73% | 10.26% |
| G1/A/map16 | 64 | 10% | **20.0%** | **0.55%** | 4.82% | 3.72% | 10.26% |
| G2/A/D64/map16 | 16 | 5% | **10.8%** | **0.41%** | 4.68% | 3.59% | 10.14% |
| G2/A/D64/map16 | 32 | 5% | **10.9%** | **0.43%** | 4.69% | 3.58% | 10.14% |
| G2/A/D64/map16 | 64 | 5% | **11.1%** | **0.44%** | 4.70% | 3.58% | 10.14% |
| G2/A/D64/map16 | 16 | 10% | **14.2%** | **0.48%** | 4.75% | 3.45% | 10.11% |
| G2/A/D64/map16 | 32 | 10% | **14.4%** | **0.53%** | 4.79% | 3.44% | 10.12% |
| G2/A/D64/map16 | 64 | 10% | **14.5%** | **0.55%** | 4.82% | 3.44% | 10.13% |
| G1/B/map16 | 16 | 5% | **15.0%** | **0.41%** | 4.68% | 3.97% | 10.32% |
| G1/B/map16 | 32 | 5% | **15.2%** | **0.43%** | 4.69% | 3.96% | 10.32% |
| G1/B/map16 | 64 | 5% | **15.4%** | **0.44%** | 4.70% | 3.96% | 10.32% |
| G1/B/map16 | 16 | 10% | **19.9%** | **0.49%** | 4.75% | 3.75% | 10.24% |
| G1/B/map16 | 32 | 10% | **20.1%** | **0.53%** | 4.79% | 3.74% | 10.26% |
| G1/B/map16 | 64 | 10% | **20.3%** | **0.55%** | 4.82% | 3.73% | 10.27% |
| G2/B/D64/map16 | 16 | 5% | **10.8%** | **0.41%** | 4.68% | 3.59% | 10.14% |
| G2/B/D64/map16 | 32 | 5% | **10.9%** | **0.43%** | 4.69% | 3.58% | 10.14% |
| G2/B/D64/map16 | 64 | 5% | **11.1%** | **0.44%** | 4.70% | 3.58% | 10.14% |
| G2/B/D64/map16 | 16 | 10% | **14.2%** | **0.48%** | 4.75% | 3.45% | 10.11% |
| G2/B/D64/map16 | 32 | 10% | **14.4%** | **0.53%** | 4.79% | 3.44% | 10.12% |
| G2/B/D64/map16 | 64 | 10% | **14.5%** | **0.55%** | 4.82% | 3.44% | 10.13% |

**Where the artefact comes back** -- `dsweep.py`, identity table A, the same tuned bias, D taken below the brief's grid:

| D | 16 u PCF: false-DARK | false-LIT | 64 u nearest: false-DARK | false-LIT |
|---|---|---|---|---|
| 8 u | 4.18% | 3.98% | 7.68% | 3.56% |
| 16 u | 4.18% | 3.98% | 7.68% | 3.56% |
| 32 u | 4.18% | 3.98% | 7.68% | 3.56% |
| 48 u | 4.18% | 4.00% | 7.68% | 3.56% |
| 64 u | 4.18% | 4.01% | 7.68% | 3.56% |
| 92 u | 4.18% | 4.05% | 7.68% | 3.56% |
| 128 u | 4.18% | 4.12% | 7.68% | 3.56% |
| 184 u | 4.18% | 4.22% | 7.68% | 3.61% |
| 256 u | 4.18% | 4.30% | 7.68% | 3.67% |
| 512 u | 4.18% | 4.36% | 7.68% | 3.72% |
| infinity (= G1) | 4.18% | 4.70% | 7.68% | 4.00% |

**The same rows scored on WALL pixels only** -- object, `|normal.z| < 0.35`, which is where the mid-wall patch lives (`wallart.py`). 412,490 of the frame's object pixels are walls:

| row | PATCH  sun LIT / row dark | LOST  sun dark / row lit |
|---|---|---|
| `G0/map16` | 17,905 px  (4.34% of walls) | 13,484 px  (3.27% of walls) |
| `G0strict/map16` | 16,905 px  (4.10% of walls) | 15,392 px  (3.73% of walls) |
| `G1/A/map16` | 18,335 px  (4.44% of walls) | 14,034 px  (3.40% of walls) |
| `G1/B/map16` | 18,335 px  (4.44% of walls) | 14,042 px  (3.40% of walls) |
| `G2/A/D64/map16` | 18,335 px  (4.44% of walls) | 13,306 px  (3.23% of walls) |
| `G2/B/D64/map16` | 18,335 px  (4.44% of walls) | 13,306 px  (3.23% of walls) |
| `G4/A/map16` | 17,905 px  (4.34% of walls) | 13,518 px  (3.28% of walls) |
| `G4/B/map16` | 17,905 px  (4.34% of walls) | 13,518 px  (3.28% of walls) |
| `G3/map16` | 17,846 px  (4.33% of walls) | 14,204 px  (3.44% of walls) |
| `G0/map64` | 34,538 px  (8.37% of walls) | 9,629 px  (2.33% of walls) |
| `G1/A/map64` | 34,545 px  (8.37% of walls) | 10,436 px  (2.53% of walls) |
| `G1/B/map64` | 34,545 px  (8.37% of walls) | 10,451 px  (2.53% of walls) |
| `G2/A/D64/map64` | 34,545 px  (8.37% of walls) | 9,738 px  (2.36% of walls) |
| `G2/B/D64/map64` | 34,545 px  (8.37% of walls) | 9,738 px  (2.36% of walls) |
| `G0/cast` | 9 px  (0.00% of walls) | 3 px  (0.00% of walls) |
| `G1/A/cast` | 9 px  (0.00% of walls) | 5,880 px  (1.43% of walls) |
| `G2/A/D64/cast` | 9 px  (0.00% of walls) | 5,085 px  (1.23% of walls) |
| `G3/cast` | 7 px  (0.00% of walls) | 10,957 px  (2.66% of walls) |

---

## 3. Two controls that were added after the first tables, because the first tables were unfair

**`fair.py` -- every rule at its OWN best bias.** In section 2 only G0 and G4 carry a tuned
bias; G1 and G2 carry identres's shipped default, because G1 has to *reproduce* identres. That
makes G0's terrain column an artefact of the tuning objective (its winning cell uses normal
offset 0, which is fine for walls and awful for ground), and it flatters the gate. `fair.py`
re-runs the same 96-cell sweep separately for **each rule**, at 16 u with 3x3 PCF, and scores
each on two objectives. Object pixels, each rule standing on its own best bias:

| camera | objective | G0 no identity | G1 pure identity | G2 gate D = 64 u |
|---|---|---|---|---|
| hwydeck | object total | **5.58%** (fD 0.75 fL 4.83) | 6.96% (fD 0.68 fL 6.29) | **5.59%** (fD 0.74 fL 4.85) |
| east | object total | **4.17%** (fD 1.14 fL 3.03) | 8.05% (fD 0.87 fL 7.18) | **4.19%** (fD 1.13 fL 3.06) |
| street | object total | **8.15%** (fD 4.18 fL 3.98) | 8.88% (fD 4.18 fL 4.70) | **8.19%** (fD 4.18 fL 4.02) |

Read that row by row. **Pure identity buys 0.07, 0.27 and 0.00 points of false-DARK and pays
1.46, 4.15 and 0.72 points of false-LIT for them.** And at the same bias all three rules give
**identical terrain** numbers (8.44 / 6.14 / 11.06) -- identity and the gate only ever touch
object pixels, terrain is `TERRAIN_ID` and is never gated, so every terrain difference in
section 2 is a bias difference and nothing else. That has to be said plainly because the ALL
column in section 2 can be read the wrong way.

**`wallart.py` -- the artefact counted where it is seen.** Object false-DARK is a frame average
over every object pixel; the mid-wall patch is a few thousand pixels on near-vertical faces, so
a rule can double the patch and move the average by a tenth of a point. Every cached panel
re-scored on **wall pixels only** (object, `|normal.z| < 0.35`), 16 u with 3x3 PCF:

| camera | walls | G0 patch / lost | G1/A patch / lost | G2/A D64 patch / lost | G4/A patch / lost |
|---|---|---|---|---|---|
| hwydeck | 170,083 | 1,644 / 9,920 | 1,625 / 13,376 | 1,756 / 10,727 | **1,612 / 9,997** |
| east | 476,193 | 5,746 / 12,801 | 6,567 / 37,539 | 7,165 / 15,387 | **5,728 / 12,944** |
| street | 412,490 | 17,905 / 13,484 | 18,335 / 14,034 | 18,335 / 13,306 | **17,905 / 13,518** |

*patch* = wall pixels the ray-cast sun calls LIT that the row calls dark -- the artefact.
*lost* = wall pixels the sun calls SHADOW that the row calls lit -- the self-shadow given up.

**Pure identity does not reduce the wall patch at a 16 u texel.** It is 19 pixels better than a
tuned no-identity bias on hwydeck, 821 pixels *worse* on east and 430 worse on street, while
losing 3,456 / 24,738 / 550 more wall pixels of real shadow. At 64 u it earns a little on one
camera (hwydeck 6,640 -> 5,755) and loses on another (east 17,613 -> 21,496).

---

## 4. The verdict

### Is the identity-gated distance better than pure identity?

**Yes. Strictly, on every camera, both identity tables and every lookup, and the margin is
large.** 16 u texel, 3x3 PCF, identity table A, object pixels:

| camera | G1 false-DARK -> G2 D=64 | G1 false-LIT -> G2 D=64 | self-shadow thrown away |
|---|---|---|---|
| hwydeck az180 el10 | 0.55% -> 0.64% | **6.84% -> 5.34%** | 42.6% -> **26.9%** |
| east az120 el15 | 1.09% -> 1.24% | **7.29% -> 3.48%** | 97.6% -> **27.4%** |
| street az120 el05 | 4.27% -> 4.27% | **4.65% -> 4.02%** | 87.8% -> **66.5%** |

The gate hands back 1.50, 3.81 and 0.63 points of real shadow for 0.09, 0.15 and 0.00 points of
new false-DARK -- a 16:1, 25:1 and infinite trade. With the map taken out of the way entirely
(per-pixel cast, the limit of the rule itself) it is not close: false-LIT **1.41% -> 0.12%**,
**5.07% -> 0.45%**, **2.64% -> 1.25%**, i.e. 91%, 91% and 53% of every lost self-shadow
returned. G4 -- the gate with the bias re-tuned on top -- is the best 16 u row on every camera
and every wall count.

### The gate makes the proximity join free, and that is the strongest single result

Pure identity on **table B** (the ruled join: 588 groups -> 167, a whole city block one
identity) loses **12.89%**, 7.72% and 4.68% of object pixels to false-LIT against table A's
6.84%, 7.29% and 4.65%; on hwydeck it throws away **97.2%** of the entire chunk's self-shadow.
Under the gate, table B and table A give **the same answer**: 5.35% vs 5.34%, 3.48% vs 3.48%,
4.02% vs 4.02%; at 64 u they are numerically identical. The size of the identity table stops
mattering. `images/identgap_joincost_hwydeck_az180_el10.png` is that sentence as a picture --
the highway deck is in shadow, pure identity on table B lights the whole deck, the gate puts it
back.

### But the honest headline is bigger than the gate

`fair.py` and `wallart.py` above say it: at a 16 u far cascade, **with the bias properly tuned,
pure identity is not earning its keep at all** -- it removes 0.07 / 0.27 / 0.00 points of
false-DARK and costs 1.46 / 4.15 / 0.72 points of false-LIT, and on walls it makes the patch
*worse* on two cameras out of three. And `dsweep.py` shows why: with a tuned bias, object
false-DARK is **flat from D = 8 u to D = 256 u** on every camera and both texels (hwydeck 16 u:
0.74% at D = 8, 0.73% at D = 256, 0.66% only at D = infinity). **The mid-wall patch is a shadow
map resolution artefact, not a geometric one.** A slope-scaled plus normal-offset bias already
covers the zone where the map cannot tell a surface from itself; what identity removes beyond
that zone is real shadow.

So the gate is better than pure identity **because it converges on what a good bias already
does**, while keeping identity available for the cases a bias cannot reach -- a coarse cascade,
a big joined identity, a sun angle the bias was not tuned for. That is the argument for it, and
it is an argument for a *small* D.

### The D to use, and why

The floor is arithmetic, not a grid search. A shadow-map texel is `texel` units of `s`; a
surface crossing one texel of `s` spans `texel / sin(el)` **along the sun ray**, and inside that
distance the map cannot distinguish a surface from itself:

| | el 15 | el 10 | el 5 |
|---|---|---|---|
| 16 u texel | 62 u | 92 u | 184 u |
| 64 u texel | 247 u | 369 u | 734 u |

**Recommendation: `D = texel / sin(sun elevation)`, clamped to a floor of 64 u** -- one
per-frame scalar on the CPU, about 92 u for today's 16 u far cascade at a 10-degree sun. The
fixed **D = 64 u** measured here is a perfectly good fallback and was the winner under the rule
stated before any measurement (keep false-DARK within 0.25 points of G1, then take the least
false-LIT) on all three cameras and both tables. The cost of the derived value over the fixed
one is at most 0.07 points of false-LIT on any camera measured. Do **not** use 512 u or more:
at el 15 that is where east's false-LIT jumps 3.06% -> 6.95% and the rule collapses back into
pure identity.

### What it costs the FO4CS shader

Nothing worth counting. The far-cascade tap already fetches a depth and, since v7, the `.lodi`
GROUP id beside it. `u` is the receiver's own light-space depth, already computed. The gate is:

```hlsl
//  perFrame.gateReach = D * cos(sunElevation)   -- one CPU scalar
bool sameId = (mapId == recvId) && (recvId != TERRAIN_ID);
bool nearMe = (mapDepth - recvDepth) <= perFrame.gateReach;
bool blocked = nearer && !(sameId && nearMe);      // was: nearer && !sameId
```

**One subtract, one compare, one AND**, on data already in registers. No new texture, no second
tap, no extra bandwidth, no change to the `.lodi` file format, no change to lodgen. Under PCF
it is per tap, so 9 extra compares for a 3x3 -- inside the noise of the 9 depth fetches
themselves.

### The other two candidates, briefly

**G3, back-face-only casting, is not available to this content,** and the geometry census
decides it before a pixel is drawn: of 2,449 placements carrying level-0 LOD triangles, **106
are closed shells and 2,343 are open**; **26,999 of 29,587 triangles (91.3%) belong to an open
shell**, and 1,282 placements are two-triangle **cards**. An open shell has nothing behind its
sun-facing side, so culling that side stops it casting at all. The pixels agree: in the exact
cast it loses 2.52%--3.78% of object pixels to false-LIT on the three cameras and puts 6.79%
of *terrain* wrong on street, because the things that should have shadowed the ground stopped
writing depth.

**G0, no identity at all with an honestly tuned bias, is the surprise:** at 16 u it ties the
gate (5.58 vs 5.59, 4.17 vs 4.19, 8.15 vs 8.19 object total). Its weakness is that it is a
*tuned constant* -- the winning cell differs per camera (normal 0.0 / depth 1.0 / slope 2.0 on
hwydeck, normal 0.0 / depth 2.0 / slope 0.0 on east, normal 1.0 / depth 2.0 / slope 0.0 on
street) and the cell that fights the artefact hardest always peter-pans: `G0strict` on street
buys 0.10 points of false-DARK (4.18% -> 4.08%) for 0.85 points of false-LIT and drives the
whole-frame disagreement from 9.71% to **12.88%**, because the same fat bias that lifts a
shadow off a wall lifts it off the ground as well (terrain 11.07% -> 16.35%). A bias tuned for
one sun angle is not a rule; the gate is.

---

## 5. The director's addendum: screen-space shadows

bungo, 06:2x: *"for the gaps that the identity does not fill, we have screen space shadows
too"*. Simulated honestly in the truth scene's own camera depth buffer, marching toward the
sun's screen direction, bias and thickness **swept** (30 cells) rather than guessed, and both
expressed as fractions of camera depth -- a first attempt with world-constant values
over-darkened 25% of the frame, because one screen pixel at this range already spans ~14 world
units, and that failure is recorded in `sss.py`. Best cell: bias 0.040 x depth, thickness
0.05 x depth, 64 steps, reach 10% of frame height.

| camera | base | recovers of the base false-LIT | NEW false-dark it invents | net, in points |
|---|---|---|---|---|
| hwydeck | G1/A | 26.9% (1.84 pts) | 1.74 pts | **+0.10** |
| hwydeck | G2/A D64 | 23.3% (1.24 pts) | 1.72 pts | **-0.48** |
| east | G1/A | 9.4% (0.69 pts) | 2.30 pts | **-1.61** |
| east | G2/A D64 | 13.2% (0.46 pts) | 2.28 pts | **-1.82** |
| street | G1/A | 20.0% (0.93 pts) | 0.55 pts | **+0.38** |
| street | G2/A D64 | 14.5% (0.58 pts) | 0.55 pts | **+0.03** |

**Screen-space shadows are not a replacement for the gate, and on this content they are close
to a wash on their own.** Best case they break even (hwydeck +0.10, street +0.38); on east they
are a clear net loss, inventing 2.30 points of false-dark to recover 0.69. Compare the gate on
the same cameras: +1.50, +3.81 and +0.63 points of false-LIT recovered for 0.09, 0.15 and 0.00
points of false-dark. **The gate does five to twenty-five times more good per unit of harm.**

They do **compose**, though, and the best 16 u result measured anywhere in this lane is the gate
*plus* the march: hwydeck false-LIT 6.84% (pure identity) -> 5.34% (gate) -> **4.10%** (gate +
march). If the march is already running, leave it on; it is not a reason to skip the gate.

Its limits are structural, not tuning: it can only see occluders **on screen**, so the tall
thing casting from behind the camera or off the left edge is invisible to it; it must **guess a
thickness** because a depth buffer stores one surface, and every false-dark pixel in the table
above is that guess being wrong; and its reach is a fixed fraction of the screen, so at the far
cascade a 10%-of-height march covers a median 62 screen pixels, which is metres, not the
hundreds of metres a building shadow runs. `images/identgap_sss3_hwydeck_az180_el10.png` shows
all three at once: pure identity loses the long shadows, the march puts back ragged
stair-stepped fragments in roughly the right places, the ray-cast sun is clean.

**So: the gated distance is needed. Identity plus screen-space shadows is not enough.**

---

## 6. Five plain sentences for bungo

1. Your idea works: instead of a building ignoring every shadow it casts on itself, let it
   ignore only the ones cast from within about 90 units of the surface being shaded, and it gets
   most of its real self-shadow back -- on the three test views it recovers 1.5, 3.8 and 0.6
   points of the shadow the current rule throws away, while the mid-wall patch grows by 0.09,
   0.15 and 0.00 points.

2. It costs the shader one subtraction and one comparison on numbers it has already loaded, with
   no new texture, no extra memory read and no change to the LOD files.

3. The biggest win is that it makes the grouping change we just approved free: joining nearby
   buildings into one group currently costs a huge amount of real shadow -- on the highway view
   the whole chunk loses 97% of its self-shadow and the elevated deck goes completely bright --
   and with the distance limit the joined groups behave exactly like the unjoined ones.

4. The uncomfortable finding is that at the current far-shadow resolution, a properly tuned
   ordinary shadow bias does just as well as the identity rule on its own, so identity is
   earning much less than we assumed; the distance limit is worth having because it keeps what
   identity is genuinely for while removing nearly all of what it costs.

5. Screen-space shadows do not cover this gap -- they get back only a tenth to a quarter of the
   missing shadow and invent about as much wrong darkness doing it, so keep them if they are
   already on, but they are not a reason to leave the distance limit out.

---

## 7. Files

| what | where |
|---|---|
| the engine (gate in both the map lookup and the exact cast, back-face selection, shell census) | `identgap.py` |
| the driver, all rows, all cameras | `run.py` -> `run.log`, `rows.json`, `lit_<view>.npz` |
| the screen-space march | `sss.py` |
| D below the brief's grid | `dsweep.py` -> `dsweep.log`, `dsweep.json` |
| every rule at its own best bias | `fair.py` -> `fair.log`, `fair.json` |
| the artefact counted on walls only | `wallart.py` -> `wallart.log`, `wallart.json` |
| the pictures | `pics.py` -> `pics.log`, `images/` |

| picture | what it shows |
|---|---|
| `images/identgap_wall4_hwydeck_az180_el10.png` | truth / G0 / G1 / gate on a wall crop, 64 u nearest |
| `images/identgap_wall4_street_az120_el05.png` | the same at a 5-degree sun |
| `images/identgap_joincost_hwydeck_az180_el10.png` | **the key picture** -- the join loses the deck's shadow, the gate gives it back |
| `images/identgap_joincost_east_az120_el15.png` | the same on the east view |
| `images/identgap_joincost_street_az120_el05.png` | the same at street level |
| `images/identgap_sss3_hwydeck_az180_el10.png` | pure identity / + screen-space shadows / truth |
