# Lane TILING3 -- vanilla's fine detail: where it comes from, and whether we can bake it

Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, nothing committed.
Exe at launch `release/NifSkope.exe` 2026-09-11 21:52:22, 21,466,624 B (TILING2's
DONE exe). Rung taken once at 22:2x: `release/NifSkope.before_tiling3.exe`.

bungo's words this lane serves (2026-09-11 22:1x, over `cmp_tiling2.png`'s green
panel "OURS --land-sample average --blend-edges quadrant"):

> "Is green "ours" the final one? because it lost all the texture to it, now it's
> only solid color blobs"
> ... "Now, the diffuse of vanilla lod land textures, it looks like there's
> variety to it, some geological features shown"

and 19:2x, on the rung: "you can see the tiling pattern of each texture, which is
not good ... it's muddy or blurry looking".

Neither panel ships. The sheet must have the land textures' grain WITHOUT their
repeat.

## 0. Pre-registered gates

Registered 22:26, before any number of this lane was read and before any code.

| gate | what must be true | floor (the reading that means "nothing") | ceiling |
|---|---|---|---|
| **F1** | hypotheses D, then A/B/C, each tested with a floor beside every number and a known-answer control run FIRST; the winner named by the number, or the round refused with the table | per test, below | vanilla-vs-vanilla where one exists |
| **F2** | every new switch at its off value == the rung's bytes, every file of both tiles; `.lodl` and `_msn` byte-identical at EVERY setting; 1 vs 16 chunk threads byte-identical on Sanctuary | a byte compare proved able to fail (a knowingly different bake) | -- |
| **F3** | **both at once, on the SAME bake**: tiling visibility inside vanilla's law (<= 0.264 absolute, <= 0.448 over the sheet's own null floor -- TILING2 1a) AND local variance + the band table within 20 % of vanilla's, on both tiles. A green on one bought by losing the other is a **RED**. | TILING2's rung (vis 1.037/1.261, locVar 12.29/24.41) and its `average` bake (vis 0.092, locVar 4.84) are the two failing states this gate must separate | vanilla vs vanilla spectrum distance 0.670 |
| **F4** | chain at TILING2's baselines (lodl_open 23/0, terrain 26/0, terrain_vt 41/1, roads 11/0, ground_cover 29/5, pbrm 14/0, native 18/0, panel_run 125/0, lod_generation 116/0, ui_align 11/0, water_ui 82/0); exe newer than every changed file; rung == launch bytes; no NifSkope left running | -- | -- |

### The floors and known-answer controls registered for F1, per hypothesis

**D -- geology from a finer heightfield, tested through VANILLA'S OWN `_msn`.**
Three instruments, because a shading term need not be linear:

