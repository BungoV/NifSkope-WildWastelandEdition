<!-- SUPERSEDED for sections 3 and 4: this file was composed while the lane
was BUILD PENDING. The build was spent at 2026-09-12 04:10:38 and sections 3
and 4 of scratchpad/lane_roads3_report.md were rewritten from sec34.md with
the real build and gate numbers. Read the report, not this file. -->


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

### 3.1 THE BUILD WAS NOT SPENT -- the game is up

```
2026-09-12 03:57:20   tasklist | Fallout4.exe   22908   Console   1   8,305,996 K
```

Standing rule, verbatim from the charter: *"tasklist for Fallout4.exe before any
build or exe launch (game up = end BUILD PENDING)"*. So **no compiler was run,
no exe was linked, no NifSkope was launched, and no harness was run.** The
source is edited and reviewed by eye; it has not been compiled, and that is
stated rather than implied. `scratchpad/roads3_20260911/PENDING.md` has the
resume steps in order, and the three touched files are backed up beside it
(`lodgen.h.bak`, `lodgen.cpp.bak`, `nifcli.cpp.bak`) so the edit can be undone
without `git` touching anything else in this shared tree.

The exe in `release/` is therefore still GRADE1's, untouched, and the rung copy
`release/NifSkope.before_roads3.exe` is still a byte copy of it
(21,489,152 B, md5 `6af74b4b4667ce50c4506a2d42a04fdf`).

### 3.2 The gate table as it stands

| gate | state | the number, beside its floor |
|---|---|---|
| **F1** fit the opacity law with a floor and a ceiling before any code | **MET, and the law is REFUSED with numbers** | unexplained 17.6 % against a shuffled-ground floor of 18.2 % on (-20,20); 52.6 % against 51.5 % on (-8,8), i.e. worse than the floor; ceiling (vanilla against its neighbour sheet) 18.1 % / 39.2 %. Alignment control: the best shift over +-3 buys 0.005 |
| F1 detail strength | **MET** | correlation +0.0275 against phase-twin floor 0.0270/0.0644; +0.0162 against 0.0124/0.0163; best strength negative |
| F1 hue named by measurement | **MET** | opponent-axis rises above; on the road texels themselves (-20,20) vanilla b_y -12.62 against ours -12.39, saturation 0.162 against 0.161 |
| **F2** every switch at its off value == the rung's bytes, `cmp` every file, compare shown able to fail | **PENDING BUILD** | by construction the multiply is branched over at 1.0; NOT yet demonstrated by `cmp`, and construction is not a measurement |
| **F3a** road mean luminance within 3 of vanilla | **REFUSED ON (-8,8) BY ARITHMETIC; simulated on (-20,20)** | the composite can only land between our ground 102.12 and our paint 106.68; vanilla is 94.59, outside by 7.53. On (-20,20) a = 0.83 gives 92.57 against vanilla 92.52 (+0.05) but takes the rise to +23.48 |
| **F3b** road hue within 3 of vanilla | **MET AS A RISE, unreachable as an absolute** | rises: 1.05 / 1.11 levels apart on (-20,20), 2.14 / 1.83 on (-8,8). As an absolute, d(b_y) is -6.5 to -6.7 for EVERY rule including the rung, because our ground's own b_y on (-8,8) is -12.79 against vanilla road's -6.25 |
| **F3c** road local SD within 20 % of vanilla's | **SIMULATED, and it fights F3a/F3e** | rung 7.26 / 6.59 = 1.10 on (-20,20) (inside 20 %), and every opacity below 0.75 falls out of the band: a = 0.326 reads 4.20 / 6.59 = 0.64 |
| **F3d** road-edge 10-90 % width inside vanilla's | **REFUSED AS UNRESOLVABLE, with the number** | vanilla's 4.29-level rise under a 6.59-level local SD, SNR 0.65. TILING2's addendum agrees from the other side: both widths 6.00 texels |
| **F3e** no step where vanilla has none | **SIMULATED** | max second difference on (-20,20): vanilla 1.31, rung 3.88 at d=3, a = 0.326 gives 1.85, a = 0.83 gives 3.37. On (-8,8) the rung is ALREADY smoother than vanilla: 1.25 against 4.43 |
| **F3f** `lodgen_roads.sh` stays 11/0 | **PENDING BUILD** | R5 floor 0.1347, ROADS2 after 0.3431, bars 0.2694 / 0.3231 -- no bar touched by this lane |
| **F3g** S1 seam not worse than 4.242 | **PENDING BUILD** | at the shipped default the bytes do not move, so the seam number cannot move; to be shown, not asserted |
| **F3h** raised-highway clearance +0.001 | **PENDING BUILD** | `--road-raised` untouched by this lane |
| **F4** the harness chain at GRADE1's baselines, exe newer than every changed file, rung == launch bytes, no NifSkope left running | **PENDING BUILD** | rung == launch bytes IS verified (md5 `6af74b4b4667ce50c4506a2d42a04fdf`, both 21,489,152 B); no NifSkope process was started by this lane at any point |

