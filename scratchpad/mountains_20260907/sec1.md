
---

## 1. OVERSEER'S OWN MEASUREMENTS (not delegated — I reproduced every one)

### 1.1 The `_msn` up-axis, settled

The brief guessed "up appears to be GREEN" from a single mean. That guess is
**correct**, but the reasoning offered for it was not sufficient, and my own
first attempt to confirm it was worthless — see REFUTED below.

The test that decides it (`msn_updecide.py`) uses two properties a terrain
normal must have and a horizontal component cannot:

* **Determinism.** The up component is not free: `up = +sqrt(1 - x^2 - y^2)`.
  Predict each channel from the other two and measure the residual. The true up
  channel has a small residual; a horizontal channel does not, because its sign
  is unrecoverable.
* **One-sidedness.** Terrain never faces downward, so the up channel must never
  encode a negative number — never below 128.

`python msn_updecide.py`, mip 2 (128x128), full decode:

    tile                          mean |predicted-actual| (bytes)  fraction of pixels < 128
                                     R      G      B                R      G      B
    Commonwealth.4.-20.24_msn       40.7    5.4   71.2            0.543  0.000  0.839
    Commonwealth.4.-20.60_msn       62.3    5.2   57.5            0.655  0.000  0.785
    Commonwealth.4.-60.60_msn       15.3    7.2   48.0            0.042  0.000  0.481
    Commonwealth.4.0.0_msn          38.8    5.6   42.5            0.615  0.000  0.696
    Commonwealth.4.-20.80_msn       14.0    1.0   13.4            0.779  0.000  0.820
    Commonwealth.8.-24.24_msn       64.1   10.7   61.0            0.618  0.000  0.597
    Commonwealth.16.-16.0_msn       56.5   15.8   64.0            0.398  0.000  0.676
    Commonwealth.32.-96.-96_msn     40.7    6.1   39.1            0.608  0.000  0.653

Green wins both tests on **every tile, at every LOD level**: residual 1.0-15.8
bytes against 13-71 for red and blue, and **exactly zero pixels below 128 in
green** while red and blue fall below 128 for 4%-84% of pixels.

**SETTLED: vanilla FO4 terrain LOD `_msn` is `R = X (east), G = UP, B = Y
(north)`, all three channels 0.5+0.5 encoded.** Not "z direct" — see REFUTED.
Note this also puts the up axis in the 6-bit channel of RGB565, the only one
with extra precision, which is what a careful encoder does.

### 1.2 The repo's own writer puts UP IN BLUE — a real, first-order mismatch

Traced through the code, not guessed:

* `src/lodgen.cpp:5021-5027` — `Vector3 nrm( -dzdx, -dzdy, 1.0f ); nrm.normalize();`
  so `nrm[0]` is east, `nrm[1]` is north, `nrm[2]` is **up**. Then
  `msn[...] = 0xFF000000U | quint32( nr << 16 ) | quint32( ng << 8 ) | quint32( nb );`
  with `nr` from `nrm[0]`, `ng` from `nrm[1]`, `nb` from `nrm[2]`.
* `src/lodgen.cpp:3591-3595` — `lodgenPack565( quint32 bgra )` reads
  `r = (bgra >> 16) & 0xFF`, `g = (bgra >> 8) & 0xFF`, `b = bgra & 0xFF` and
  packs `(r>>3)<<11 | (g>>2)<<5 | (b>>3)`, so **bits 16-23 land in RED5**.
  Corroborated at `src/lodgen.cpp:3621-3622`, where that same `>>16` field is
  the one multiplied by the 0.299 luma coefficient — red's.
* `src/lodgen.cpp:3812` routes every colour block through that packer and
  `src/lodgen.cpp:5285` writes the result as `<tile>_msn.DDS`.

So this fork emits **R = east, G = north, B = up**, where FO4 wants
**R = east, G = up, B = north**. Green and blue are transposed.

UNVERIFIED: whether xLODGen or DynDOLOD make the same transposition. That is
lens 4's question and I have not measured their output. **This finding is about
bungo's own fork** and must not be dressed up as the answer to his DynDOLOD
question unless lens 4 finds matching output.

