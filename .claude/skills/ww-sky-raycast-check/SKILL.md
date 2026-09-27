---
name: ww-sky-raycast-check
description: Choose or check a ground sky/AO law (terrain march, object height-field march, union vs product, step spacing, strength) by scoring it against a brute-force ray cast through the placed objects' LOD triangles on a sample grid. Use before changing how the VT mask B / sky AO byte sees objects, or when someone asks whether an occupancy march is good enough.
---

# Ground sky law vs a ray cast

A sky-occlusion law is a guess about how much sky a texel sees. The reference is a ray cast through the real
geometry. Score the candidate laws against that reference before you ship one. Do not tune a law by looking at
a picture.

## Inputs (all offline, no NifSkope run needed)
- **Height sheet:** a `.lodt` VT.2 file from any Boston bake. Heights do not depend on the sky law.
- **Object LOD triangles:** a native `.lodo`/`.lodi` pair (for example ao2 `reg_x7`). Read them with
  `tests/spells/lodgen_native_decode.py` and pick level-0 geometry with `lodl_channels_table.drawn(slot -1, level 0)`.
- **Object height lattice:**
  - Best: the bake's own `--dump-object-ao FILE` (OBJH: max plane, then min plane, rows south to north).
  - If no bake can run: build a stand-in from the same LOD triangles, with max/min z per 128-unit square. Say
    plainly that it is a stand-in, and re-run on the real dump when you can.

## Method (lane TERR1, scripts in `scratchpad/terr1_20260927/`, not in git)
1. **`skycast.py <sheet> <objh> <out.json> [step] [J]`**
   - Samples lie on a grid 512 units apart. Keep them far enough inside the box that the 1,458-unit reach stays
     in the box.
   - Class each sample:
     - `under`: an object square stands above the texel. Exclude these.
     - `near`: something 64+ units tall is within reach.
     - `open`: nothing is.
   - The reference casts 8 directions × J elevations, spaced uniformly in F = t/(1+t), which is the march's own
     measure. Each ray starts 1 unit above the ground and runs to 1,458 units. It hits the terrain (bilinear, every
     16 u) or a triangle (Möller–Trumbore, triangles binned at 256 u).
   - Score each direction by its blocked fraction, then vis = clamp(1 − 1.6·Σ/8).
2. **`skyfit.py <out.json>`** reports MAE, bias and correlation, per class and in 0..255 levels. It covers:
   - the terrain march alone;
   - product A(s);
   - union B(s) at the 7 march steps;
   - union Bd(s) with the lattice read every 64 u;
   - union Be(s) = Bd plus the two fixes below (the shipped law, `lodgenSkyDirBlocked`).
3. **Two self-checks must pass before you trust any score:**
   - The Python terrain march must reproduce the sheet's mask B (TERR1: MAE 3.96 levels, corr 0.988).
   - On `open` samples the terrain ray cast must equal the union reference (MAE 0.09).
4. Score the `lifted` class on its own (samples whose own lattice square is 0..128 u above the ground: road
   pieces). A law that fails only there is reading low covers as ceilings.

## What TERR1 measured (Boston, stand-in lattice 512 u grid, J = 32, 2,208 near samples)

| law | MAE | bias | corr |
|---|---|---|---|
| terrain only | 87.4 | +87.2 | 0.07 |
| product, s = 1 | 32.6 | +7.4 | 0.741 |
| union, 7 steps | 32.3 | +9.4 | 0.752 |
| union, dense 64 u (no lift, no bar) | 30.5 | -0.4 | 0.755 (lifted samples: bias -101) |
| **Be: dense + lift + 128 bar (shipped)** | **22.3** | **-13.0** | **0.905** |

- Deck class (909 samples) under Be: 15.9 / -15.9 / 0.779.
- Any strength other than 1 on the union moves open ground (bias +16.7 at s = 0.75). A union takes no strength dial.
- An earlier, smaller run (2,095 samples, 1,024 grid) had the dense union at 26.3 / +5.2 / 0.834; the table above
  is the one the shipped law was chosen on.

## Traps
- A strength that scales the union also scales the terrain term, so it breaks "open ground unchanged". Only a
  product can take a dial.
- The 7-step march (128·1.5^k) steps over a street's far wall in a 128-unit lattice. Read the lattice densely.
- Road pieces sit a few units over the terrain. Marched from the terrain height, the slab law reads them as
  ceilings and a street goes black. Two fixes, both shipped: start the march from the texel's own square top when
  it is 0..128 u above the ground (`lodgenSkySurface`), and call a square a wall when its bottom is within 128 u
  of that start.
- Check the stand-in lattice's row order against the OBJH doc: rows run south to north, `gy = floor(y/128) - gy0`.