Read plainly: **everything that could be measured without the compiler is
measured and is in section 1; everything that needs the compiler is owed and is
named as owed.** Nothing in the F3 row set is claimed as passed on a built exe,
because there is no built exe.

## 4. Pictures

Both are in `scratchpad/roads3_20260911/images/`, made by `r3_pics.py` from the
rung's own bakes and Bethesda's shipped sheets.

**`cmp_road_wash.png`** -- two rows, chunk (-20,20) on top and (-8,8) beneath,
four columns each: vanilla | the rung (which IS `--road-opacity 1`) |
`--road-opacity 0.326` | `--road-opacity 0.83`. Every panel is the same crop of
the same 512-texel grid at 32 world units a texel, nothing resampled on any
side, and each carries its own road mean luminance, its rise over the surround,
its local 5x5 SD and its biggest step. The crops are not chosen after the
numbers: (-20,20) keeps lane ROADS1's own (150,120)-(300,270), and (-8,8) is
picked by a rule that only looks at the road mask -- the 96x96 window holding
the most road texels, which came out at (112,116)-(208,212) with 3,176 of them.

**The third and fourth columns are SIMULATED and say so on the picture.** The
generator's composite is `colour = ground + ( paint - ground ) * a`, the grass
tint never fires on these chunks and `--grade` is 1.0 with its multiply branched
over, so the arithmetic in the picture is the arithmetic the C++ runs -- but it
does not pass through the 8-bit quantisation and the BC1 compression a baked
sheet does, so those panels are right to about half a level, not to the byte.
They must be re-taken from real bakes when the build is spent.

What the picture shows, in plain words: at 0.326 the Sanctuary road stops being
a stripe and starts being ground with a road on it -- and goes 19 levels darker
than vanilla's while doing it. At 0.83 it sits on vanilla's brightness (+0.05)
and is still a stripe (+23.48 against vanilla's +4.29). The downtown row barely
changes at all, because there our paint and our ground are already 4 levels
apart.

**`cmp_road_profile.png`** -- the cross-road luminance profile, mean luminance
against signed distance to the mask edge, one plot a tile, with vanilla, the
rung, our own `--no-roads` ground and the two candidates on one axis, and the
per-texel opacity the wash would need drawn against its own scale on the right,
which is what the brief asked for beside the luminance. Vanilla reaches its full
value one texel in and runs flat; ours ramps over three texels, plateaus near
96.5 and climbs again to 105.5 in the core. That darker outer band around a
brighter core is the two-tone bungo is seeing, and it is the opposite way round
from the guess in the brief.

## 5. Owed / red / bungo's calls

**BUNGO'S CALL 1 -- the default.** The switch ships at 1.0, which means this
lane changes nothing he can see until he says so. The priced table, both tiles,
simulated:

| `--road-opacity` | (-20,20) road L (vanilla 92.52) | (-20,20) rise (vanilla +4.29) | (-8,8) road L (vanilla 94.59) | (-8,8) rise (vanilla +4.40) |
|---|---|---|---|---|
| **1.000 (shipped)** | 99.05 (+6.53) | +29.96 | 106.68 (+12.09) | +3.84 |
| 0.830 | 92.57 (+0.05) | +23.48 | 105.91 (+11.31) | +3.07 |
| 0.500 | 80.01 (-12.52) | +10.91 | 104.40 (+9.81) | +1.56 |
| 0.326 | 73.38 (-19.15) | +4.28 | 103.61 (+9.02) | +0.77 |
| 0.250 | 70.48 (-22.04) | +1.39 | 103.26 (+8.67) | +0.42 |

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

**OWED -- the build.** F2, F3f, F3g, F3h and F4 all need the compiler. Resume
steps are in `scratchpad/roads3_20260911/PENDING.md`, in order, with the exact
commands.

**OWED -- the pictures re-taken from real bakes** once the exe exists, so the
two simulated columns become baked ones.

**NOT OWED, and said so on purpose:** vanilla's edge width, vanilla's road hue
as a separate defect, and a `--road-detail` refit. All three were measured and
all three came back as "there is nothing here" with a floor beside the number.

## 6. Mistakes

Both are in `scratchpad/roads3_20260911/MISTAKES_ENTRIES.md` in the ledger's
format, ready to splice into `MISTAKES.md`.

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
wherever it reaches a picture.

**Amended: `.claude/skills/ww-control-calibration/SKILL.md`** with one
paragraph: a floor must be checked for NaN and for zero variance before it is
believed, because a translated mask-shaped signal is all zeros on the mask and
`corrcoef` then returns NaN, which reads like a passing floor in a table.

Both are mirrored into `E:\Projects\NifskopeWWE_ui\.claude\skills\` so the
parallel tree has them.
