# Lane ROADS3 -- the far road as a soft wash, not a flat opaque ribbon

Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, nothing
committed. Exe at launch `release/NifSkope.exe` **2026-09-12 03:06:21,
21,489,152 B** (GRADE1's DONE exe), confirmed on disk at 03:34 by `ls -la`.
`Fallout4.exe` not running at 03:34; no NifSkope process of any kind at 03:34.
BUILDING marker `scratchpad/roads3_20260911/BUILDING` written 03:34:25 before
anything else.

## 0. Pre-registered gates

Copied from the brief BEFORE any number was taken, so a gate invented later is
visible as such (CONSTITUTION 1, rule 41).

| id | the gate, as the brief states it | the floor beside it |
|---|---|---|
| **F1** | vanilla's per-texel road opacity `a` fitted, with a floor and a ceiling, BEFORE any code; the residual's detail strength measured; the hue named by a measurement, not by a guess | floor = the same regression with the ground field shuffled (must return no fit); ceiling = vanilla against a neighbouring shipped tile |
| **F2** | every new switch at its off value == the rung's bytes, on both tiles, `cmp` every file; `.lodl` and `_msn` untouched at every setting | the compare must be shown able to FAIL (a switch at a non-off value moves the files it should) |
| **F3a** | road texels' mean luminance within 3 of 255 of vanilla's on both tiles | rung 101.59 vs vanilla 92.18 on (-20,20) |
| **F3b** | road hue within 3 of 255 of vanilla's | rung not measured; this lane measures it first |
| **F3c** | road local SD within 20 % of vanilla's 6.62 | rung 4.37 |
| **F3d** | road-edge 10-90 % transition width inside vanilla's | rung 6.00 texels vs vanilla 6.00 -- TILING2's addendum says the defect is CONTRAST (+29.45 over surround vs vanilla's +3.92), not width |
| **F3e** | the cross-road luminance profile has no step where vanilla's has none (the two-tone skirt band gone): max second difference of ours within vanilla's | rung = the two-tone profile this lane must first measure |
| **F3f** | `lodgen_roads.sh` stays 11/0; if its R5 bar moves because the road matches better, say so with the number, never lower a bar | R5 floor 0.1347, ROADS2 after 0.3431, reference 0.4038, bars 0.2694 / 0.3231 |
| **F3g** | ROADS2's S1 seam number not worse than 4.242 | 4.242 = vanilla's feathered-boundary gradient; ROADS2 shipped 11.837 on max-z+detail0 (see 3.1 below -- the brief's 4.242 is vanilla's number, not the rung's) |
| **F3h** | raised-highway clearance stays +0.001 | vanilla -0.009, rung-before +0.314 |
| **F4** | the chain at GRADE1's baselines; exe newer than every changed file; rung == launch bytes; no NifSkope left running | GRADE1's counts: lodgen_terrain 26/0, lodgen_terrain_vt 41/1, lodgen_roads 11/0, lodgen_ground_cover 29/5, lodl_open 23/0, lod_generation 116/0, lodgen_native 18/0, lodgen_terrain_pbrm 14/0 |

Rules this lane may not break (from the brief): no ground changes (TILING2 /
TILING3 / TILING4's), no tone-curve change (GRADE1's), no tiling-constant
change, never his installed `Data\Terrain`, never `git stash`, never commit.

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

### 1.5 Vanilla keeps NO road-texture detail -- and bungo has since ruled that this does NOT decide the default

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
to put back, so `--road-detail` is not re-fitted.

**What this measurement does NOT decide, added after the lane closed.** bungo
looked at both bakes on 2026-09-12 and ruled: *"--road-detail 1 is always on, do
not ever use road detail 0, that looks terrible"*. The table above is untouched
by that and is still the honest answer to "does vanilla keep the road texture's
detail" -- it does not. It is not an answer to "how should ours look", and this
lane was wrong to let it carry a default recommendation. `docs/LODGEN_TERRAIN_VT.md`
1a.5d paragraph 7 carries the overrule. ROADS3's own change never touched
`--road-detail`, so nothing measured in this report depends on which way it
goes.

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

## 2. The change

One switch, `--road-opacity A`, default **1.0**, and at 1.0 the bake is the rung
by construction.

| file | what changed |
|---|---|
| `src/lodgen.h` | `float roadOpacity = 1.0f;` on `LodgenCoverOptions`, beside `roadCoverSuppress`, with the measurement that decided the default written into the comment (the two rises, the three slopes, the three floors, and the two arithmetic refusals) |
| `src/lodgen.cpp` chunk writer (the composite at `:7944`) | the road plane's alpha is split into two named things: `raGeom`, the COVERAGE, and `ra`, the paint strength |
| `src/lodgen.cpp` tile writer (the composite at `:9261`) | the same split, same rule, pointing at the chunk path's comment |
| `src/lodgen.cpp` census (`:~10111`) | `roadOpacity %.3f` written beside `roadDetail`, whether the pass is on or off |
| `src/nifcli.cpp` | `--road-opacity` parsed and clamped to 0..1, `lgRoadOpacitySet` so `--roads-legacy` cannot silently override a value the caller named, and a usage-page entry with the numbers |

The composite now reads:

```cpp
const float raGeom = float( rp >> 24 ) / 255.0f;
const float ra = ( coverOpts.roadOpacity == 1.0f )
    ? raGeom : raGeom * coverOpts.roadOpacity;
if ( raGeom > 0.0f ) {
    ...
    if ( ra > 0.0f )
        for ( int k = 0; k < 3; k++ )
            color[k] = color[k] + ( rc[k] - color[k] ) * ra;
    if ( doCover ) {
        const float keep = qBound( 0.0f,
            1.0f - raGeom * coverOpts.roadCoverSuppress, 1.0f );
        ...
    }
}
```

Three things about it are deliberate and are the reason it can be checked rather
than argued about:

1. **At 1.0 the multiply is not done at all** and the outer branch is entered on
   exactly the same condition as before, so the off value is the previous bake's
   BYTES by construction and not a float argument about `1.0f`. This is
   GRADE1's `g_landGrade != 1.0f` pattern, followed on purpose.
2. **The ground-cover suppression keeps the UNSCALED coverage.** A faintly
   painted road is still a road and grass still must not grow through it, so
   `raGeom` and not `ra` goes into `keep`. At the default the two are the same
   value, so this changes nothing today; it is what makes `--road-opacity 0`
   mean "paint nothing" rather than "there is no road here".
3. **`--roads-legacy` restores 1.0** unless the caller named an opacity on the
   same command line. That is a no-op while the default is 1.0, and it is
   written anyway so the one-token way back stays the way back if the default
   is ever moved.

**What did NOT change, each with the measurement that refused it:**

| refused | the number that refused it |
|---|---|
| a **default** other than 1.0 | no single value meets the gates: on (-20,20) the level wants 0.83, the rise wants 0.326, the step wants 0.25 or less and the SD wants 0.75 or more; on (-8,8) nothing can reach vanilla at all (7.53 levels outside the interval the composite can produce). And by TILING2's precedent no lane changes the look of every future bake on its own |
| a **ground-relative** (per-texel) mode | simulated and beaten by a flat opacity on every row of both tiles: it lands the (-20,20) road 25 levels below vanilla with a rise of **-1.8** because most of its texels clamp to a = 0, and it halves the (-8,8) local SD to 0.48 of vanilla's |
| a **`--road-detail` refit** | the residual-vs-detail correlation IS its phase-twin floor at every blur on both tiles (+0.0275 against 0.0270/0.0644; +0.0162 against 0.0124/0.0163) and the best-fit strength is negative. There is no detail to put back |
| a **hue knob** | road minus surround on the opponent axes: vanilla +2.30/-2.14 and ours +3.35/-3.25 on (-20,20); vanilla +4.10/-3.07 and ours +1.96/-1.24 on (-8,8). Same sign, same direction, every gap under 3 levels |
| an **edge / feather knob** | vanilla's edge cannot be resolved: a 4.29-level rise under a 6.59-level local SD, signal-to-noise **0.65**. A width fitted there is a reading of the terrain's own texture |
| any change to the **ground**, the **grade**, or the **tiling** | out of this lane's scope by the brief, and TILING2's addendum says in writing not to chase the 25 levels with the road pass |

## 3. Build and gates

### 3.1 The build

The game came down while section 2 was being written, so the build was spent
after a fresh check, not an old one:

```
2026-09-12 03:57:20   tasklist | Fallout4.exe   22908   Console   1   8,305,996 K
2026-09-12 04:08:58   tasklist | Fallout4.exe   -- not running
```

`tools/ww_build.sh` then ran once and returned **BUILD-RC=0 on the first try**:
**one build, zero extra relinks.** The chain is the gated one -- it refuses
while the game is up, renames the old exe aside instead of killing anything,
runs `make -j2` and reads `make`'s own exit code, and then checks the exe is
newer than every source it depends on.

| | |
|---|---|
| exe before (the rung) | 2026-09-12 03:06:21, 21,489,152 B, md5 `6af74b4b4667ce50c4506a2d42a04fdf` |
| kept aside as | `release/NifSkope.before_roads3.exe` -- a byte copy, same md5, **equal to the exe this lane found at launch**. Its own file date reads 03:37:13, which is when the copy was taken, not when it was linked |
| exe after | **2026-09-12 04:10:38, 21,489,152 B**, md5 `fe65cc978f3896881140c2eea57c69c6` |
| builds spent | 1 |
| extra relinks | 0 |
| `--road-opacity` in the built usage page | yes, read back off the new exe |
| census line in the built `--vt` report | `roadOpacity 1.000`, seen in the VT harness log |

The two exes are the same SIZE and different bytes; the size is a coincidence of
the linker, and the md5s are what separate them.

**bungo's open NifSkope window is now out of date and needs a restart** to pick
this exe up. Nothing was killed and no window of his was touched.

### 3.2 F2 -- the off value IS the rung's bytes, and the compare can fail

`r3_f2.sh`, four arms a tile, every file `cmp`-ed (not a sample), logs in
`logs/f2_bytes.txt`:

| arm | chunk (-20,20) | chunk (-8,8) |
|---|---|---|
| new exe, **no new flag** vs the rung | **9 identical, 0 differ** | **10 identical, 0 differ** |
| `--road-opacity 1` vs the rung | **9 identical, 0 differ** | **10 identical, 0 differ** |
| `--roads-legacy` vs the rung's `--roads-legacy` | **9 identical, 0 differ** | **10 identical, 0 differ** |
| `--road-opacity 0.326` vs the rung (**the compare shown able to fail**) | 6 identical, **3 differ** | 6 identical, **4 differ** |

What moves at 0.326 and what does not: the colour sheet
`tex/Commonwealth.4.<x>.<y>.DDS` and the virtual-texture `.lodt` levels (2 and 4
on Sanctuary; 2, 4 and 8 downtown, because that tile has a deeper pyramid). What
never moves at any setting: the `_msn` normal sheet, the `_data` sheet, the
`.bto` object files, the `.lodl` and the `.lodm`. So the switch reaches the road
COLOUR and nothing else, and that is a `cmp` result rather than a claim about
the code.

### 3.3 F4 -- the harness chain, each count beside GRADE1's baseline

`r3_f4.sh`, all eight in order on the new exe, logs `logs/f4_*.txt`:

| harness | this lane | GRADE1's baseline | |
|---|---|---|---|
| `lodgen_roads` | **11 checks, 0 failures** | 11 / 0 | same |
| `lodgen_terrain` | **26 / 0** | 26 / 0 | same |
| `lodgen_terrain_vt` | **41 / 1** | 41 / 1 | same count, see below |
| `lodgen_ground_cover` | **29 / 5** | 29 / 5 | same (the five are the pre-existing C11b block) |
| `lodgen_terrain_pbrm` | **14 / 0** | 14 / 0 | same |
| `lodgen_native` | **0 failures in all seven sections** (69, 44, 87, 37, 17, 15, 18 checks) | 18 / 0 | same, and more of it than the baseline row records |
| `lodl_open` | **23 / 0** | 23 / 0 | same |
| `lod_generation` | **116 / 0** | 116 / 0 | same |

**The one red row, run down properly.** `lodgen_terrain_vt` holds its baseline
COUNT but the failing check is **V9c** ("the direct sheets are continuous across
a chunk seam"), where the baseline's red row was historically V9b. A moved
failure is not something to wave through, so the rung exe was made to answer for
it: `EXE=release/NifSkope.before_roads3.exe bash tests/spells/lodgen_terrain_vt.sh`,
log `logs/f4_vt_RUNG_control.txt`. The V9 block comes back **digit for digit
identical** on both exes:

```
E/W seam 188.074  interior 13.243  ratio 14.20  (edge step 14.348)
N/S seam  35.857  interior 12.182  ratio  2.94  (edge step 11.905)
FAIL V9c  ... on the rung exe AND on this lane's exe, same numbers
```

So V9c's failure predates this lane -- it belongs to the rung, and the move from
V9b to V9c happened before ROADS3 started. This lane moved nothing in that
harness. (The control run reports one EXTRA failure, `the exe is newer than
every source`, which is the preflight noticing that a copied-aside older exe is
being run against a patched tree. That is the control's own artifact, not a
terrain result.)

`lodgen_roads` R5, the number the brief asked to be watched: floor 0.1354, after
0.3435, reference 0.4039, bars 0.2708 and 0.3231, both cleared -- and the
centreline colour error against vanilla falls from 38.04 with `--no-roads` to
22.34 with `--roads`. **No bar was touched.**

No NifSkope process was left running: the count after the chain was 0.

### 3.4 F3 -- the gate table on REAL bakes

Every F3 row below is read off sheets a built exe wrote, not off a simulation.
`r3_f3.py`, log `logs/f3_gates.txt`; the seam row is ROADS2's own `seam.py`
re-run on this lane's bakes, log `logs/f3g_seam.txt`.

The measured table, chunk (-20,20) then (-8,8):

| field | road L | rise over surround | local 5x5 SD | biggest step | b_y | r_g |
|---|---|---|---|---|---|---|
| **vanilla (-20,20)** | 92.52 | +4.29 | 6.59 | 1.31 at d=8 | -12.63 | +5.28 |
| the rung = `--road-opacity 1` | 99.05 | +29.96 | 7.26 | 3.88 at d=3 | -12.47 | +8.09 |
| baked `--road-opacity 0.326` | 73.09 | **+4.01** | 4.20 | 1.69 at d=3 | -14.18 | +10.41 |
| baked `--road-opacity 0.83` | 92.43 | +23.34 | 6.72 | 3.35 at d=3 | -- | -- |
| **vanilla (-8,8)** | 94.59 | +4.40 | 6.44 | 4.43 at d=9 | -6.28 | +0.48 |
| the rung = `--road-opacity 1` | 106.68 | +3.84 | 5.42 | 1.25 at d=8 | -12.95 | +5.54 |
| baked `--road-opacity 0.326` | 103.39 | +0.55 | 4.29 | 1.04 at d=2 | -12.90 | +4.87 |
| baked `--road-opacity 0.83` | 105.89 | +3.05 | 5.10 | 1.26 at d=2 | -- | -- |

| gate | state on the built exe | the number, beside its floor |
|---|---|---|
| **F1** the opacity law fitted with a floor and a ceiling BEFORE any code | **MET, and the brief's law is REFUSED by its own floor** | unexplained 17.6 % against a shuffled-ground floor of 18.2 % on (-20,20); 52.6 % against 51.5 % on (-8,8). Ceiling (vanilla against a neighbouring shipped sheet) 18.1 % / 39.2 %. Alignment control: the best shift over +-3 buys 0.005 |
| F1 detail strength | **MET -- null result, and OVERRULED as a default** | correlation +0.0275 against a phase-twin floor of 0.0270 mean / 0.0644 max; +0.0162 against 0.0124 / 0.0163; best-fit strength negative. The measurement stands; bungo looked at both bakes on 2026-09-12 and ruled *"--road-detail 1 is always on, do not ever use road detail 0, that looks terrible"*, so it is not what picks the default. See the note below section 1.5 |
| F1 hue named by a measurement | **MET** | on the road texels themselves at (-20,20): vanilla b_y -12.63 against ours -12.47, saturation 0.162 against 0.161. Not a defect and not a knob |
| **F2** off value == the rung's bytes, `cmp` every file, compare shown able to fail | **MET ON BOTH TILES** | 9/9, 9/9, 9/9 and 10/10, 10/10, 10/10 identical; 0.326 moves 3 and 4 files, all of them colour. See 3.2 |
| **F3a** road mean luminance within 3 of vanilla | **the DEFAULT is 6.53 and 12.09 out; a = 0.83 lands (-20,20) to 0.09; (-8,8) is REFUSED by arithmetic** | baked 0.83 on (-20,20): 92.43 against vanilla 92.52, gap **-0.09**. On (-8,8) the composite can only land between our ground 102.12 and our paint 106.68, and vanilla is at 94.59 -- **7.53 levels outside the reachable interval at every opacity** |
| **F3b** road hue within 3 of vanilla | **MET AS A RISE on both tiles; unreachable as an absolute on (-8,8)** | rises 1.05 / 1.11 levels apart on (-20,20), 2.14 / 1.83 on (-8,8). As an absolute, d(b_y) is -6.5 to -6.7 for EVERY setting including the rung, because our ground's own b_y downtown is -12.79 against vanilla road's -6.25 |
| **F3c** road local SD within 20 % of vanilla's | **the default MEETS it on (-20,20) and every calming setting breaks it** | rung 7.26 / 6.59 = 1.10 (inside); baked 0.83 = 6.72 / 6.59 = **1.02** (inside); baked 0.326 = 4.20 / 6.59 = **0.64** (outside). On (-8,8) the rung is 5.42 / 6.44 = 0.84, already outside, and no setting fixes that |
| **F3d** road-edge 10-90 % width inside vanilla's | **REFUSED AS UNRESOLVABLE, with the number** | vanilla's 4.29-level rise sits under a 6.59-level local SD: SNR **0.65**. TILING2's addendum agrees from the other side -- both widths read 6.00 texels |
| **F3e** no step where vanilla has none | **MET at 0.326 on (-20,20); the rung is ALREADY smoother than vanilla on (-8,8)** | (-20,20): vanilla 1.31, rung 3.88, baked 0.326 **1.69**, baked 0.83 3.35. (-8,8): vanilla 4.43, rung 1.25, baked 0.326 1.04 |
| **F3e'** the two-tone skirt itself, measured | **the knob SHRINKS it and cannot remove it** | correlation of road luminance with the mesh's own vertex alpha: vanilla **+0.001**, ours at a=1 **-0.792**, a=0.83 -0.694, a=0.326 **-0.436** -- and our unpainted ground's own floor is **-0.325**, so opacity walks ours from -0.79 toward -0.33 and never to vanilla's 0. The skirt geometry is still there underneath |
| **F3f** `lodgen_roads.sh` stays 11/0 | **MET** | 11 / 0. R5 floor 0.1354, after 0.3435, reference 0.4039, bars 0.2708 / 0.3231 -- no bar touched |
| **F3g** ROADS2's S1 seam not worse than 4.242 | **MET at every setting** | feathered-boundary luminance gradient, 54 texels, vanilla 4.242: rung **3.979**, baked 0.83 **3.352**, baked 0.326 **2.979**. Solid-boundary control (2,886 texels) moves with it: 5.772 / 5.567 / 3.351 against vanilla 5.291. Displaced-boundary floors 3.10-4.81 |
| **F3h** raised-highway clearance stays +0.001 | **MET by bytes** | `--road-raised` is untouched, and F2 shows the `.bto`, `.lodl` and `.lodm` files byte-identical at **every** setting including 0.326, so no geometry number can have moved. That is a `cmp` result, not an assurance |
| **F4** the chain at GRADE1's baselines, exe newer than every changed file, rung == launch bytes, no NifSkope left running | **MET** | eight harnesses, every count equal to the baseline; the one red row is the rung's own, proved by the control in 3.3; exe 04:10:38 newer than all three touched sources; rung md5 == launch md5; 0 NifSkope processes after |

**How the pre-build simulation held up.** Section 1.7 priced every candidate
offline before a line of C++ was written. Against the real bakes it is right on
the AGGREGATES and wrong per texel, and both halves of that deserve saying:
road mean luminance agrees to **0.29** of a level on (-20,20) and **0.22** on
(-8,8), and the rise to the same, but a single road texel differs by **1.58**
levels on average, 7.34 at the 99th percentile and **15.28** at worst (1.53 /
5.75 / 13.80 downtown). The bake goes through 8-bit quantisation and BC1 block
compression and the simulation does not. So the simulation was the right tool
for choosing WHICH candidates to bake and is not a substitute for baking them.

**What the table adds up to, plainly.** The default did not move and its bytes
prove it. On Sanctuary the knob does exactly what it says: at 0.83 the road
lands on vanilla's brightness to a tenth of a level, at 0.326 it lands on
vanilla's rise to three tenths -- and **no setting does both, because they are 22
levels apart and those 22 levels are the ground's, not the road's.** Downtown
the knob can only make the road darker than a vanilla road that is already
darker than our ground, so it cannot help there at all. That is why nothing is
being proposed as a new default.

## 4. Pictures

Both are in `scratchpad/roads3_20260911/images/`, made by `r3_pics.py`. **Every
panel in both is a real bake or Bethesda's own shipped sheet. Nothing in the
final pictures is simulated**; the script keeps the simulation only as a
labelled fallback for a variant that is not on disk, and no panel took that
path.

**`cmp_road_wash.png`** (1960 x 1174) -- two rows, chunk (-20,20) on top and
(-8,8) beneath, four columns each: vanilla | the rung, which IS
`--road-opacity 1` | baked `--road-opacity 0.326` | baked `--road-opacity 0.83`.
Every panel is the same crop of the same 512-texel grid at 32 world units a
texel, nothing resampled on any side, and each carries its own road mean
luminance, its gap to vanilla, its rise over the surround, its local 5x5 SD and
its biggest step. The crops were fixed before the candidates were compared:
(-20,20) keeps lane ROADS1's own (150,120)-(300,270), and (-8,8) is picked by a
rule that looks only at the road mask -- the 96x96 window holding the most road
texels -- which came out at (112,116)-(208,212) with 3,176 of them.

What it shows in plain words: at 0.326 the Sanctuary road stops being a stripe
laid on the ground and starts being ground with a road on it, and goes 19.43
levels darker than vanilla's while doing it. At 0.83 it sits on vanilla's
brightness (-0.09) and is still a stripe (+23.34 against vanilla's +4.29). The
downtown row barely moves at any setting, because there our paint and our ground
are already only 4 levels apart.

**`cmp_road_profile.png`** (1344 x 544) -- the cross-road luminance profile,
mean luminance against signed distance to the mask edge, one plot a tile, with
vanilla, the rung, our own `--no-roads` ground and the two baked candidates on
one axis, and the per-texel opacity the wash would need drawn against its own
scale on the right, which is what the brief asked for beside the luminance.
Vanilla reaches its full value one texel in and runs flat; ours ramps over three
texels, plateaus near 96.5 and climbs again to 105.5 in the core. **That darker
outer band around a brighter core is the two-tone bungo is seeing, and it is the
opposite way round from the guess in the brief** -- the brief expected a bright
halo around a darker core.

## 5. Owed / red / bungo's calls

**BUNGO'S CALL 1 -- the default.** The switch ships at 1.0, which means this
lane changes nothing he can see until he says so. The priced table, both tiles.
Three rows were then BAKED on the built exe and are marked so; the other two are
the offline pricing, which section 3.4 shows is good to about a quarter of a
level on these aggregates:

| `--road-opacity` | (-20,20) road L (vanilla 92.52) | (-20,20) rise (vanilla +4.29) | (-8,8) road L (vanilla 94.59) | (-8,8) rise (vanilla +4.40) |
|---|---|---|---|---|
| **1.000 (shipped)** -- BAKED | 99.05 (+6.53) | +29.96 | 106.68 (+12.09) | +3.84 |
| 0.830 -- BAKED | 92.43 (**-0.09**) | +23.34 | 105.89 (+11.30) | +3.05 |
| 0.500 -- priced | 80.01 (-12.52) | +10.91 | 104.40 (+9.81) | +1.56 |
| 0.326 -- BAKED | 73.09 (-19.43) | **+4.01** | 103.39 (+8.79) | +0.55 |
| 0.250 -- priced | 70.48 (-22.04) | +1.39 | 103.26 (+8.67) | +0.42 |

My reading, said plainly and not hedged: **if he wants the Sanctuary road to
stop reading as a painted stripe, 0.326 is the value that does it, and it costs
19 levels of darkness on that road because our GROUND there is 19 levels dark.**
Turning the road down is borrowing against a ground error that lane TILING2
measured and told this lane not to touch. The honest order is ground first, road
second; with the ground where vanilla's is, `--road-opacity` would not be
needed at all on that tile. That is his call, not mine.

**RED 1 -- our ground on (-20,20) is 19 levels darker than vanilla's** (68.69
against 83.52, ROADS2's number; this lane's surround band reads 69.10 against
88.24). It is the whole reason the road reads wrong there, it is not the road
pass's, and GRADE1 already measured that it is a per-cell CONTENT difference
with a near-zero mean -- not exposure, not gamma, not a colour space, not a tone
curve. Carried forward unchanged.

**RED 2 -- the object-path material fix-up** at `src/lodgen.cpp:6811` still
resolves `materials/c:/projects/fallout4/...` style paths by keying on the LAST
`materials/` in the string. This lane confirmed those lines in the bake log come
from `lodgenLoadModel`'s object path and NOT from the road pass: the road census
reads `roadRefusedNoTexture 7` out of 319 shape tiles with `roadTexels 27509`.
GRADE1's red 3 can be narrowed to the object path; it is still live there.

**RED 3 -- the ground-cover plane is empty on these chunks** (`dwReserved1 = 0`,
GRADE1's red 1), which is why the grass tint is inert and why this lane could
read the road plane straight off the sheet. Unchanged, and still worth someone's
lane.

**SETTLED -- the build.** It was spent: once, first try, zero extra relinks, and
F2, F3a-F3h and F4 are all measured on the built exe in section 3.
`scratchpad/roads3_20260911/PENDING.md` is superseded and says so at its top.

**SETTLED -- the pictures** are re-taken from real bakes; no panel in either
picture is simulated.

**OWED -- bungo restarts his NifSkope window.** The deployed exe changed at
2026-09-12 04:10:38; his open window is still on the 03:06:21 one. Nothing of
his was touched and nothing was killed.

**OWED -- nobody has looked at this in the game.** Every number here is off a
baked sheet on disk. The default's bytes did not move, so there is nothing new
to see at the default; the two candidate settings have not been flown.

**RED 4 (new) -- V9c in `lodgen_terrain_vt` is red and is NOT this lane's.** The
rung exe fails it with digit-for-digit identical numbers (section 3.3). It is a
real red row in the tree and it wants a lane of its own: the E/W chunk-seam step
reads 14.20x the interior step against a bar of 3.20, and the interior control
itself (13.243 / 12.182) is outside its own 1.20..2.20 window, which says the
sheets being measured are not the terrain those bars were set on.

**NOT OWED, and said so on purpose:** vanilla's edge width, vanilla's road hue
as a separate defect, and a `--road-detail` refit. All three were measured and
all three came back as "there is nothing here" with a floor beside the number.

## 6. Mistakes

All five are in `scratchpad/roads3_20260911/MISTAKES_ENTRIES.md` in the
ledger's format (`--` headings, newest-at-top ordering left to the splicer),
ready to splice into `MISTAKES.md`. The fifth was added after the lane closed,
when bungo's ruling on `--road-detail` showed the lane had let a measurement
carry a recommendation about how something should look.

1. **A ledgered mistake repeated in the very next lane.** Lane ROADS2's
   MISTAKES entry 4 says a heredoc carrying prose apostrophes dies with
   `unexpected EOF while looking for matching '''` and that every patch in that
   lane became a file written with the Write tool. This lane hit the same trap
   appending report section 1 -- the section was composed, the append failed,
   and nothing was on disk until it was rewritten as a file. Reading the ledger
   is not the same as obeying it.
2. **A floor that could not fire.** In `r3_fit.py` the detail-correlation floor
   translated a mask-shaped signal, which is all zeros on the mask, so
   `np.corrcoef` returned NaN on all five draws. A floor that returns NaN is
   not a weak floor, it is no floor, and it sat there looking like one.
   `r3_chroma.py` replaced it with `splatlib.phase_twin` (amplitude kept, phase
   broken), which fires and produced the numbers in section 1.5. CONSTITUTION
   rule 4 says a control must be shown able to fail; NaN is how that goes wrong
   quietly.
3. **An accuracy figure written into a contract document before it was
   measured.** The report and `docs/LODGEN_TERRAIN_VT.md` both said the offline
   simulation was "right to about half a level". When the same setting was baked
   and compared texel by texel, the aggregate agreement was 0.286 and 0.221 of a
   level but the per-texel difference was 1.583 on average and **15.279** at
   worst. The guess was 3x optimistic on the average and 30x on the tail; it was
   conservative on the one number the gate table was read on, which is luck. A
   tolerance is a measurement or it is not written down. Both documents now
   carry the measured figures.
4. **410 CRLF lines smuggled into an LF-only report by the shell.** Parts of
   this report were appended with heredocs and every one of those lines arrived
   carriage-return terminated, in a directory where all sibling lane reports
   are LF-only. Nothing
   visible was wrong; a byte count found it, and only because a `str.replace`
   anchor copied out of a `sed -n` view failed to match. The repo already has
   the rule (byte counts only, heredocs arrive CRLF) and it is the same
   mechanism as mistake 1: prose through the shell. Every file this lane leaves
   behind was byte-counted at the end and reads 0 CRLF. The entry you are
   reading was itself written with a literal CR in it, by the same shell, and
   the end-of-lane byte count is what caught it.
5. **A measurement was allowed to carry a recommendation about how something
   should LOOK.** The lane measured that vanilla's far road keeps none of the
   road texture's detail -- sound, floored, and a test that could have
   overturned it did not -- and then wrote in four documents that
   `--road-detail` therefore "stays 0" and that "ROADS2's default is
   confirmed". The correlation answered what Bethesda's sheets contain; it was
   quoted as an answer to what our roads should look like. bungo looked at both
   bakes on 2026-09-12 and ruled `--road-detail 1` always on. The tell is the
   verb: "stays", "is confirmed", "should be" in a sentence whose only evidence
   is a correlation. The same report got this right for `--road-opacity` one
   section later, where the table is explicitly labelled his call.

## 7. Finished-work skill review

Skills read before the work: `ww-control-calibration` (floors, known answers,
ceilings), `ww-contract-provenance` (numbers carry where they came from),
`nifskope-ww-resume-pending` (how a BUILD PENDING lane hands over),
`nifskope-ww-commit` (not used -- nothing is committed).

**Added: `.claude/skills/ww-simulate-before-build/SKILL.md`.** The one
genuinely new procedure this lane ran, and the one that saved the build: before
spending a build on a new knob, apply the knob's arithmetic offline to sheets
that are already baked, and read the WHOLE gate table off the simulation. It
caught two things no amount of coding would have: that on (-8,8) no opacity can
satisfy F3a at all, and that the gates on (-20,20) are mutually exclusive by 22
levels. The skill states when the trick is legitimate (the pass is a closed-form
composite, the stages after it are inert or branched over, and both inputs are
on disk), what it cannot show (quantisation, compression, anything that changes
the road plane's own coverage), and that its output must be labelled SIMULATED
wherever it reaches a picture. It carries this lane's own audit of it: against
the real bakes the simulation agreed on the AGGREGATES to 0.29 and 0.22 of a
level and differed PER TEXEL by 1.58 levels on average and 15.28 at worst, so
the skill says in writing that a simulation licenses a choice of what to bake,
and never a picture and never a gate row.

**Amended: `.claude/skills/ww-control-calibration/SKILL.md`** with one
paragraph: a floor must be checked for NaN and for zero variance before it is
believed, because a translated mask-shaped signal is all zeros on the mask and
`corrcoef` then returns NaN, which reads like a passing floor in a table.

Both are mirrored into `E:\Projects\NifskopeWWE_ui\.claude\skills\` so the
parallel tree has them.

## DONE

Lane ROADS3 closed 2026-09-12 04:30 (`date` read in the same turn, not
estimated). `scratchpad/roads3_20260911/DONE` written, `BUILDING` removed.

**The exe.**

| | |
|---|---|
| `release/NifSkope.exe` | **2026-09-12 04:10:38**, **21,489,152 B**, md5 `fe65cc978f3896881140c2eea57c69c6` |
| the rung it replaced, as found at launch | 2026-09-12 03:06:21, 21,489,152 B, md5 `6af74b4b4667ce50c4506a2d42a04fdf` |
| kept aside as | `release/NifSkope.before_roads3.exe` -- same md5 as the launch exe; its own file date is 03:37:13, which is when the copy was taken, not when it was linked |
| builds spent | **1** (BUILD-RC=0 first try) |
| extra relinks | **0** |
| sources changed | `src/lodgen.h` 03:57:07, `src/lodgen.cpp` 03:57:07, `src/nifcli.cpp` 03:58:08 -- all older than the exe |
| `Fallout4.exe` at close | not running |
| NifSkope processes at close | **0** |

The two exes are the same SIZE and different bytes; the md5s are what separate
them.

**The gate table.**

| gate | verdict | the number, beside its floor |
|---|---|---|
| **F1** vanilla's opacity law fitted with a floor and a ceiling BEFORE any code | **MET -- and the law is REFUSED by its own floor** | unexplained 17.6 % vs shuffled-ground floor 18.2 % on (-20,20); 52.6 % vs 51.5 % on (-8,8). Ceiling 18.1 % / 39.2 %. Alignment control: best shift over +-3 buys 0.005 |
| **F1** detail strength | **MET -- null result, OVERRULED as a default** | correlation +0.0275 vs phase-twin floor 0.0270 / 0.0644; +0.0162 vs 0.0124 / 0.0163. The measurement stands; bungo ruled `--road-detail 1` always on, 2026-09-12, after seeing both |
| **F1** hue named by a measurement | **MET -- null result** | road texels at (-20,20): vanilla b_y -12.63 vs ours -12.47; saturation 0.162 vs 0.161 |
| **F2** off value == the rung's bytes, every file `cmp`-ed, compare shown able to fail | **MET on both tiles** | no flag 9/9 and 10/10 identical; `--road-opacity 1` 9/9 and 10/10; `--roads-legacy` 9/9 and 10/10. Able to fail: 0.326 moves 3 and 4 files, **all colour**; `_msn`, `_data`, `.bto`, `.lodl`, `.lodm` identical at every setting |
| **F3a** road mean luminance within 3 of vanilla | **default is 6.53 and 12.09 out; a=0.83 lands (-20,20) to 0.09; (-8,8) REFUSED by arithmetic** | baked 0.83: 92.43 vs vanilla 92.52. On (-8,8) the composite can only land between ground 102.12 and paint 106.68 and vanilla is 94.59 -- **7.53 outside at every opacity** |
| **F3b** road hue within 3 of vanilla | **MET as a rise; unreachable as an absolute** | rises 1.05 / 1.11 apart on (-20,20), 2.14 / 1.83 on (-8,8). Absolute d(b_y) -6.5 to -6.7 for every setting including the rung |
| **F3c** road local SD within 20 % of vanilla's | **default MET on (-20,20); every calming setting breaks it** | rung 1.10x, baked 0.83 **1.02x**, baked 0.326 **0.64x**. On (-8,8) the rung is already 0.84x and no setting fixes it |
| **F3d** road-edge 10-90 % width inside vanilla's | **REFUSED AS UNRESOLVABLE, with the number** | 4.29-level rise under a 6.59-level local SD: SNR **0.65** |
| **F3e** no step where vanilla has none | **MET at 0.326 on (-20,20); the rung is already smoother on (-8,8)** | (-20,20) vanilla 1.31, rung 3.88, 0.326 **1.69**, 0.83 3.35. (-8,8) vanilla 4.43, rung 1.25 |
| **F3e'** the two-tone skirt itself | **the knob shrinks it and cannot remove it** | luminance vs mesh vertex alpha: vanilla +0.001, a=1 **-0.792**, 0.83 -0.694, 0.326 **-0.436**, our own ground's floor **-0.325** |
| **F3f** `lodgen_roads.sh` stays 11/0 | **MET, no bar touched** | 11/0. R5 floor 0.1354, after 0.3435, reference 0.4039, bars 0.2708 / 0.3231 cleared; centreline error 38.04 -> 22.34 |
| **F3g** ROADS2's S1 seam not worse than 4.242 | **MET at every setting** | feathered boundaries (54 texels, vanilla 4.242): rung **3.979**, 0.83 **3.352**, 0.326 **2.979**. Solid-boundary control 5.772 / 5.567 / 3.351 vs vanilla 5.291. Displaced floors 3.10-4.81 |
| **F3h** raised-highway clearance stays +0.001 | **MET by bytes** | the `.bto`, `.lodl` and `.lodm` are byte-identical at every setting (F2), so no geometry number can have moved |
| **F4** the chain at GRADE1's baselines; exe newer than every changed file; rung == launch bytes; no NifSkope left running | **MET** | `lodgen_roads` 11/0, `lodgen_terrain` 26/0, `lodgen_terrain_vt` 41/1, `lodgen_ground_cover` 29/5, `lodgen_terrain_pbrm` 14/0, `lodgen_native` 0 failures in all seven sections, `lodl_open` 23/0, `lod_generation` 116/0 -- every one equal to the baseline. Exe 04:10:38 newer than all three sources. Rung md5 == launch md5. 0 NifSkope processes |

**The one red row in that table is not this lane's, and there is a control that
says so.** `lodgen_terrain_vt`'s failing check is V9c; the rung exe run through
the same harness fails V9c with digit-for-digit identical numbers (E/W seam
188.074, interior 13.243, ratio 14.20, edge step 14.348),
`logs/f4_vt_RUNG_control.txt`. It is carried forward as RED 4.

**Deliverables on disk.**

* `scratchpad/lane_roads3_report.md` -- this report, sections 0-7 and `## DONE`,
  LF-only.
* `scratchpad/roads3_20260911/WW_CHANGES_ENTRY.md` -- the changelog entry,
  starting `## 2026-09-12 —`.
* `scratchpad/roads3_20260911/HANDOFF_BLOCK.md` -- the handoff block.
* `scratchpad/roads3_20260911/MISTAKES_ENTRIES.md` -- **four** entries in the
  ledger's `--` heading format, not appended by the lane.
* `scratchpad/roads3_20260911/images/cmp_road_wash.png` and
  `cmp_road_profile.png` -- every panel a real bake or Bethesda's own sheet.
* `docs/LODGEN_TERRAIN_VT.md` -- amended in place, section 1a: new **1a.5d**
  (the whole finding with its provenance, its baked table, and what the built
  exe measured), plus 1a.4, 1a.7 and 1a.8. LF-only, byte-counted.
* `.claude/skills/ww-simulate-before-build/SKILL.md` -- **added**, and it now
  carries this lane's own audit of the technique's accuracy.
  `.claude/skills/ww-control-calibration/SKILL.md` -- **amended** with the
  NaN-floor paragraph. Both mirrored additively into
  `E:\Projects\NifskopeWWE_ui\.claude\skills\`; nothing in that tree was deleted
  or overwritten, because lane UINOTES1 is live in it.
* `scratchpad/roads3_20260911/PENDING.md` -- superseded, and says so at its top.

**`scratchpad/lodui1_20260911/BAKE_INSTRUCTION.md` needs no change, and here is
why in one line:** no default moved. `--road-opacity` ships at 1.0, the multiply
is branched over there, and F2 shows every file of a default bake byte-identical
to the previous exe's -- so the instruction as written still produces exactly
the bytes it says it produces.

**What is not claimed.** Nothing here has been looked at in the game. The
default's bytes did not move, so there is nothing new to see at the default; the
two candidate settings have not been flown. bungo's open NifSkope window is
still on the 03:06:21 exe and needs a restart to pick this one up. And no
default is being proposed: the table in section 5 is for his call.
