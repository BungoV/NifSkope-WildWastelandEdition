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
| kept aside as | `release/NifSkope.before_roads3.exe` -- a byte copy, same md5, **equal to the exe this lane found at launch** |
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
| F1 detail strength | **MET** | correlation +0.0275 against a phase-twin floor of 0.0270 mean / 0.0644 max; +0.0162 against 0.0124 / 0.0163; best-fit strength negative. `--road-detail` stays 0 |
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
