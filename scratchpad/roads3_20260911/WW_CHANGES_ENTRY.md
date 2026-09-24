## 2026-09-12 — What vanilla's far road really is: a wash that follows the ground, and the knob that can make ours one

bungo looked at the shipped road and said *"no diffuse is sampled from the road,
only like solid colors"* and *"our road wasn't flat before, but now it is, and
it still has the seam at the edges of it"*. Both halves were taken as questions
and measured on Bethesda's own sheets before a line of code was written — two
chunks a whole biome apart, (-20,20) at Sanctuary and (-8,8) downtown, 512
texels at 32 world units each, nothing resampled on either side, every number
printed beside the floor it was read against.

**Vanilla's far road is not painted, it is washed.** Regress each road texel's
luminance on the mean luminance of the non-road ground within 8 texels of it:
vanilla's road tracks its own surroundings with a slope of **+0.714** on
(-20,20) and **+0.755** on (-8,8) — as strongly as unpainted ground tracks its
own neighbourhood (**+0.637** and **+0.565**) — while ours tracks it at
**+0.339** and **+0.209**, about half. Floors, both sides: the same field
translated reads a slope of −0.009 (worst 0.298) over five draws, a known-answer
road of a fixed colour reads +0.000, and a known-answer road of ground+4 reads
+1.000. Vanilla's road sits **+4.29** levels above the ground around it on one
tile and **+4.40** on the other. Ours sits **+29.96** and **+3.84**.

Read that last line twice, because it is the whole finding: **our road is right
on one chunk and 25.67 levels too contrasty on the other**, and the difference
is not in the road. Our paint is a fixed material colour (99.05 and 106.68 on
the two tiles) while our ground swings from 60.96 to 102.12 between them. Where
the ground happens to land near the paint the road looks like vanilla's; where
the ground is 38 levels darker, the road is a stripe.

**The hue is not the defect, and now there is a number for it.** Road minus
surround on the opponent axes: vanilla goes +2.30 bluer and −2.14 less red on
(-20,20), ours +3.35 and −3.25; on (-8,8) vanilla +4.10 and −3.07, ours +1.96
and −1.24. Same sign, same direction, every gap under three levels of 255. On
the road texels themselves at Sanctuary, vanilla's blue-yellow axis reads −12.62
against our −12.39 and saturation 0.162 against 0.161. The "soft blue-grey blob
against a crisp warm ribbon" that lane ROADS2 owed a measurement for is a
brightness difference, not a colour one.

**Vanilla keeps none of the road texture's own detail.** The residual after the
best wash correlates with our full-detail bake's departure from its flat average
at **+0.0275** against a phase-twin floor of 0.0270 mean / 0.0644 max on
(-20,20), and **+0.0162** against 0.0124 / 0.0163 on (-8,8) — the correlation IS
the floor at every blur radius on both tiles, and the best-fit strength is
negative. A test that could have overturned that answer did not.

**And that measurement does not get to pick the default.** bungo looked at both
bakes on 2026-09-12 and ruled — *"--road-detail 1 is always on, do not ever use
road detail 0, that looks terrible"*. What vanilla measures is why the flag
exists and what its `0` end means; it is not an argument about how our roads
should look. The measurement above stands exactly as written and no longer
carries a recommendation with it.

**The two-tone edge is real and is the opposite way round from the guess.**
Vanilla's profile across the road reaches full value one texel in and runs flat
(92.70, 92.57, 92.54, 92.18 …). Ours ramps over three texels (79.86, 86.48,
93.45), plateaus near 96.5, then climbs again to 105.5 in the core — a **darker
outer band around a brighter core**, which is the alpha-blended skirt mixing our
paint into a ground that is far darker, with the wider trunk material inside it.
The biggest step inside the road is **3.88 for ours against vanilla's 1.31** at
Sanctuary; downtown ours is already the smoother of the two, **1.25 against
4.43**. Vanilla's edge WIDTH could not be measured honestly and is refused with
its number: a 4.29-level rise under a 6.59-level local texture SD is a
signal-to-noise of **0.65**, so any width fitted there is a reading of the
terrain, not of the road.

