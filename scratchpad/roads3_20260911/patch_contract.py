"""ROADS3's amendments to docs/LODGEN_TERRAIN_VT.md section 1a.

Four places, each asserted before it is written:
  1a.4   the cover suppression uses the UNSCALED coverage, and why
  1a.5d  NEW -- what vanilla's far road actually is, and how strongly ours is
         mixed in, with the provenance of every number
  1a.7   `roadOpacity` in the census list
  1a.8   `--road-opacity 1` named as part of the way back

Written as a file, not a heredoc: prose apostrophes.
"""
import io
import sys

P = r'E:\Projects\NifskopeWildWastelandEdition\docs\LODGEN_TERRAIN_VT.md'
s = io.open(P, encoding='utf-8', newline='').read()
if '\r\n' in s:
    sys.exit('REFUSED: the contract is not LF-only')
if '1a.5d' in s:
    sys.exit('REFUSED: 1a.5d already present')


def sub(old, new):
    global s
    if s.count(old) != 1:
        sys.exit('REFUSED: %d matches for %r' % (s.count(old), old[:70]))
    s = s.replace(old, new)


# ------------------------------------------------------------------- 1a.4
OLD = """* **Before the tint**, because the tint's weight is the cover byte and a road
  suppresses the cover under it: `cover *= 1 - coverage x roadCoverSuppress`
  (default 1). Grass grows beside a road, not through it. On the chunk path the
  cover PLANE is rewritten with the same byte, so what a consumer reads out of
  the sheet's alpha is what the tint used."""
NEW = """* **Before the tint**, because the tint's weight is the cover byte and a road
  suppresses the cover under it: `cover *= 1 - coverage x roadCoverSuppress`
  (default 1). Grass grows beside a road, not through it. On the chunk path the
  cover PLANE is rewritten with the same byte, so what a consumer reads out of
  the sheet's alpha is what the tint used.

The `coverage` in that formula is the road plane's alpha **unscaled by
`--road-opacity`** (lane ROADS3): the opacity says how strongly the paint is
mixed into the colour, not whether there is a road there. A road painted at
`--road-opacity 0` still suppresses the cover under it, because the mesh is
still lying on the ground. At the default of 1 the two are the same value, so
this distinction changes no shipped byte -- it decides what the knob MEANS at
its other settings."""
sub(OLD, NEW)

