
## 1. Vanilla's road law (opacity, edge, detail, hue)

All bakes for this section were made on the RUNG exe
(`release/NifSkope.before_roads3.exe`, a byte copy of the launch exe, md5
`6af74b4b4667ce50c4506a2d42a04fdf`), into `scratchpad/roads3_20260911/out/`
only, 03:37-03:39. Two 4x4-cell regions: (-20,20) Sanctuary and (-8,8)
downtown, the highway tile ROADS2 used. His installed `Data\Terrain` was never
touched. Four variants a tile: shipped defaults, `--no-roads` (the ground
alone), `--road-detail 1`, `--roads-legacy`.

### 1.0 The instrument, tested on known answers first -- 14 of 14

`r3_selftest.py` -> `logs/s0_selftest.txt`. Every check has the other side of
its floor, and the floors were watched firing: a synthetic sheet built AS the
law returns the planted opacity to 4 decimals (0.3500 / 0.6000 / 0.9000) with a
residual of 0.0000; a planted per-texel ramp comes back with a maximum error of
0.0000 and an edge profile that rises 0.100 -> 0.800; a sheet built NOT as the
law (a third, unrelated colour) leaves 95.1 % of what it is asked to explain
unexplained; shuffling the ground takes a perfect fit from 0.0000 to 0.3268;
the mask reads 0 texels different from the painted set and 0 when nothing was
painted; the step detector reads 0.0000 on a planted smooth ramp and 8.0000 on
a planted 8-level step.