* **D1, the best possible linear shading.** Least-squares regression of vanilla's
  colour high-pass residual onto a basis built from vanilla's own shipped `_msn`
  high-pass: `nx, ny, nz` (which span EVERY un-clipped Lambert `dot(n,l)` at every
  azimuth and elevation, so this subsumes the brief's "best of 8-16 azimuths"),
  plus the nonlinear terms `sqrt(nx^2+ny^2)` (slope magnitude), `nz^2`, and the
  divergence of `(nx,ny)` (a curvature proxy). Statistic: R^2.
  * floor 1: the same regression with every predictor replaced by its phase twin
    (same power spectra, alignment destroyed) -- the overfit floor;
  * floor 2: OUR `_msn` for the same chunk under the same regression (TILING2
    found our 128-u grid reads zero; it must stay there);
  * known answer: a synthetic "colour residual" = `a * shading(vanilla _msn) +
    Gaussian noise` at a known variance share must read R^2 within 10 % of `a`'s
    share before any verdict is read off the instrument.
* **D2, the envelope.** `corr(blur(|colour hp|), blur(|_msn hp|))` -- "where the
  ground is rough, the colour is detailed". This survives a sign flip and any
  monotone recolouring, so it catches geology that D1's linear form cannot.
  Floor: the same against the `_msn`'s phase twin, and against our own `_msn`.
* **D3, the direction.** Structure-tensor orientation of the colour residual vs
  that of the `_msn` residual, over 8x8 windows, weighted by coherence; the
  statistic is `mean(cos 2*dtheta)`. Floor: 0 is chance (uniform), plus the twin
  and our own `_msn`. Known answer: the colour residual against ITSELF must read
  1.000, and against a 90-degree rotation of itself -1.000.
* **D passes** only if at least one of D1/D2/D3 stands clear of ALL of its floors
  on BOTH tiles.

**A -- stochastic tiling.** (1) radial amplitude spectrum of vanilla's residual
against the land-texture composite's spectrum with phase randomised per texel/tile,
and against white noise of the same SD (the floor: a spectrally white residual is
noise, not texture); (2) skew and kurtosis of vanilla's residual against the
phase-randomised composite's and against Gaussian (skew 0, kurtosis 3).
A passes if the spectrum matches the composite and NOT white, and the moments match
the composite and NOT Gaussian.

**B -- a noise texture.** `Textures/Terrain/Noise.dds` as a fixed-phase candidate
over a scale search (tiled at 1/2/4/8/16/32 cells and at 341.3333 u), through
TILING2's `t4_corr.py` machinery; floor = each candidate's own phase twin, exactly
as TILING2 used it. Plus its own spectrum against vanilla's residual.

**C -- a coarser bake resampled.** The composite baked at 4x resolution then
box/Lanczos-downsampled; its residual spectrum against vanilla's, and its
periodicity at 10.67 texels against vanilla's law (0.264 / 0.448).

**If none of A/B/C/D fits both the spectrum and the moments on both tiles and on
5 of TILING2's 22 sheets, the lane REFUSES with the table and ships nothing new**
-- and then also says what "as close as we can get" costs, marked as not vanilla's
law.

## 1. Hypothesis D, tested first, through vanilla's own `_msn`

The brief told this lane to test D first and it did. The answer is split, and the
split is the most useful thing this lane found.

**D is the origin.** All three instruments clear all of their floors on both tiles:

| instrument | (-20,24) | (-20,20) | phase-twin floor | our own `_msn` |
|---|---|---|---|---|
| D1 best linear shading, R^2 | 0.01771 | 0.01889 | 0.00003 | 0.00002 |
| D2 envelope agreement | +0.1533 | +0.1268 | +0.0038 | -0.0407 |
| D3 orientation agreement | +0.3188 | +0.2681 | +0.0403 | +0.0433 |

and D2/D3 hold on five more of TILING2's 22 shipped sheets (D3 +0.106..+0.383
against twins +0.016..+0.063). Vanilla's colour grain **lies along** vanilla's own
fine relief. It is geology, not noise, and it is geology our 128-unit height grid
cannot see -- our `_msn` reads the floor under the identical test. The known-answer
controls ran first and passed: D1 recovers an injected variance share, D3 reads
+1.000 against the residual itself and -1.000 against its 90-degree rotation.

**D is not a computable source.** The richest basis this lane could justify -- 22
columns: the three normal components, slope, `nz^2`, the divergence, and the `_msn`
high-pass at eight blur radii -- puts a **ceiling** of R^2 = 0.018 / 0.023 on any
per-texel law from the `_msn` to the colour. Written as amplitude: of a residual SD
of 4.476 / 5.459, the explained part is SD 0.6 / 0.8 and the remainder is SD 4.4 /
5.4, with the remainder's moments (skew +1.078, kurtosis 6.403) indistinguishable
from the whole. **About 98 % of vanilla's fine colour is not a function of
vanilla's fine normal.**

So D names the SOURCE (a finer surface Bethesda had and we do not) and refuses to
be a GENERATOR. That is exactly why taking Bethesda's own shipped data is the only
route to Bethesda's geology -- which is the route bungo then ruled for on his own
(section 5).

## 2. A, B and C on what remains

* **B is refuted.** `Textures/Terrain/Noise.dds` correlates with vanilla's residual
  at |r| <= 0.0036 at every scale from half a cell to 32 cells and at the landscape
  repeat -- at its own twin floor -- and its skew is **-2.191** against vanilla's
  **+1.123**: the wrong sign, so no scaling or inversion of it can be the term.