# ------------------------------------------------------------------ 1a.5d
ANCHOR = '### 1a.6 The channels touched'
NEW_SECTION = """### 1a.5d How strongly the paint is mixed in, and what vanilla's road actually is (lane ROADS3, 2026-09-12)

`--road-opacity A` (default **1.0**, clamped to 0..1) scales the road plane's
alpha into the composite of 1a.4:

```
colour = ground + ( roadColour - ground ) * coverage * roadOpacity
```

At 1.0 the multiply is not performed and the branch is entered on exactly the
same condition as before, so the default is the previous bake's BYTES by
construction and not an argument about `1.0f` -- the same discipline
`g_landGrade != 1.0f` uses in 2.5f.

**Provenance of every number below.** Two chunks, measured on the exe of
2026-09-12 03:06:21 (lane GRADE1's, copied aside as
`release/NifSkope.before_roads3.exe`, md5 `6af74b4b4667ce50c4506a2d42a04fdf`):
(-20,20) Sanctuary and (-8,8) downtown, 4x4 cells each, 512 texels at 32 world
units a texel, against Bethesda's own `Commonwealth.4.<x>.<y>.DDS` on the same
grid with no resampling on either side. Scripts and logs:
`scratchpad/roads3_20260911/r3_law.py`, `r3_chroma.py`, `r3_sim.py`, logs
`f1_law.txt`, `f1_chroma.txt`, `f2_sim.txt`. Instrument self-test 14 of 14
(`r3_selftest.py`, `s0_selftest.txt`), including a planted law recovered to four
decimals and a sheet built NOT as the law leaving 95.1 % unexplained.

**1. Vanilla's far road is a wash that follows the ground under it, not a
paint.** Road luminance regressed on the mean luminance of the NON-road texels
within 8 texels (256 world units) of it, on the same sheet:

| field | slope on the local ground | corr | road L | rise over that ground |
|---|---|---|---|---|
| vanilla, (-20,20) | **+0.714** | +0.442 | 92.02 | +1.02 |
| ours, (-20,20) | +0.339 | +0.300 | 88.30 | +16.85 |
| our own unpainted ground, (-20,20) | +0.637 | +0.562 | 66.06 | -5.39 |
| vanilla, (-8,8) | **+0.755** | +0.472 | 94.17 | +1.91 |
| ours, (-8,8) | +0.209 | +0.088 | 106.09 | +2.29 |
| our own unpainted ground, (-8,8) | +0.565 | +0.338 | 102.13 | -1.67 |

Floors, both sides, on the same texels: the local-ground field translated by a
large random shift reads slope **-0.009** (worst |slope| 0.298) over five draws;
a known-answer road of a FIXED colour built from these sheets reads **+0.000**;
a known-answer road of `ground + 4` reads **+1.000**. Vanilla's road tracks its
neighbourhood at least as strongly as unpainted ground does; ours at half.

**2. The rise, which is the number to design against.** Road mean minus the mean
of the 1..8-texel band outside the mask: vanilla **+4.29** on (-20,20) and
**+4.40** on (-8,8) -- two tiles a whole biome apart -- against ours **+29.96**
and **+3.84**. Ours is right on (-8,8) to 0.56 of a level and 25.67 levels too
contrasty on (-20,20), because our paint is a fixed material colour (99.05 and
106.68) while our ground swings 60.96 -> 102.12 between the tiles.

**3. The hue is NOT a defect and is not a knob.** Road minus surround, opponent
axes `b_y = B - (R+G)/2` and `r_g = R - G`: vanilla +2.30 / -2.14 and ours
+3.35 / -3.25 on (-20,20); vanilla +4.10 / -3.07 and ours +1.96 / -1.24 on
(-8,8). Same sign, same direction, every gap under 3 levels of 255. On the road
texels themselves at Sanctuary, vanilla `b_y` -12.62 against ours -12.39 and
saturation 0.162 against 0.161.

**4. The edge WIDTH is refused as unresolvable, with its number.** Vanilla's
4.29-level rise sits under a local 5x5 SD of 6.59 levels, a signal-to-noise of
**0.65**; a width fitted there reads the terrain's texture, not the road. What
can be read is the profile: vanilla reaches full value at d = +1 and is flat
across the width (92.70, 92.57, 92.54, 92.18 ...), while ours ramps 79.86 ->
86.48 -> 93.45 over three texels, plateaus near 96.5 and climbs to 105.5 in the
core -- a **darker outer band around a brighter core** (the alpha-blended skirt
of 1a.5 over a far darker ground, with the wider trunk material inside it).
Biggest step inside the road, as a max second difference of the profile: ours
**3.88** against vanilla's **1.31** on (-20,20); ours **1.25** against vanilla's
**4.43** on (-8,8), i.e. already the smoother of the two there.

**5. WHY THE DEFAULT IS STILL 1.0.** Every candidate was simulated on the rung's
own sheets, using this section's own composite, before any code was written
(`r3_sim.py`; the tint is inert on these chunks because the cover plane is empty
and `--grade` is 1.0 with its multiply branched over, so the sheet's RGB on a
road texel IS the road plane's). Two refusals came out of it, both arithmetic:

* **On (-8,8) no opacity can match vanilla's road brightness at all.** The
  composite can only land the road between our ground (102.12) and our paint
  (106.68); vanilla's road is at **94.59**, 7.53 levels outside that interval.
  The same holds for the hue read as an absolute rather than as a rise:
  `d(b_y)` is -6.5 to -6.7 for every rule including the default, because our
  GROUND's own `b_y` there is -12.79 against vanilla road's -6.25.
* **On (-20,20) the gates are mutually exclusive by 22 levels.** Vanilla's
  absolute level wants a = 0.83, vanilla's rise wants a = 0.326, the step wants
  a <= 0.25 and a local SD inside 20 % of vanilla's wants a >= ~0.75. Those 22
  levels are the GROUND's: ours is 19 levels darker than vanilla's on that
  chunk (68.69 against 83.52), which 2.5f records as a per-cell CONTENT
  difference with a near-zero mean, and lane TILING2's addendum says in writing
  not to chase with the road pass.

The priced table, simulated, both tiles (road L, and rise over the surround;
vanilla is 92.52 / +4.29 and 94.59 / +4.40):

| `--road-opacity` | (-20,20) | (-8,8) |
|---|---|---|
| **1.000 (default)** | 99.05, +29.96 | 106.68, +3.84 |
| 0.830 | 92.57, +23.48 | 105.91, +3.07 |
| 0.500 | 80.01, +10.91 | 104.40, +1.56 |
| 0.326 | 73.38, **+4.28** | 103.61, +0.77 |
| 0.250 | 70.48, +1.39 | 103.26, +0.42 |

**6. A per-texel ground-relative mode was built, simulated and REJECTED with its
numbers**, and is recorded so it is not proposed again without new evidence:
choosing `a` per texel so the result sits a fixed rise above the local non-road
ground lands the (-20,20) road 25 levels below vanilla with a rise of **-1.8**
(more than half its texels clamp to a = 0, because the ground directly under the
road is brighter than the disc mean for most of them), and halves the (-8,8)
local SD to 0.48 of vanilla's. A flat opacity beats it on every row of both
tables.

**7. `--road-detail` was re-tested and stays 0.** The residual after the best
wash correlates with the full-detail bake's departure from its flat average at
**+0.0275** against a phase-twin floor of 0.0270 mean / 0.0644 max on (-20,20),
and **+0.0162** against 0.0124 / 0.0163 on (-8,8) -- the correlation IS the
floor at every blur radius, and the best-fit strength is negative. 1a.5c's
conclusion survives a test that could have overturned it.

**What is NOT yet measured on a built exe.** As of this amendment the change is
written and uncompiled: `Fallout4.exe` was running when the build would have
been spent, so the lane ended BUILD PENDING. The byte-identity claim in the
first paragraph is a claim about the CODE (the multiply is branched over), not
yet a `cmp` result, and it is owed -- see `scratchpad/roads3_20260911/PENDING.md`.

"""
sub(ANCHOR, NEW_SECTION + ANCHOR)

# ------------------------------------------------------------------- 1a.7
OLD = ("ROADS2 also `roadComposite` (`max-z` or `blend`), `roadDetail`,")
NEW = ("ROADS2 also `roadComposite` (`max-z` or `blend`), `roadDetail`, from\n"
       "lane ROADS3 `roadOpacity`,")
sub(OLD, NEW)

# ------------------------------------------------------------------- 1a.8
OLD = """**`--roads-legacy`** is the way back to the road pass as it was before lane
ROADS2, in one token: it means `--road-composite max-z`, `--road-detail 1`,
`--road-raised` and `--road-sidewalks` together."""
NEW = """**`--roads-legacy`** is the way back to the road pass as it was before lane
ROADS2, in one token: it means `--road-composite max-z`, `--road-detail 1`,
`--road-raised` and `--road-sidewalks` together, and from lane ROADS3 also
`--road-opacity 1` -- a no-op while 1 is the default, written down so the way
back stays the way back if the default is ever moved."""
sub(OLD, NEW)

io.open(P, 'w', encoding='utf-8', newline='').write(s)
print('contract amended: 1a.4, 1a.5d (new), 1a.7, 1a.8')