**What ships: `--road-opacity A`, default 1.** It scales how strongly the road
paint is mixed into the ground under it — `colour = ground + (roadColour −
ground) × coverage × A` — and at the default of 1 the multiply is branched over
entirely, so the bake is the previous bake's bytes. That last part is not an
argument about the code: every file of both bakes was compared on both chunks,
and the new exe with no flag, and with `--road-opacity 1`, and with
`--roads-legacy`, reproduces the previous exe's output **9 of 9 and 10 of 10
files identical in all three arms**. The same run shows the compare can fail:
`--road-opacity 0.326` moves 3 files on one chunk and 4 on the other, and every
one of them is a colour sheet — the normal sheet, the wetness sheet, the objects
and the mesh files are byte-identical at every setting. The
ground-cover suppression deliberately keeps using the UNSCALED coverage, so
`--road-opacity 0` means "paint nothing here" and not "there is no road here":
grass still does not grow through it. `--roads-legacy` restores 1 unless the
caller named a value on the same command line.

**The default did NOT move, and that is a refusal with arithmetic, not
caution.** Every candidate was priced on the already-baked sheets, using the
generator's own composite, before any code existed — and the three that mattered
were then baked for real and read the same way. On (-8,8) **no opacity can
work at all**: the composite can only land the road between our ground (102.12)
and our paint (106.68), and vanilla's road is at 94.59 — 7.53 levels outside
that interval. On (-20,20) the gates want four different values: vanilla's
absolute brightness needs 0.83, vanilla's rise over the ground needs 0.326, the
two-tone step needs 0.25 or less, and keeping the local detail SD inside 20 % of
vanilla's needs 0.75 or more. The 22 levels between them are the **ground's** —
our ground on that chunk is 19 levels darker than vanilla's, which lane GRADE1
measured as a per-cell content difference and lane TILING2 said in writing must
not be chased with the road pass. Turning the road down there buys a calmer
stripe and pays 19 levels of darkness for it. The priced table for both tiles,
and the pictures, are in `scratchpad/lane_roads3_report.md` for bungo's call.

A ground-relative mode — pick the opacity per texel so the road sits a fixed
rise above its local ground — was built, simulated and **rejected with its
numbers**: it lands the Sanctuary road 25 levels below vanilla with a rise of
**−1.8** because most of its texels clamp to zero, and it halves the downtown
local SD to 0.48 of vanilla's. It is recorded so nobody proposes it again
without new evidence.

**It was then built and gated.** One build, first try, no extra relinks; the
new `release/NifSkope.exe` is 2026-09-12 04:10:38, 21,489,152 B. The eight
harnesses come back at the previous lane's counts row for row — `lodgen_roads`
11/0, `lodgen_terrain` 26/0, `lodgen_terrain_vt` 41/1, `lodgen_ground_cover`
29/5, `lodgen_terrain_pbrm` 14/0, `lodgen_native` no failures in any of its
seven sections, `lodl_open` 23/0, `lod_generation` 116/0. The single red row in
`lodgen_terrain_vt` was made to answer for itself by re-running that harness on
the OLD exe: it fails there with digit-for-digit identical numbers, so it is not
this change's. Baked, `--road-opacity 0.83` puts the Sanctuary road within
**0.09 of a level** of vanilla's brightness and `0.326` puts it within **0.28**
of vanilla's rise over the ground — and neither does both, which is the 22-level
gap above.

**One thing this change cannot do, with the number.** The two-tone band is the
road mesh's own ramped skirt showing through. Correlating road luminance with
the mesh's interpolated vertex alpha: vanilla reads **+0.001**, ours reads
**−0.792** at the default, **−0.694** at 0.83 and **−0.436** at 0.326 — while our
own unpainted ground on the same texels reads **−0.325**. So turning the opacity
down walks the skirt signature toward the ground's own floor and can never reach
vanilla's zero: the ramp is still in the geometry underneath. Removing it is a
mesh change, not a colour one, and it is not attempted here.

Pictures: `scratchpad/roads3_20260911/images/cmp_road_wash.png` (both chunks,
vanilla | shipped | two candidate opacities, all four columns real bakes, every
number burned in) and `cmp_road_profile.png` (the cross-road luminance profile
with the per-texel opacity the wash would need on the same axis).