* **C is refuted on its own discriminator.** A 4x bake box-downsampled still reads a
  repeat of **1.562** against vanilla's ceiling of 0.264. Downsampling does not
  remove a periodic term; it aliases it.
* **A is the one left standing, and it stands on evidence rather than on
  elimination.** Vanilla's residual is **not Gaussian** (skew +1.007 / +1.123,
  kurtosis 7.373 / 6.539 against 0 and 3) -- so it is not noise -- and its spectral
  shape sits closest to the land-texture composite sampled about four mip levels
  finer than the footprint (shape distance 0.187 against a white-noise floor of
  0.244). Texture-shaped spectrum, texture-shaped moments, zero fixed-phase
  correlation: that is hypothesis A's signature.

## 3. A's shippable form, and the number it was picked on

A smooth deterministic **domain warp** of world position before the texture lookup:
uint32-hash value noise on a world-space lattice, smoothstep interpolated, a pure
function of world position -- hence seamless across chunk and cell boundaries by
construction, and byte-identical at 1 vs 16 threads by construction -- plus a **mip
bias** to put back the grain the warp's own smoothing takes out.

`a6_pick.py` selected on seven sheets, with a per-sheet floor (the `average` bake,
provably repeat-free because one repeat IS the whole texture at its 1x1 mip) and
the ceilings TILING2 froze on 22 shipped sheets (absolute 0.264, ratio 0.448):

| warp | RMS strain | worst repeat | worst ratio | sheets gated |
|---|---|---|---|---|
| none (the rung) | 0.000 | 1.968 | 8.831 | 0 of 7 |
| A=341 L=2048 o1 | 0.182 | 0.599 | 1.237 | 2 of 7 |
| A=683 L=2048 o1 | 0.365 | 0.407 | 1.035 | 2 of 7 |
| **A=683 L=1024 o1** | **0.718** | **0.340** | **0.698** | **6 of 7** |
| A=683 L=1024 o2 | 1.020 | 0.318 | 0.533 | 5 of 7 |
| A=1365 L=2048 o3 | 1.238 | 0.376 | 0.600 | 4 of 7 |

with the mip bias swept separately: -1.00 puts the median grain at **+2 %** of
vanilla's (4.565 against 4.476), where 0.00 leaves it at -27 % and -1.25 overshoots
to +13 %.

**The gate was not met and this lane does not call 6 of 7 a pass.** The red is named
with its numbers: chunk **(-36,-20)**, repeat 0.095 (ceiling 0.264, control 0.004)
and ratio 0.698 (ceiling 0.448). Two further honesties: the picked strain 0.718 is
above `a5_tune.py`'s own note that anything over 0.5 is a visible wobble, and the
picture shows that wobble as a faint swirl; and the same selection rule with the
outlier sheet (28,-20) dropped picks a *different* mip bias (-1.25), so the pick
does turn on one sheet.

The C++ transcription was checked against the prototype at fifteen world positions
x five settings -- **worst disagreement 0 world units** to the probe's nine
significant digits, with OFF proved to return the coordinate untouched and every ON
setting proved to move it.

## 4. What shipped, and what did not

| switch | default | what the default does |
|---|---|---|
| `--land-detail-source none/vanilla/vanilla-blend` | **`vanilla`** | vanilla's `_msn` byte for byte where one is shipped; vanilla's COLOUR byte for byte on chunks with no land paint; the crevice term on every other chunk's colour |
| `--vanilla-lod-root PATH` | `E:/Tools/Fallout 4/DataUnpacked/Data` | where vanilla's sheets are read, **as loose files**, never through the resource stack |
| `--land-shade K` | **-3.242** | the crevice coefficient, in 8-bit luminance levels per unit of detail-normal divergence |
| `--land-warp`, `--land-warp-lattice`, `--land-warp-octaves`, `--land-mip-bias` | **0 / 1024 / 1 / 0 = OFF** | nothing: the rung's expression, compiled |
| `--land-sample stochastic` | not the default | sets the warp to 683 / 1024 / 1 octave with mip bias -1.00 -- **the proposal** |

The repeat fix is **off by default** because its own gate scored 6 of 7. One flag
turns it on, and section 3 gives bungo the numbers to decide on.

