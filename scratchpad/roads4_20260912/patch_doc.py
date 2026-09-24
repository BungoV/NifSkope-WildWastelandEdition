"""Amend docs/LODGEN_TERRAIN_VT.md for lane ROADS4.

Two edits:
  1. 1a.5's colour bullet says `--road-detail 0` is the default. It is 1.0 now.
  2. A new 1a.5e, the ground-material shapes inside the road models and the
     `--road-ground-paint` question, inserted before 1a.6.

The file is LF-only (CR 0); the script writes with newline='\n' and prints the
CR count so a regression is visible.
"""
import io
import sys

P = 'docs/LODGEN_TERRAIN_VT.md'
s = io.open(P, encoding='utf-8').read()

OLD1 = (
    "* **colour** = the shape's diffuse sampled at the interpolated UV, multiplied by\n"
    "  the interpolated vertex colour, then **lerped toward that texture's own\n"
    "  average by `1 - roadDetail`**; `--road-detail 0` is the default, so what lands\n"
    "  in the sheet is one flat colour a material. Why, and what it fixed, is 1a.5c.\n")
NEW1 = (
    "* **colour** = the shape's diffuse sampled at the interpolated UV, multiplied by\n"
    "  the interpolated vertex colour, then **lerped toward that texture's own\n"
    "  average by `1 - roadDetail`**. **`--road-detail` defaults to 1.0 since\n"
    "  2026-09-12** (lane ROADS4), which is the sampled texel itself with no flatten:\n"
    "  bungo ruled on the picture, *\"--road-detail 1 is always on, do not ever use\n"
    "  road detail 0, that looks terrible\"*. It was 0 until then, and `--road-detail\n"
    "  0` still reproduces those bakes byte for byte, so 1a.5c below describes what\n"
    "  that switch does and why it was once the default, not what happens now.\n")
if s.count(OLD1) != 1:
    sys.exit('anchor 1: %d' % s.count(OLD1))
s = s.replace(OLD1, NEW1)

ANCHOR2 = "### 1a.6 The channels touched\n"
NEW2 = """### 1a.5e The road models carry TERRAIN, and it is a third of the road plane (lane ROADS4, 2026-09-12)

bungo, 2026-09-12: *"the issue with the roads is, these meshes have some terrain
included there, you can see the sharp mesh terrain being included into the
chunk's bake"*. He is right, and it is not a skirt, not a shading term and not
anything the composite above does wrong. It is what Bethesda modelled.

Fallout 4's road pieces -- `Landscape\\Roads\\Sanctuary\\SancRoadStr01.nif` and
its siblings -- contain shapes whose MATERIAL lives under
`materials\\Landscape\\Ground\\`: `CommonwealthDefault01.bgsm`,
`DirtGravel01.bgsm`. They are the verge and the junction fill, geometry that
carries landscape colour inside a road model. The road pass paints them, because
they are shapes in a road NIF, and so the far sheet gets a hard-edged patch of
ground colour sitting in the middle of the road plane, at road detail:

| chunk | road texels | won by a `Landscape/Ground/` material | share |
|---|---|---|---|
| (-20,20) Sanctuary | 23,116 | 8,337 | **36.1%** |
| (-8,8) | 11,069 | 2,756 | **24.9%** |

| what | ours | vanilla | our floor |
|---|---|---|---|
| two-tone step, surface minus patch, (-20,20) | **25.28** | 2.53 | -- |
| two-tone step, (-8,8) | **13.13** | 1.76 | -- |
| gradient across the patch boundary, (-20,20) | **15.387** | 5.362 | 6.077 |
| gradient across the patch boundary, (-8,8) | **8.010** | 5.138 | 4.805 |

The floor is the same boundary texel set displaced five ways on the same sheet,
per `ww-control-calibration`; vanilla sits within a level of its own floor on
both tiles and ours sits two and a half times above it on (-20,20).

**The discriminator is the material's FOLDER, never its file name.** A name-stem
list put the two biggest contributors (`CommonwealthDefault01`,
`SancSW01.BGSM`) in an unclassed bucket and hid the whole finding. The rule is
`lodgenRoadMaterialIsGround()`: normalise with `lodgenRoadMaterialPath()` (which
keys on the LAST `materials/`, see 1a.5) and ask whether the result contains
`materials/landscape/ground/`.

**`--road-ground-paint 0..1`** is the coverage multiplier for such a shape. It
**defaults to 1.0**, which is the behaviour above unchanged, and it ships as the
instrument that refuted its own candidate rather than as a fix:

| `--road-ground-paint` | 1.0 | 0.75 | 0.5 | 0.25 | 0 |
|---|---|---|---|---|---|
| boundary gradient, (-20,20) | 15.387 | 19.243 | 23.810 | 28.598 | **33.352** |
| the patch, levels from vanilla | -5.47 | -11.61 | -17.94 | -24.50 | **-29.22** |
| R5 road-presence metric | 0.3078 | 0.2935 | 0.2854 | 0.2833 | **0.2783** |

Fading the terrain shapes out makes the seam **monotonically worse**, because
the patch darkens toward our own ground (85.53 -> 61.78) while the asphalt
beside it does not move at all (110.82 -> 111.49): the step it makes with the
road surface more than doubles. The multiply is on **coverage**, not on paint
strength, so 0 also stops such a shape suppressing ground cover -- which is the
behaviour a consumer expects from a shape that is not painting.

**What the numbers actually indict is the asphalt's own tone.** Our road surface
sits at luminance 110.82 where vanilla's is 93.54, while our terrain class is
already within 5.47 levels of vanilla's. That is `roadOpacity`, which lane
ROADS3 measured and refused to set, and this lane's seam number is new evidence
on the same knob and reaches the same refusal: `--road-opacity 0.326` gives
vanilla's own seam on BOTH tiles (5.304 vs 5.362; 4.242 vs 5.138) and destroys
the road-presence metric (R5 0.1576, both bars fail, centreline colour error
23.64 -> 30.87); `--road-opacity 0.83` is the only value that passes both R5
bars (0.3545 >= 0.3271) while improving the seam to 12.630; and (-8,8) prefers
the opposite direction to (-20,20). **Unset, deliberately.**

**Two hypotheses are closed by measurement and should not be re-opened without
new evidence.** There is no skirt to suppress: skirt-only texels are **0** on
both chunks, a skirt triangle is the max-z winner on 455 of 23,116 and 556 of
11,069 texels and never alone. And vertex alpha carries no signal: luminance
against vertex alpha reads **-0.0345** on ours and **-0.0359** on vanilla on
(-20,20), **+0.0404** and **+0.0766** on (-8,8). The **-0.792** in an earlier
lane's note was an instrument artefact -- a `np.zeros` alpha buffer that read
every shape without a vertex-alpha channel as fully transparent.

The census reports it: `ground_shapes` and `ground_texels` on the census line,
`roadGroundPaint` / `roadGroundShapes` / `roadGroundTexels` in the meta report.

"""
if s.count(ANCHOR2) != 1:
    sys.exit('anchor 2: %d' % s.count(ANCHOR2))
s = s.replace(ANCHOR2, NEW2 + ANCHOR2)

io.open(P, 'w', encoding='utf-8', newline='\n').write(s)
b = io.open(P, 'rb').read()
print('ok  CR %d  LF %d  bytes %d' % (b.count(b'\r'), b.count(b'\n'), len(b)))