### 1.3 What that swap costs, simulated on real vanilla data

`python swap_sim.py` — decode each vanilla `_msn`, recover the true surface
normal with the settled convention, re-encode it the way a swapping writer
would, and shade both with the same sun. Sun azimuth 225, elevation 45,
`light = 0.25 ambient + 0.75 * max(0, n.L)`:

    tile              CORRECT mean/std     SWAPPED mean/std     light      variation
    4.-20.24           0.830 / 0.111        0.250 / 0.005       -69.8%      -95.2%
    4.-20.60           0.837 / 0.135        0.251 / 0.009       -70.0%      -93.2%
    4.-60.60           0.562 / 0.175        0.251 / 0.007       -55.4%      -96.1%
    4.0.0              0.792 / 0.110        0.252 / 0.022       -68.2%      -79.7%
    4.-20.40           0.833 / 0.117        0.251 / 0.010       -69.9%      -91.6%
    16.-16.0           0.781 / 0.065        0.250 / 0.002       -68.0%      -96.4%
    32.-96.-96         0.794 / 0.041        0.250 / 0.005       -68.5%      -88.4%
    MEAN               0.776 / 0.108        0.251 / 0.009       -67.7%      -92.0%

The swapped result is **0.250-0.252 on every tile — the ambient constant to
three decimals**. The direct sun term is gone entirely. This is not sensitive to
my choice of sun angle: the mis-read normal points nearly horizontally north, so
`n.L` is at or below zero for essentially any sun above the horizon.

**That accounts for DARKER (-68% light) and FLATTER (-92% of shading variation)
directly, with numbers.** It accounts for DESATURATED only **indirectly, and I
label that step UNVERIFIED**: with the direct term gone the surface is lit by
ambient sky alone, which in FO4 is a cool blue-grey, so the warm light carrying
the scene's colour is removed and what is left tends toward one cool hue — which
is what bungo describes. I measured the collapse of the direct term. I did
**not** measure FO4's ambient colour or its fog curve, so the last step from
"ambient only" to "reads blue-grey" is reasoning, not measurement.

### 1.4 The vanilla diffuse is plain albedo — no sun, no AO baked in

This decides whether the `_msn` carries all the shading. `python baked_light.py`
pairs each texel of the diffuse with the same texel of the `_msn` (both 512x512,
same footprint) and hunts for a baked directional term:

    tile        alpha mean/min/max   corr(lum, up)   best n.L corr   at (az, el)
    4.-20.24      255.0 255 255         -0.374          -0.321        (75, 60)
    4.-20.60      255.0 255 255         -0.262          -0.281        (315, 60)
    4.-60.60      255.0 255 255         -0.631          -0.503        (180, 60)
    4.0.0         255.0 255 255         +0.067          -0.132        (315, 20)
    4.-20.40      255.0 255 255         -0.408          -0.332        (30, 60)
    16.-16.0      255.0 255 255         +0.071          +0.125        (330, 40)

**No sun is baked in.** The best-fitting direction is weak (|r| <= 0.50) and its
azimuth is incoherent across tiles — 75, 315, 180, 315, 30, 330 degrees. A baked
sun would agree on one azimuth across the whole worldspace. These do not.

The negative `corr(lum, up)` says steeper ground is *brighter*. That is a
material correlation, not a lighting one: cliff and rock textures are paler than
the dark vegetated flats. It is **not** baked ambient occlusion, which would
need concavity rather than slope and would darken the steep.

Also: **the diffuse's alpha is a constant 255 on every tile tested** — it
carries nothing. No mask, gloss or specular hides there, so a rebake that writes
opaque alpha loses nothing.

**Consequence: at distance essentially all terrain shading comes from the
`_msn`.** A correct diffuse cannot rescue a wrong normal map — which is exactly
why the corpus result in section 0 (far diffuse no brighter or more saturated
than near) is consistent with the mountains nonetheless looking completely
different.