The vanilla sheets are read as LOOSE FILES under `--vanilla-lod-root` and never
through the resource stack. That is deliberate: the stack would serve our own
previously installed output out of the game's `Data`, and the bake would "reuse
vanilla" by copying yesterday's copy of itself.

## 5. bungo's rulings, verbatim, and what it cost to implement them

> "For now, I think we can use vanilla normal map for those tiles, use those
> details for the diffuse."

> "so now we do not use our own normal map if that is toggled, but reuse these ones
> for terrain chunks."

> "out of bounds terrain blends are not included in the actual cells out of bounds,
> they never were, so we can't recover the color data anymore, because it was baked
> in a different tool outside of fo4."

> the G channel of vanilla's `_msn` (up, = cos of the slope angle) "is the slopeness
> for the micro details our heightmap bakes do not possess"

**The `_msn`.** A chunk with a shipped vanilla `_msn` writes vanilla's file, byte
for byte; our normal bake is skipped for it. No guard with a number in it -- the
only question a chunk is asked is whether the file exists.

**The out-of-bounds colour.** A chunk whose cells carry no land paint at all (no
base texture and no alpha layers on any quadrant of any cell) writes vanilla's
COLOUR file byte for byte on the same rule. The classification is **per chunk, by
"any cell has paint"** -- the coordinator's named fallback -- because the shipping
writer assembles a chunk sheet from four virtual-texture tiles and has no
per-quadrant seam to split on at that point. It errs toward keeping our composite:
a partly painted chunk counts as painted.

**The diffuse detail, and this is where the measurement contradicted the
instruction.** The ruling asks for a *shading* of vanilla's normal detail. Fitted on
seven vanilla sheets, a Lambert shading of that detail **reads zero**: R^2 0.0000 at
a twin floor of 0.00001, with the up coefficient flipping from -1.34 to +0.45 sheet
to sheet. The fit machinery is sound -- a known-answer control recovers an injected
0.5 as 0.5000. A Lambert dot cannot see a rill, because a rill's two walls tilt
opposite ways and their dots cancel.

What does measure is the **divergence** of the same field -- crevice darkening, dark
in the channel, light on the ridge:

| sheet | r | r (twin) | kDiv |
|---|---|---|---|
| (-20,24) | -0.1222 | -0.0055 | -3.242 |
| (-20,20) | -0.1353 | +0.0023 | -4.392 |
| (-36,-20) | -0.0770 | -0.0009 | -1.597 |
| (-4,-20) | -0.1046 | +0.0024 | -3.486 |
| (28,-20) | -0.0136 | +0.0006 | -0.135 |
| (-4,16) | -0.1342 | +0.0004 | -5.655 |
| (24,16) | -0.0949 | +0.0010 | -2.836 |
| **median** | **-0.1046** | **+0.0006** | **-3.242** |

Same sign on 7 of 7 sheets, over its own phase twin on 7 of 7. It recovers about
**1 %** of the colour's fine variance (median R^2 0.011), not most of it, and
nothing in this lane claims otherwise. The ruling's intent is delivered -- vanilla's
own fine geology reaches the diffuse, rills and drainage where vanilla has them --
by the term that measures rather than by the term that was named. The refuted
Lambert coefficients are printed in `logs/d3_shade.txt` as a refutation, not as a
setting.

**The 23:5x refinement, tested and not adopted -- and the test is the reason.**
bungo then named a better-motivated driver: the `_msn`'s G channel is the cosine
of the slope angle, so the micro-detail our heightmap cannot see is a
MICRO-STEEPNESS, `dTheta = acos(up fine) - acos(up coarse)`, with no light
direction and no view dependence; darken and shift toward the rock tint with it;
"keep the better one, say which". Measured on the same seven sheets, against the
same colour residual, with the same phase-twin floor (`d4_steep.py`,
`logs/d4_steep.txt`):

| driver | median r | median twin | sign agreement | beats twin |
|---|---|---|---|---|
| A micro-steepness, `acos` form | +0.0022 (median \|r\| 0.0039) | -0.0010 (0.0018) | 4 of 7 | 6 of 7 |
| A' the same in cos space (`up_fine - up_coarse`) | -0.0046 | -- | -- | -- |
| **B the crevice term, divergence** | **-0.1046 (\|r\| 0.1046)** | +0.0006 (0.0010) | **7 of 7** | **7 of 7** |

