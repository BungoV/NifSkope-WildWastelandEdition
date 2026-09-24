#!/usr/bin/env python
"""The two documentation edits: a `## Lighting` note in the native LOD spec's
Viewer section, and the lighting facts added to the render-shot skill's
"Photographing a BUILT document".  Both files are LF-only; both anchors are
taken from the file's own bytes.
"""
import io
import sys

ROOT = "E:/Projects/NifskopeWildWastelandEdition"
DOC = ROOT + "/docs/LODGEN_NATIVE_LODO_LODI.md"
SKILL = ROOT + "/.claude/skills/nifskope-ww-render-shot/SKILL.md"

DOC_ANCHOR = "  * Gate: `tests/spells/native_open.sh`.\n"

DOC_ADD = """
## Lighting -- the terrain sheets are MODEL-space normal maps

Measured 2026-09-16, lane NATIVEVIEW2, on Bethesda's own shipped sheets and on
this tree's renderer.

* **A terrain LOD normal sheet is an `_msn`: a MODEL-space normal map.** It
  carries all three components in the model's own axes, and it says so in the
  shape: `lodgen` sets Shader Flags 1 bit 12 (`SLSF1_Model_Space_Normals`,
  `LAND_SHADER_FLAGS1 = 0x80401000`), and `src/btdterrain.cpp` sets the same bit
  on every sheet-lit `.lodl` tile it builds.
* **The channel order, measured, not assumed:** `R = EAST (+x)`,
  `G = UP (+z)`, `B = NORTH (+y)`, range 0..255 mapping to -1..+1 with no sign
  flip, alpha a constant 255 that carries nothing, and texel row 0 = NORTH. Six
  vanilla `Commonwealth.16.*_msn.DDS` tiles were decoded and correlated against
  the heights of the same cells: R against `-dh/dx` 0.364 and against `-dh/dy`
  0.001; B against `-dh/dy` 0.422 and against `-dh/dx` 0.002; G's mean 238.6 of
  255. The same statistic with the sheet's rows NOT flipped collapses to
  0.060 / -0.063, which is the refuter for the row order. Two of the six tiles
  are flat and read 0.000 everywhere -- a constant sheet has no variance to
  correlate, so it is not evidence either way. This matches our writer
  (`lodgenTerrainMsnPixel`) and `res/shaders/sk_msn.frag`'s `.rbg` swizzle.
* **The viewer has a model-space path for it, and takes it only on bit 12.**
  `res/shaders/fo4_default.frag` transforms the texel by `normalMatrix` --
  model to view -- and by nothing else. There is no tangent frame in that path
  on purpose: a `.lodl` tile's tangent frame is arbitrary
  (`src/btdterrain.cpp`, `T = n x worldUp`, `B = n x T`), so reading the sheet
  as tangent-space sends its "up" along that arbitrary bitangent. That is what
  the dark blotches on the native terrain were. `src/gl/renderer.cpp`
  (`setupProgramCE1`) sets `hasModelSpaceNormals` from the shape's own bit 12,
  gated on the same test that decides whether a real normal map was bound, so
  with lighting or normal maps switched off the branch switches off too --
  otherwise `default_n`, a flat TANGENT-space texel, would decode as "north".
* **The legacy `.BTR` does NOT use that path, and this was measured.** Its
  `Land` shape is Shader Type 18, which `res/shaders/fo4_default.prog` excludes
  by condition, and the program scan hands it to `res/shaders/sk_msn.prog` --
  a model-space path already, the Skyrim one. The census that says so is
  `WW_PROGRAM_CENSUS=<absolute path>`, which writes one row per first-sighted
  (shape, program) pair plus the view-space light direction; on chunk (-20,24)
  it reads `shape="Land" bsver=130 msn=1 lodland=1 prog=sk_msn.prog`. Whether
  the two model-space paths agree with each other on brightness is NOT
  measured here and is open.
* Gate: `tests/spells/native_lighting.sh` (14 checks).
"""

SKILL_ANCHOR = "## Photographing a BUILT document"