One assumption is load-bearing and is CHECKED, not assumed: on these chunks the
sheet's RGB on a road texel **is** the road plane's RGB, because the grass tint
never fires (GRADE1's red 1 -- the cover plane is empty, `dwReserved1 = 0`) and
`--grade` is 1.0 with the multiply branched over. The control is that
`--road-detail 0` and `--road-detail 1` differ on **23,270 of 25,249** mask
texels on (-20,20) (mean 8.15 levels, p90 16.45) and **11,073 of 12,182** on
(-8,8) -- which cannot happen if the sheet were not carrying the paint.

### 1.1 THE BRIEF'S OPACITY LAW IS REFUSED BY ITS OWN FLOOR

The law under test was `vanilla = a * ourPaint + (1 - a) * ourGround`, fitted
per texel by least squares over the three channels. Result, with the floor the
brief pre-registered beside it:

| tile | road texels | `a` median | unexplained | FLOOR: ground shuffled | CEILING: vanilla's neighbour sheet |
|---|---|---|---|---|---|
| (-20,20) | 25,249 (24,045 read) | 0.735 | **17.6 %** | **18.2 %** (sd 0.1) | 18.1 % |
| (-8,8) | 12,182 (9,606 read) | 0.104 | **52.6 %** | **51.5 %** (sd 0.1) | 39.2 % |

The fit is **indistinguishable from its own floor on (-20,20) and worse than it
on (-8,8)**. A per-texel opacity is therefore not identifiable, and the reason
is already in the record: TILING2 measured our sheet and vanilla's at r = 0.034
and 0.314 texel to texel, so our ground does not predict vanilla's colour
anywhere, road or not. The spread of `a` says the same thing out loud -- sd
1.414 and 1.789, p10 0.258 and -2.000, p90 1.334 and 2.175, values an opacity
cannot take. The alignment control rules out a registration excuse: the best
shift over a +-3 texel sweep improves the fit by 0.005 and 0.005 (0.1711 at
(3,-3) against 0.1763 at (0,0); 0.5203 at (-2,-3) against 0.5256), so the mask
does name vanilla's road and the fit still fails.

**Nothing was built on this law.** The question was then asked of vanilla
ALONE, with no reference to our bake at all.

### 1.2 WHAT VANILLA'S ROAD ACTUALLY IS: a wash that follows the ground it lies on

`r3_law.py` -> `logs/f1_law.txt`. Each road texel's luminance regressed on the
mean luminance of the NON-road texels in a disc of 8 texels (256 world units)
around it, on the same sheet. Slope ~1 means the road follows the ground;
slope ~0 means a fixed paint that ignores it.

| tile | field | slope | corr | road L | local ground L | rise |
|---|---|---|---|---|---|---|
| (-20,20) | **vanilla** | **+0.714** | +0.442 | 92.02 | 91.00 | +1.02 |
| (-20,20) | ours (rung) | +0.339 | +0.300 | 88.30 | 71.45 | +16.85 |
| (-20,20) | our ground (the control) | +0.637 | +0.562 | 66.06 | 71.45 | -5.39 |
| (-8,8) | **vanilla** | **+0.755** | +0.472 | 94.17 | 92.26 | +1.91 |
| (-8,8) | ours (rung) | +0.209 | +0.088 | 106.09 | 103.80 | +2.29 |
| (-8,8) | our ground (the control) | +0.565 | +0.338 | 102.13 | 103.80 | -1.67 |

Floors, both sides, on the same texels: the local-ground field TRANSLATED by a
large random shift reads slope **-0.009** (worst |slope| 0.298) over 5 draws; a
known-answer road of a FIXED colour built from these very sheets reads
**+0.000**; a known-answer road of `ground + 4 levels` reads **+1.000**.

Read it against the ground's own self-slope, which is the honest reference for
"as coupled to its neighbourhood as ground is": vanilla's road tracks its
neighbourhood **at least as strongly as unpainted ground does** (0.714 vs
0.637; 0.755 vs 0.565), while ours tracks it at **half** that (0.339 vs 0.637;
0.209 vs 0.565). Vanilla's far road is not a painted ribbon at all. It is the
ground with a small rise on it.

### 1.3 The rise is the same on two very different tiles, and ours is not

Road mean minus the mean of the 1..8 texel band outside the mask:

| tile | vanilla | ours (rung) | our ground (control) |
|---|---|---|---|
| (-20,20) | **+4.29** | **+29.96** | -8.13 |
| (-8,8) | **+4.40** | **+3.84** | -0.72 |

Vanilla's rise is +4.29 and +4.40 on two tiles whose grounds differ by a whole
biome. **Ours is right on (-8,8) (-0.56 of a level from vanilla's) and 25.67
levels too contrasty on (-20,20)** -- the tile bungo looked at. The cause is in
the same table: our paint is a fixed material colour (99.05 and 106.68) while
our ground swings 60.96 -> 102.12 between the two tiles. Where the ground
happens to sit near the paint the road looks right; where it is 38 levels
darker the road is a stripe.

### 1.4 THE HUE IS NOT THE DEFECT -- named, with the numbers

`r3_chroma.py` -> `logs/f1_chroma.txt`. Road minus surround, per channel and on
the two opponent axes (b_y = B - (R+G)/2, positive = bluer; r_g = R - G):

| tile | field | dR | dG | dB | d(b_y) | d(r_g) | dL | d(sat) |
|---|---|---|---|---|---|---|---|---|
| (-20,20) | vanilla | +2.65 | +4.79 | +6.02 | **+2.30** | **-2.14** | +4.29 | -0.037 |
| (-20,20) | ours | +27.48 | +30.73 | +32.45 | **+3.35** | **-3.25** | +29.96 | -0.120 |
| (-8,8) | vanilla | +1.96 | +5.03 | +7.59 | **+4.10** | **-3.07** | +4.40 | -0.053 |
| (-8,8) | ours | +2.82 | +4.06 | +5.40 | **+1.96** | **-1.24** | +3.84 | -0.027 |

Vanilla's road goes bluer and less red than the ground around it, and so does
ours, in the same direction, differing by **1.05 and 1.11 levels** on (-20,20)
and **2.14 and 1.83** on (-8,8) -- every one of the four inside the gate's 3
levels. Measured on the road texels themselves rather than as a rise, (-20,20)
reads vanilla b_y -12.62 against ours -12.39 and saturation 0.162 against
0.161.

So *"vanilla's cul-de-sac is a soft desaturated blue-grey blob and ours a crisp
warm pale-grey ribbon"*, the thing ROADS2 owed a number for, is **not a hue
difference**. It is the luminance rise: +4.29 against +29.96. Nothing routes to
a hue knob and nothing is a lighting term.

**Where the road is graded**, since the brief asks: the road is composited into
the colour BEFORE `--grade` (`src/lodgen.cpp:7923` and `:9235`, the grade at
`:7958` and `:9257`), so it is graded exactly once, with the ground it sits in,
and at the shipped default of 1.0 the multiply is branched over entirely. The
order is right; nothing needed fixing there.

### 1.5 Vanilla keeps NO road-texture detail -- `--road-detail` stays 0

The residual after the best wash, correlated with our full-detail bake's own
departure from its flat average (`--road-detail 1` minus `--road-detail 0`),
against a floor that is the SAME signal with its phase broken
(`splatlib.phase_twin`, 5 seeds):

| tile | blur | correlation | FLOOR mean / max | best strength |
|---|---|---|---|---|
| (-20,20) | r=0 | +0.0275 | 0.0270 / 0.0644 | -0.212 |
| (-20,20) | r=2 | +0.0404 | 0.0609 / 0.1504 | -1.422 |
| (-8,8) | r=0 | +0.0162 | 0.0124 / 0.0163 | -0.153 |
| (-8,8) | r=2 | -0.0022 | 0.0296 / 0.0695 | -2.274 |

**The correlation IS the floor**, at every blur and on both tiles, and the
best-fit strength is NEGATIVE wherever it is anything at all. There is no detail
to put back. `--road-detail` is not re-fitted; 0 stays the default and 1 stays
the way back.

### 1.6 Vanilla's road EDGE cannot be resolved, and that is the answer

The brief asks for the 10-90 % transition width on vanilla against ours. It is
refused as a width, with the number: vanilla's road stands **4.29 levels** above
the ground while the local 5x5 SD of that ground is **6.59 levels**, so the edge
sits at a signal-to-noise of **0.65** and any width read off it is a reading of
the terrain's own texture. TILING2's addendum already said this from the other
side (both widths 6.00 texels, vanilla's profile range 23.26 levels of which
+3.92 is the road).

What CAN be read is the profile itself, mean luminance against signed distance
to the mask edge, (-20,20):

| d | -3 | -1 | +1 | +2 | +3 | +4 | +6 | +8 | +10 | +12 |
|---|---|---|---|---|---|---|---|---|---|---|
| vanilla | 88.48 | 89.72 | 92.70 | 92.57 | 92.54 | 92.18 | 92.23 | 91.40 | 91.48 | 91.58 |
| ours | 69.93 | 70.78 | 79.86 | 86.48 | 93.45 | 96.53 | 96.47 | 98.20 | 105.30 | 105.56 |
| our ground | 69.93 | 70.78 | 73.25 | 72.57 | 70.71 | 67.98 | 63.41 | 58.02 | 54.91 | 54.24 |

Vanilla reaches its full value at d = +1 and is FLAT across the whole width.
Ours ramps over three texels (79.86, 86.48, 93.45), plateaus near 96.5, then
climbs again to 105.5 in the core. **That is the two-tone bungo sees, and it is
a darker outer band around a brighter core, not the lighter band the brief
guessed.** The outer band is the alpha-blended skirt (ROADS2's 76 vertex-alpha
shapes) mixing the paint into a ground 39 levels darker; the brighter core is
the wider trunk material. The biggest step inside the road (max second
difference of the profile) is **3.88 at d=3 for ours against 1.31 for vanilla**.

On (-8,8) the same statistic reads **ours 1.25 against vanilla's 4.43** -- ours
is already smoother than vanilla there. The step and the flatness are not two
defects; they are the same 25.67 levels of contrast, seen twice.

### 1.7 EVERY CANDIDATE RULE SIMULATED BEFORE THE BUILD

`r3_sim.py` -> `logs/f2_sim.txt`. The generator's composite is
`colour = ground + (paint - ground) * a` (`lodgen.cpp:7923`), and on these
chunks the tint and the grade are both inert, so this is the arithmetic the C++
will do, not an approximation of it. Vanilla: road L 92.52, rise +4.29, SD 6.59,
step 1.31 on (-20,20); 94.59, +4.40, 6.44, 4.43 on (-8,8).

(-20,20):

| rule | road L | dL vs vanilla | rise | SD / vanilla's | step |
|---|---|---|---|---|---|
| rung (a = 1) | 99.05 | **+6.53** | +29.96 | 1.10 | 3.88 |
| flat a = 0.750 | 89.53 | **-3.00** | +20.43 | 0.88 | 3.13 |
| flat a = 0.500 | 80.01 | -12.52 | +10.91 | 0.71 | 2.37 |
| flat a = 0.326 | 73.38 | -19.15 | **+4.28** | 0.64 | 1.85 |
| flat a = 0.250 | 70.48 | -22.04 | +1.39 | 0.62 | 1.62 |
| ground-relative, rise 1.02..3.00 | 67.3..68.0 | -25.2..-24.5 | -1.8..-1.1 | 0.78 | 2.87 |

(-8,8):

| rule | road L | dL vs vanilla | rise | SD / vanilla's | step |
|---|---|---|---|---|---|
| rung (a = 1) | 106.68 | **+12.09** | **+3.84** | **0.84** | **1.25** |
| flat a = 0.750 | 105.54 | +10.95 | +2.70 | 0.71 | 1.13 |
| flat a = 0.500 | 104.40 | +9.81 | +1.56 | 0.65 | 1.02 |
| flat a = 0.250 | 103.26 | +8.67 | +0.42 | 0.68 | 0.96 |
| ground-relative, rise 1.02..3.00 | 104.6..105.3 | +10.0..+10.8 | +1.7..+2.5 | 0.48 | 1.26 |

Two things fall out, and both are refusals with arithmetic.

1. **On (-8,8) NO opacity can meet gate F3a.** The composite can only land the
   road somewhere between our ground (102.12) and our paint (106.68). Vanilla's
   road is at **94.59**, outside that interval by 7.53 levels. The gate asks the
   road pass to close a gap that does not live in the road. The same holds for
   the hue read as an absolute rather than as a rise: d(b_y) is -6.5 to -6.7 for
   every rule including the rung, because our GROUND's b_y on that tile is
   -12.79 against vanilla road's -6.25.
2. **On (-20,20) the gates are mutually exclusive by 22 levels.** Matching
   vanilla's absolute level needs a = 0.83; matching vanilla's rise over the
   ground needs a = 0.326; matching vanilla's step needs a <= 0.25; keeping the
   local SD inside 20 % of vanilla's needs a >= ~0.75. There is no a that does
   two of them.

The 22 levels are named and they are not this lane's: our ground on (-20,20) is
19 levels darker than vanilla's (68.69 against 83.52, ROADS2's own number; the
surround here reads 69.10 against 88.24), and GRADE1 measured that this is a
per-cell CONTENT difference with a near-zero mean -- not an exposure, a gamma, a
colour-space slip or a lighting term, and explicitly NOT a tone curve. TILING2's
addendum said the same in advance: *"Do not chase all 25 levels with the road
pass."* This lane does not.

**The ground-relative rule is rejected with its numbers and does not ship.** It
was the obvious implementation of 1.2 -- pick the opacity per texel so the
result sits a fixed rise above the local non-road ground -- and it loses to a
flat opacity on every row of both tables: on (-20,20) it lands the road 25
levels below vanilla with a rise of MINUS 1.8, because more than half its texels
clamp to a = 0 (the ground under the road there is brighter than the disc mean
for the majority of texels while the mean is dragged down by a dark minority),
and on (-8,8) it halves the local SD to 0.48 of vanilla's. Kept in the record so
it is not proposed again without new evidence.

**Gate F1 verdict: the law is fitted, with its floor and its ceiling. Vanilla's
road is a wash that follows its local ground with a +4.3-level rise; the brief's
per-texel opacity form of it is refused by its own floor; the hue is measured
and is not a defect; the detail strength is measured and is zero; the edge width
is refused as unresolvable at a signal-to-noise of 0.65.**