**B beats A on 7 of 7 sheets, by 27x in median |r|, and A's sign agreement (4 of
7) is chance.** The instruction's own rule -- adopt A only if it matches at least
as well -- therefore keeps B, and nothing in the exe changed for it: no rebuild,
no relink. Two details that make the result trustworthy rather than merely
convenient: taking `coarse` from OUR OWN baked `_msn` instead of vanilla's mip 2
moves A's reading by at most 0.0019 (so the null is not an artefact of the
stand-in), and the cos-space form -- which IS the `dUp` column d3_shade.py
already fitted -- reads the same null, so `acos` is not hiding a signal.

**The tint half of that sentence also reads zero, and this matters for what the
code does.** Correlating micro-steepness with each colour channel's own residual
gives +0.0015 / +0.0023 / +0.0021 (R/G/B) with the three channels moving together
to within 0.0018, and the saturation residual at -0.0016. There is no measurable
hue shift with micro-steepness in vanilla's sheets. The shipped term adds the
same `dL` to all three channels -- a pure darkening -- which is exactly what the
measurement supports and nothing more.

**One real finding did come out of his refinement, and it is his, not the crevice
term's.** Asked as an ENVELOPE question -- "is the ground more DETAILED where it
is micro-steep?" rather than "is it darker there?" -- `|dTheta|` reads **+0.1269**
median against a twin floor of +0.0040, beats that twin on **7 of 7** sheets, and
beats the divergence's own envelope on **5 of 7**. So micro-steepness is the
better predictor of WHERE vanilla's grain lives; it is simply not a predictor of
its SIGN, and a colour writer needs a sign. Shipping it would mean modulating an
amplitude rather than adding a level -- a different term, a second fitted
parameter and a second build -- so it is recorded here as the strongest available
lead for the next lane, not smuggled in as this one.

**`vanilla-blend`** exists as the second value for reshaped terrain: vanilla's fine
detail over OUR coarse normal, with up recomputed from east and north so the stored
normal stays unit length. It cost no extra build.

The `_msn` channel order used throughout is the one this tree documents at
`src/lodgen.cpp:5566`: **R = east, G = up, B = north**.

## 6. The gates, with their counts

**F2 -- off is the rung's bytes.** `--land-detail-source none` on the new exe against
a fresh bake by `release/NifSkope.before_tiling3.exe`: **9 files each on both tiles,
0 differing** -- containers, BTR, BTO, manifest, all three sheets.

**F2 -- 1 vs 16 chunk threads.** The 16-chunk block (-36,20)..(-21,35) at the shipped
defaults: **97 files, 0 differing.**

**F2 -- what the default moves and what it does not.** Against the rung, on both
tiles: the colour DDS and the `_msn` DDS change; `_data`, the BTR, the BTO, the BTO
manifest, `Commonwealth.VT.2.lodt`, `Commonwealth.VT.4.lodt` and
`Commonwealth.VT.lodm` are **byte-identical**.

One pre-registered clause of F2 is **superseded by the ruling and it is named here
rather than quietly dropped**: F2 as registered said "`.lodl` and `_msn` byte-
identical at EVERY setting". The `.lodl` clause holds. The `_msn` clause cannot, and
must not -- bungo's correction made the `_msn` the deliverable. The replacement gate
is the one he dictated, and it is stricter:

**The `_msn` census gate.** Every chunk written with a vanilla sheet available:
output `_msn` **cmp == 0** against vanilla's file -- **18 of 18** checked across the
two tiles and the 16-chunk block. Every chunk without one: **cmp == the rung** --
proved by a known-answer control, `--vanilla-lod-root` pointed at an empty
directory, which reproduced the rung's bytes on all 9 files.

**The chunk classes, counted.**