SKILL_ADD = """
### The lighting a built LOD document is photographed under (lane NATIVEVIEW2, 2026-09-16)

A render-shot picture of terrain LOD is a picture of a LIGHT as much as of a
mesh, and three facts about that light decide what the picture can prove:

* **The default light is a HEADLIGHT.** `frontalLight` is true by default
  (`src/glview.h`), so `globalUniforms.lightSourcePosition[0]` is
  `(0, 0, 1)` in VIEW space -- the light points down the view axis and MOVES
  WITH THE CAMERA. Two views of the same shape are therefore two different
  lighting conditions, and the brightness difference between a top view and an
  oblique is not a defect. The direction in world axes is the bottom row of
  `Matrix::fromEuler( Rot )`: for `WW_RENDER_VIEW=1` (Top, rotation 0,0,0) it is
  `(0, 0, 1)`; for `WW_RENDER_VIEW=8` (ViewUser, the Blender startup rotation
  `-63.5593, 0, 133.3081`) it is `(-0.6516, +0.6142, +0.4453)`.
* **`diffuse = A + D * max(N.L, eps)`** in `res/shaders/fo4_default.frag`, with
  `A = sqrt(ambient) * 0.375` and `D = sqrt(diffuse)` from the vertex stage, and
  a tone map after it. So a frame's mean luma is NOT proportional to `N.L`: a
  prediction of the form "half the `N.L`, half the luma" will be wrong. What
  survives the tone map is ORDER and EQUALITY -- two normals with the same `N.L`
  must give the same picture, and a larger `N.L` must not be darker. Build a
  known-answer gate out of those, not out of a ratio.
* **Terrain LOD is lit from a MODEL-space normal sheet** when Shader Flags 1
  bit 12 is set (`docs/LODGEN_NATIVE_LODO_LODI.md`, "Lighting"). A picture that
  is meant to show terrain SHAPE must therefore be shot with the real `_msn`
  sheets present: `WW_LODL_SHEET_CACHE` pointed at a cache whose `.n.DDS` tiles
  are flat is a perfectly valid picture of nothing, and it is the control arm,
  not the subject. The legacy `.BTR` is a DIFFERENT program (`sk_msn.prog`,
  measured with `WW_PROGRAM_CENSUS`), so it is a control, never a like-for-like
  comparison.
* **`WW_PROGRAM_CENSUS=<ABSOLUTE path>`** writes which program lit which shape
  for the frame just taken -- `shape=".." bsver=.. msn=.. lodland=.. prog=..`,
  one row per first sighting, with the view-space light on the header line. Take
  it with every lighting picture: "the terrain looks wrong" and "the terrain is
  on the program I think it is" are different claims, and only one of them is
  cheap to check.
"""


def splice(path, anchor, add, where):
    b = open(path, "rb").read()
    crlf = b.count(b"\r\n")
    s = b.decode("utf-8")
    n = s.count(anchor)
    assert n == 1, "%s: anchor %d times" % (path, n)
    if where == "after":
        s = s.replace(anchor, anchor + add)
    else:
        s = s.replace(anchor, add + anchor)
    out = s.encode("utf-8")
    assert out.count(b"\r\n") == crlf, "CR count moved"
    return out


def main():
    check = "--check" in sys.argv
    d = splice(DOC, DOC_ANCHOR, DOC_ADD, "after")

    b = open(SKILL, "rb").read().decode("utf-8")
    i = b.index(SKILL_ANCHOR)
    j = b.find("\n## ", i + 1)
    assert j > 0, "no section after 'Photographing a BUILT document'"
    s2 = (b[:j] + "\n" + SKILL_ADD + b[j:]).encode("utf-8")
    print("doc  %d -> %d B" % (len(open(DOC, "rb").read()), len(d)))
    print("skill %d -> %d B (inserted at the end of its section, offset %d)"
          % (len(open(SKILL, "rb").read()), len(s2), j))
    if check:
        return 0
    open(DOC, "wb").write(d)
    open(SKILL, "wb").write(s2)
    for p in (DOC, SKILL):
        x = open(p, "rb").read()
        print("%-70s %7d B  CRLF %d  LF %d" % (p, len(x), x.count(b"\r\n"),
                                               x.count(b"\n") - x.count(b"\r\n")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