| region | chunk sheets | layered | layerless | layerless with no vanilla colour | `_msn` copied | colour copied |
|---|---|---|---|---|---|---|
| t2024 (-20,24) | 1 | 1 | 0 | 0 | 1 | 0 |
| t2020 (-20,20) | 1 | 1 | 0 | 0 | 1 | 0 |
| edgeN, 4x4 chunks | 16 | 7 | 9 | 0 | 16 | 9 |
| census, cells (-48,-24)..(-1,23) | 180 (dim 4 and dim 8) | 121 | 59 | 0 | 180 | 59 |

Both of the brief's tiles are fully painted, so both take our composite plus the
crevice term and vanilla's `_msn`. **Nothing anywhere fell into the "layerless with
no vanilla sheet" class**, and nothing fell into "no vanilla `_msn`": Bethesda ships
a complete 48x48 dim-4 grid over cells -96..95, which covers the whole worldspace,
so on Commonwealth the fallback branch is unreachable in practice. It is still
implemented, still tested by the empty-root control, and still the behaviour for any
worldspace that ships no LOD.

The census line is printed by the bake itself, unconditionally, in the `report`
block: `landDetail`, `vanillaRoot`, `msnCopied`, `msnOurs`, `colCopied`, `colOurs`,
`chunksLayered`, `chunksLayerless`, `chunksLayerlessNoVanilla`, `chunksShaded`,
`landShade`. One count in it was suspected of double-counting (180 sheets for a
144-chunk region) and the suspicion was **refuted, not waived**: the assembler also
writes dim-8 chunk sheets, 144 + 36 = 180, and 540 files = 180 x 3.

**F3 -- the repeat gone AND the grain kept, on the same bake, both tiles.**

| variant | tile | repeat | vanilla's | grain vs vanilla | verdict |
|---|---|---|---|---|---|
| rung | (-20,24) | 1.037 | 0.201 | 75 % | repeat RED |
| rung | (-20,20) | 1.261 | 0.032 | 79 % | repeat RED |
| **ship** (the default) | (-20,24) | 1.042 | 0.201 | 76 % | repeat RED, grain OK |
| **ship** | (-20,20) | 1.242 | 0.032 | 79 % | repeat RED, grain OK |
| stoch (the proposal) | (-20,24) | **0.183** | 0.201 | **111 %** | **both OK** |
| stoch | (-20,20) | 0.593 | 0.032 | 99 % | repeat RED, grain OK |

`off` reads exactly the rung's numbers, which is F2 again through a second
instrument. **The shipped default does not fix the repeat and this report does not
pretend it does** -- it was never asked to; it delivers the ruling. The proposal
passes both halves on one tile and fails the repeat half on the other, which is the
same 6-of-7 verdict `a6_pick.py` reached, now on real bakes.

One caveat the table forces: on these two single chunks every reading, vanilla's
included, sits below the instrument's own 12-period null sweep. That sweep is too
coarse a significance floor at 512 texels on one chunk; the threshold the verdicts
are graded against is the one frozen on 22 shipped sheets (absolute 0.264).

**F4 -- the chain, at TILING2's baselines.** Run 23:36-23:42 on the new exe:

| harness | baseline | this lane |
|---|---|---|
| lodl_open | 23 / 0 | 23 / 0 |
| lodgen_terrain | 26 / 0 | 26 / 0 |
| lodgen_terrain_vt | 41 / 1 | 41 / 1 |
| lodgen_roads | 11 / 0 | 11 / 0 |
| lodgen_ground_cover | 29 / 5 | 29 / 6 -> **29 / 5** after the harness fix below |
| lodgen_terrain_pbrm | 14 / 0 | 14 / 0 |
| lodgen_native | 18 / 0 | 18 / 0 |
| lodgen_panel_run | 125 / 0 | 125 / 0 |
| lod_generation | 116 / 0 | 116 / 0 |
| ui_align | 11 / 0 | 11 / 0 |
| water_ui | 82 / 0 | 82 / 0 |

The one extra failure was real and is worth stating plainly. `lodgen_ground_cover.sh`
check C1 asserts that all three of a chunk's sheets are 174,888 bytes -- DXT1 with a
full mip chain. Under the new default the `_msn` is vanilla's own file, which
Bethesda encodes as BC5: **349,680 bytes**. The harness was reading the ruling
working as a ground-cover regression. Per this tree's own rule that a harness forces
the state it measures rather than inheriting it, the ground-cover bakes now pass
`--land-detail-source none` explicitly, with a comment naming the lane and pointing
at the gates that do cover the default. Re-run at 23:44: **29 checks, 5 failures, and
its ok/FAIL list is now line-for-line identical to TILING2's.** No rebuild: a shell
harness, not the exe.

**F4 preflight.** `release/NifSkope.exe` 2026-09-11 23:26:29, 21,484,032 B, newer
than every source file it depends on. Rung `release/NifSkope.before_tiling3.exe`
21,466,624 B == the launch exe's size, intact. Fallout4.exe down before the build and
before every bake. No NifSkope process left running. One build, no relinks after it.

## 7. The picture

`images/cmp_tiling3.png` (1078 x 1454) -- chunk (-20,24), the same 128 texels at
(224,96) as `cmp_tiling2.png`, 4x nearest, numbers burned in. **All four panels are
real DDS off disk**; the modelled row an earlier revision carried is gone, and with
it its "compare down a row, never across" caveat. Vanilla | the rung | the shipped
default | the proposal. On that crop: repeat 0.456 / 2.084 / 2.055 / 0.598, grain
4.416 / 3.463 / 3.543 / 5.135, local variance 19.46 / 12.92 / 13.41 / 26.53.

## 8. Documents, harness and skills

**Documents written.**

* `docs/LODGEN_TERRAIN_VT.md` -- new **§2.5b** (where a chunk's sheets come from,
  the three-way decision, the loose-file rule, the class census), **§2.5c** (the
  crevice term, the two shadings that measure zero, and the 22-column ceiling),
  **§2.5d** (`vanilla-blend`), six new rows in **§5 The CLI**, and a **TILING3
  provenance block** with the three source hashes and fifteen re-found anchors.
* `scratchpad/tiling3_20260911/WW_CHANGES_ENTRY.md` -- the changelog text, for the
  overseer to splice.
* `scratchpad/tiling3_20260911/HANDOFF_BLOCK.md` -- with all four of bungo's
  rulings verbatim.
* `scratchpad/tiling3_20260911/MISTAKES_ENTRIES.md` -- four entries: the false
  fallback claim in a log line, a per-sheet gate graded without a per-sheet floor,
  a brief's premise carried as if it were a finding, and a harness reading a ruling
  working as a regression.
* `scratchpad/lodui1_20260911/BAKE_INSTRUCTION.md` -- the new default, the census
  line to read it back, the `_msn` size change (about 400 MB instead of 200 MB for
  a whole-Commonwealth dim-4 bake), and the stochastic switch offered with its
  numbers.

**Harness.** `tests/spells/lodgen_ground_cover.sh` now pins
`--land-detail-source none` on its own bakes. Shell only; no rebuild.

**Skills.** Two amended and one new, **in both trees**
(`E:\Projects\NifskopeWildWastelandEdition\.claude\skills` and the live mirror
`E:\Projects\Claude\.claude\skills`):

* `ww-module-off-is-identical` -- new **§7**: when the new value is the DEFAULT,
  the harness fleet is part of the deliverable; read every chain delta as "which
  stale invariant did my default break"; **pin the state, never relax the
  assertion**; and `off == rung` then proves only that a way back exists, so the
  default needs its own gate. The closing "what this does NOT prove" was sharpened
  to match.
* `nifskope-ww-vanilla-compare` -- new **§2a**: *vanilla is a path, not a stack*.
  Any read through the resource system resolves by load order and will serve our
  own installed output the moment the generated mod is enabled, producing a
  spectacular agreement that means nothing. Read loose files by absolute path,
  make the root an explicit argument, validate magic bytes, count reused against
  made-our-own in the run's own output, and prove the root matters by pointing it
  at an empty directory.
* `ww-explained-variance-ceiling` (**new**) -- the method that saved this lane
  twice. Known answer, then phase-twin floor, then the **ceiling regression**
  before any coefficient; read a small R^2 as amplitude and check the remainder's
  moments; choose between two drivers on the same residual with the same floor and
  report sign agreement; and the **envelope-versus-sign** distinction, which is what
  decides whether a term can be written into a colour at all.
