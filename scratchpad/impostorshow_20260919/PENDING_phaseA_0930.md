# BUILD PENDING

Lane IMPOSTORSHOW, 2026-09-19 09:3x. Phase A is finished; phase B needs the
build slot, which lane GLTFEXPORT1 holds.

Nothing below has been built or run in the application. Everything below has
been syntax-gated against the real Qt 6 headers and the real defines
(`scratchpad/impostorshow_20260919/syn.sh`), and the offline half of the gate
is green and refuted.

---

## 0. The state of play, in one paragraph

**No draw path for octahedral impostors exists in this tree today.** The only
impostor geometry the viewer has ever drawn is `lodgenCardShape()` at
**`src/lodgen.cpp:3047`** -- two crossed flat quads reading `<id>_fs.DDS`, the
front view on the left half of the sheet and the side view on the right, with
lighting baked in. That is the stock-engine fallback, not the octahedral set.
The octahedral sheets are **baked** (`src/nifskope_ui.cpp:22680..23249`) and
**saved as PNGs** (`:23243`), and then nothing reads them: `cardLayer`,
`FORCE_CARD` and `cardCount` never reach `src/lodinative.cpp`, whose instance
walk at `:729..:734` builds BSTriShapes from mesh ranges, so a card-drawn
instance with an empty range draws nothing at all. bungo's words are exactly
right: he has not seen them.

---

## 1. THE CLAUSE TABLE

Every normative sentence of `docs/LODGEN_IMPOSTOR_SPEC.md` **about drawing**.
Four verdicts:

- **IMPLEMENTED** -- with the file and line that does it.
- **NOT IMPLEMENTED** -- with what it would take.
- **SPEC GAP** -- the spec is silent or contradicts itself. My proposed wording
  is below the table. Where I implemented the standard hemi-octahedral method
  to fill a gap **it is marked as mine, never passed off as spec.**
- **WAITS FOR THE PBR RENDERER** -- geometric work is done, shading is not, per
  bungo 2026-09-19: *"we can't display the impostors fully though with all the
  textures, not until we recreate lighting in nifskope from [Fallout 4] and
  fo4cs pbrm"*.

| # | Spec | The clause | Verdict | Where |
|---|------|-----------|---------|-------|
| C1 | 42-44 | `_d`/`_bc` BC3: RGB colour, A coverage | IMPLEMENTED | `impostor_oct.frag` ColourSheet; `impostorcard.cpp` colour |
| C2 | 45 | `_n` BC3: R normal X, G normal Y, B height, A sway | IMPLEMENTED | `impostor_oct.frag:118..` + the blend loop |
| C3 | 46-47 | `_gsaos`/`_rmaos` BC3: gloss/rough, spec/metal, AO, subsurface | **WAITS FOR THE PBR RENDERER** (AO alone is used) | bound + debug ch. 6,7,8,9; seam `IMPOSTOR_MATERIAL_SEAM` |
| C4 | 48 | `_g`/`_e` BC1: emissive colour | **WAITS FOR THE PBR RENDERER** | bound + debug ch. 10 |
| C5 | 49 | normal Z rebuilt as sqrt(1 - x^2 - y^2) | IMPLEMENTED | `impostoroct.cpp:269`, `impostor_oct.frag:118` |
| C6 | 49-50 | coverage is a FRACTION, alpha-tested | IMPLEMENTED | `impostor_oct.frag:199`, `:233` |
| C7 | 51-52 | everything linear but the colour sheet (sRGB) | NOT IMPLEMENTED | the sRGB decode is the viewer's `GL_FRAMEBUFFER_SRGB` path; needs the build to verify which end does it |
| C8 | 88-95 | `emissiveScale` is a MULTIPLE a consumer scales by, never folded in | IMPLEMENTED (carried, not shaded) | `impostorcard.cpp` emissiveScale, `impostordraw.cpp` uniform |
| C9 | 106-109 | legacy gloss/specular are what the engine composes | **WAITS FOR THE PBR RENDERER** | -- |
| C10 | 135 | the `card` block: oct, frame, half, center, depthSpan, mips | IMPLEMENTED | `impostorcard.cpp` card branch |
| C11 | 187-191 | sheets named as GAME paths | IMPLEMENTED, with a loose-file escape | `impostorcard.cpp` fillSheet; `impostordraw.cpp` registerLooseSheets |
| C12 | 192-193 | a consumer reading `C` opens the `.lodm` and draws the sheets | NOT IMPLEMENTED (phase B) | `impostorParseCLine` + `impostorReadManifest` exist; the chunk placement does not |
| C13 | 218-221 | one bake serves every ring; ONE QUAD PER TREE | IMPLEMENTED | `impostordraw.cpp` -- four vertices, six indices, one draw |
| C14 | 225 | frame (i,j) at pixel (i*frameW, j*frameH) of an N x N sheet | IMPLEMENTED | `impostoroct.h:158`, `frameRect` |
| C15 | 226-231 | the hemi-octahedral forward mapping, frames on the VERTICES | IMPLEMENTED | `impostoroct.cpp:112` |
| C16 | 232 | "the centre frame the exact top" | **SPEC GAP #3** -- true for odd N only | checked as a selftest property, `impostor_oct_ref.py` |
| C17 | 233-238 | gutters and dilation are INSIDE the frame | IMPLEMENTED (not subtracted) | `impostoroct.h:162..166` |
| C18 | 239 | halfW/halfH span the full frame -- the quad IS the frame | IMPLEMENTED | `impostor_oct.vert:41..43` |
| C19 | 243 | mips stop at 8 texels a side; a consumer obeys the cap | NOT IMPLEMENTED | `cardMipCap` is carried to the shader and not yet used to clamp the LOD |
| C20 | 252-261 | frame size classes; a card quad is its frame, undistorted | IMPLEMENTED | `impostor_oct.vert` |
| C21 | 276-281 | colour is UNLIT | IMPLEMENTED | `impostor_oct.frag` lights it with the viewer's own sun, nothing baked in |
| C22 | 284-286 | height = the bake window's depth; 0.5 = the card plane; units = (v-0.5) x depthSpan | IMPLEMENTED | `impostoroct.h:180`, `impostor_oct.frag` blend + `gl_FragDepth` |
| C23 | 286 | height drives the ghost-free frame blend AND the pixel depth offset | IMPLEMENTED | `impostor_oct.frag` reprojection loop; `useHeightBlend`, `useDepthOffset` |
| C24 | 282 | the normal is "the geometric normal in the VIEW's space" | IMPLEMENTED, with **SPEC GAP #6** on the blend | `impostoroct.cpp:288`, `impostor_oct.frag:118` |
| C25 | 293-295 | AO baked per pixel | IMPLEMENTED -- into the AMBIENT term only | `impostor_oct.frag:324`, `useBakedAo` |
| C26 | 296-302 | sway weight per pixel | PARTIAL, and named so: a UV shear, not a displacement (a card is four vertices). Default 0. | `impostor_oct.frag:92`, `swayAmplitude` |
| C27 | 303-310 | subsurface mask, emissive per pixel | **WAITS FOR THE PBR RENDERER** | debug ch. 9, 10 |
| C28 | 311-321 | coverage cut-out; 0.5 for full crowns, lower/blend for bare trees | IMPLEMENTED, threshold exposed | `impostor_oct.frag:233`; **SPEC GAP #7** on who picks it |
| C29 | 329-333 | the manifest `C` line | IMPLEMENTED (parsed), NOT placed | `impostorcard.cpp` impostorParseCLine |
| C30 | 335-389 | card sheet arrays, one layer per tree | IMPLEMENTED (loaded) | `impostorcard.cpp` cardArray branch |
| C31 | 484-492 | `A <block> -1 <lodm>` | NOT IMPLEMENTED (phase B, chunk path) | -- |
| C32 | 617-630 | half-resolution aux sheets, `card.auxDiv` | IMPLEMENTED (read); the shader does not yet divide the aux lookup | `impostorcard.cpp` auxDiv |

**COUNTS: implemented 20 · not implemented 5 · spec gap 1 (plus 7 gaps noted
against implemented clauses) · waits for the PBR renderer 5.**
(C16 is the only clause whose sole verdict is SPEC GAP; gaps #1, #2, #4, #5,
#6, #7 sit underneath clauses that are otherwise implemented, and #8 is the
bake defect below.)

### The spec gaps, with proposed wording for the director

1. **The inverse mapping is never given.** The spec gives `u,v -> x,y,z` and
   stops, so every consumer must invert it and they can disagree. *Proposed:*
   "A consumer maps a direction back by L = |x| + |y| + z; x /= L; y /= L;
   u = x + y; v = x - y; i = (u+1)/2 x (N-1); j = (v+1)/2 x (N-1)."
2. **Which triangle of a cell, and the weights.** Line 231 says every direction
   falls inside a triangle and does not say which one or how it is weighted.
   *Proposed:* "The cell (i,j)..(i+1,j+1) is split on the diagonal
   (i+1,j)-(i,j+1). With a,b the fractional position in the cell, a+b <= 1
   takes (i,j),(i+1,j),(i,j+1) with weights 1-a-b, a, b; otherwise
   (i+1,j+1),(i,j+1),(i+1,j) with a+b-1, 1-a, 1-b."
3. **"The centre frame the exact top" is false for even N.** At N=4 and N=8 --
   the two grids the brief asks for pictures of -- there is no centre frame and
   the top falls inside a triangle. *Proposed:* "For odd N the centre frame is
   the exact top; for even N the top direction falls inside a cell and is
   blended from three frames."
4. **The height channel's SIGN.** The magnitude is given, the direction is not,
   and a consumer that guesses wrong gets an impostor turned inside out that
   still looks like a tree. *Proposed:* "...units = (value - 0.5) x depthspan,
   measured ALONG the frame's view direction, away from the camera."
5. **A frame's picture plane is never oriented.** Two cameras looking the same
   way can differ by a roll, and the sheet does not say which. *Proposed:*
   "A frame's up axis is world +Z projected into the picture plane; at the pole
   it is world +X. Its right axis is up x forward."
6. **Blending normals from three frames.** Spec 282 puts the normal in the
   VIEW's space; three frames are three different view spaces, and averaging
   them directly is wrong. *Proposed:* "A consumer rotates each frame's normal
   into a common space using that frame's own basis before weighting."
7. **Who chooses the alpha threshold.** 0.5 for full crowns and "lower for bare
   trees" is guidance without an owner. *Proposed:* "The threshold is the
   consumer's, defaulting to 0.5; a bake may recommend one in the `.lodm`."
8. **NOT A GAP -- A DEFECT. RULED AND REPAIRED (bungo, 2026-09-19: "Okay,
   fix the 180 issue").**
   The bake's own camera angles put the camera at `(-d.x, -d.y, +d.z)` of the
   frame's spec direction: the elevation was right and **the azimuth was
   turned by 180 degrees**, so frame (i,j) held the view from the opposite
   side of the object. It survived because only near-axially-symmetric trees
   had ever been baked.

   Proved before it was touched, not taken from the one-liner: with
   `Matrix::fromEuler(rx, 0, rz)` the camera SITS at
   `row2 = (sinX sinZ, sinX cosZ, cosX)`; `rx = -90 + elev` already gives
   `cosX = sin(elev)` and `sinX = -cos(elev)`; wanting `row2 = d` therefore
   requires `sinZ = -cos(azim)` and `cosZ = -sin(azim)`, which `Z = 270 - azim`
   satisfies exactly, while `Z = 90 - azim` yields `(-d.x, -d.y, +d.z)`.

   **The repair, applied:** `src/nifskope_ui.cpp` viewDir, `rz = 90.0f - azim`
   -> `rz = 270.0f - azim`, with the derivation in the comment. No toggle and
   no way-back switch for the bake. The preview's default convention is now
   `SpecLiteral`; `AsBaked` survives ONLY as a diagnostic for opening a set
   baked before the repair, and it is selected by the SET's own token, not by
   a preference.

   **The token:** the sidecar's `oct` line gains a thirteenth token after
   `base` (`spec1`), carried to `card.conv` in a `.lodm` and to the layer's
   `conv` in a `cardArray`. Absent = the pre-repair vintage.
   `docs/LODGEN_IMPOSTOR_SPEC.md` now states the camera formula, the token and
   the re-bake requirement.

   **EVERY IMPOSTOR SET BAKED BEFORE THIS EXE MUST BE RE-BAKED.**

---

## 2. Files

### Written by this lane (new, nobody else's)

    src/impostoroct.h              the mapping, Qt-free and GL-free on purpose
    src/impostoroct.cpp            + the camera-convention derivation
    src/impostorcard.h             .lodm -> drawable set, and the `C` line
    src/impostorcard.cpp
    src/gl/impostordraw.h          the pass, modelled on Renderer::drawSkyBox
    src/gl/impostordraw.cpp
    src/impostorpreviewtest.h      WW_IMPOSTOR_PREVIEW
    src/impostorpreviewtest.cpp
    res/shaders/impostor_oct.vert  the card quad
    res/shaders/impostor_oct.frag  the drawing contract + the one seam
    res/shaders/impostor_oct.prog
    tests/spells/impostor_draw.sh  the gate
    tests/spells/impostor_oct_ref.py     the independent Python reference
    tests/spells/impostor_oct_oracle.cpp the standalone table printer
    scratchpad/impostorshow_20260919/syn.sh     the syntax gate
    scratchpad/impostorshow_20260919/hookup.py  the anchored hook-up

### Shared files touched -- EVERY ONE, and the whole of each hunk

All four were applied by `hookup.py`, which refuses unless its anchor occurs
exactly once and is a no-op on a second run. Nothing was reverted, reformatted
or tidied.

1. **`NifSkope.pro`** +97 B after `\tsrc/gl/renderer.h \` -- four header lines.
2. **`NifSkope.pro`** +105 B after `\tsrc/gl/renderer.cpp \` -- four source lines.
3. **`src/glview.cpp`** +34 B after `#include "gl/renderer.h"` -- one include.
4. **`src/glview.cpp`** +237 B at the `scene->draw()` block in `paintGL`
   (was line 3922): `wwImpostorPreviewDraw( scene );` before it and the scene
   draw wrapped in `if ( !wwImpostorPreviewSuppressScene() )`.
5. **`src/nifskope_ui.cpp`** +33 B after `#include "glview.h"` -- one include.
6. **`src/nifskope_ui.cpp`** +487 B before the `WW_RENDER_SHOT` block in
   `NifSkope::createWindow` -- the comment and
   `wwImpostorPreviewStart( skope, skope->ogl, skope->viewportHeader );`.

### Shared files touched by THE AZIMUTH REPAIR (2026-09-19, bungo's ruling)

Hand edits, smallest hunks, each one listed:

7.  **`src/nifskope_ui.cpp`**, the bake's `viewDir` lambda: `rz = 90.0f - azim`
    -> `rz = 270.0f - azim`, plus the derivation as a comment above it.
8.  **`src/nifskope_ui.cpp`**, the sidecar's `oct` line: ` << " spec1"` appended
    after the `base` token, plus the comment saying why it goes last.
9.  **`src/lodgen.cpp`**, the sidecar reader's `oct` branch: `card.octConv` read
    from token 12 when present; a new `QString octConv` field on the local card
    struct with its doc comment; `oc.insert( "conv", ... )` in the card `.lodm`
    writer, guarded on non-empty.
10. **`src/lodgen.cpp`**, the card-ARRAY builder: `conv` added to the local
    `Layer` struct, read from each source `.lodm`'s `card.conv`, written per
    layer. Per layer, not per array, because a library part-way through a
    re-bake holds both vintages in one size class.
11. **`docs/LODGEN_IMPOSTOR_SPEC.md`**, after the frame-mapping paragraph: the
    camera formula with its derivation, the 180-degree defect, the convention
    token and **the re-bake requirement**.
12. **`tests/spells/lodgen_octahedral.sh`**: two `oct`-line greps RE-BASED
    (`legacy 64$` -> `legacy 64 spec1$`, `pbr 64$` -> `pbr 64 spec1$`), and two
    NEW checks that the token reaches the meta's token 12 and the `.lodm`'s
    `card.conv`.
13. **`tests/spells/lodgen_card_arrays.sh`**: the synthetic red set's `oct` line
    gains `<base> spec1` (base = max(FW,FH), the value the reader already
    derived, so nothing but the token moves); the blue set keeps NO token, and
    two new checks assert the key travels per layer and is not invented.

Everything else in `src/lodgen.cpp`, all of `src/lodinative.cpp`, the channel
table and every file of lanes HORIZONOUT and GLTFEXPORT1 are **untouched**.

---

## 3. What is proven today, and what is not

**Proven (offline, no build):**

    IMPOSTOR_OFFLINE_ONLY=1 sh tests/spells/impostor_draw.sh
    -> done  10 steps, 0 failures

    sh scratchpad/impostorshow_20260919/syn.sh
    -> OK src/impostoroct.cpp, src/impostorcard.cpp,
          src/gl/impostordraw.cpp, src/impostorpreviewtest.cpp
    sh scratchpad/impostorshow_20260919/syn.sh src/glview.cpp src/nifskope_ui.cpp
    -> OK both, after the hunks above

**Refuted** (the gate bites): changing `a + b <= 1.0f` to `a + b <= 2.0f` in
`ImpostorOct::pickFrames` produced 32 failures at N=8.

**NOT proven, and not to be quoted as if it were:** nothing has been linked,
nothing has been drawn, no card has appeared on screen. The GLSL has never seen
a compiler. Two specific assumptions are flagged in the source and must be
checked first in phase B:

- `GLView::WwCameraPin` with `haveView` false is assumed not to re-assert the
  rotation on later paints (`impostorpreviewtest.cpp`, `aimCamera`). If it
  does, all sixteen views photograph one direction -- which the `map` mode's
  own output makes obvious immediately.
- Sheet row order. `ImpostorOct::frameRect` puts row j downward in sheet space
  and the fragment stage flips within the frame. A sign error here draws a
  convincing card of the WRONG frame, which is why both the frame index (vs the
  Python reference) and the silhouette (vs the mesh) are checked.

---

## 4. PHASE B -- the exact commands, in order

**Before anything:** `tasklist | grep -i -E "Fallout4|NifSkope"` as its OWN
command. Fallout4 up = no build, no exe run, stop here.

### 4.1 Build

    cd /e/Projects/NifskopeWildWastelandEdition
    make -j8 -f Makefile.Release 2>&1 | tail -40
    ls -l release/NifSkope.exe && sha1sum release/NifSkope.exe

Confirm the three shader files reached the build:

    ls -l release/shaders/impostor_oct.vert release/shaders/impostor_oct.frag release/shaders/impostor_oct.prog

(The `.pro` copies `res/shaders` wholesale, line 741 `copyDirs( $$SHADERS, shaders )`.)

### 4.2 Bake a fixture (the maple, N=4 then N=8)

    sh tests/spells/lodgen_octahedral.sh            # the existing bake gate
    # note the <id>_oct.lodm it leaves and use it below

### 4.2b RE-BAKE. Nothing baked before this exe may be measured.

The azimuth repair moved the bake, so every `.lodm` on disk without a
`card.conv` of `spec1` holds frames photographed 180 degrees the wrong way.
A preview will still open one, say so in its notes and draw it under
`AsBaked`, but no IoU, picture or number in this lane may come off one.
Check before measuring:

    python -c "import json,sys;b=open(sys.argv[1],'rb').read();print(json.loads(b[12:]).get('card',{}).get('conv'))" <the _oct.lodm>
    # spec1 = re-baked;  None = legacy, re-bake it

### 4.3 The gate

    export IMPOSTOR_LODM=<the absolute E:/... path to that _oct.lodm>
    export IMPOSTOR_SIZE=1024x1024
    export IMPOSTOR_PORT=27713
    sh tests/spells/impostor_draw.sh <the baked object's own .nif>

Read `release/ww_impostor_map.log` FIRST and check the `viewport` line says
1024x1024. If it does not, the window is still inheriting a maximized
geometry and every number after it is the machine's, not the code's.

Then set the IoU floor **from the measured distribution**, once, with the
number written beside it -- it is pre-registered at 0.80 and that number is a
guess that is owed a measurement. The red control must show
honest - shuffled >= 0.15 or the metric is not measuring frame selection.

Steps 7 and 8 are the azimuth repair's own rows and they need an **asymmetric
subject**: step 7 asserts the card matches the mesh from the frame's SPEC
direction better than from the opposite one, step 8 forces
`WW_IMPOSTOR_CONVENTION=asbaked` and requires the margin to go the OTHER way.
If step 8 passes as well as step 7, the subject is too symmetric to carry the
argument and BOTH numbers are about nothing -- the gate says so in those
words. Raise `IMPOSTOR_AZIM_MARGIN` above 0 once a real margin is measured.

### 4.3b The proof picture, `images/00_azimuth_180_explained.png`

Two harness runs on the SAME asymmetric subject, one on a set baked before the
repair and one on a set re-baked by this exe:

    WW_IMPOSTOR_SHOT=<dir>/old WW_IMPOSTOR_PREVIEW=azimuth ... (the legacy .lodm)
    WW_IMPOSTOR_SHOT=<dir>/new WW_IMPOSTOR_PREVIEW=azimuth ... (the re-baked .lodm)

then rename into `az_mesh_<a>.png` / `az_oldbake_<a>.png` / `az_newbake_<a>.png`
for a in 0 90 180 270 and

    python tools/impostor_pictures.py azimuth <dir> images/00_azimuth_180_explained.png
    : > images/00_READY

### 4.4 Depth and AO gate rows (owed, not yet written)

- depth-offset on/off changes the intersection line where a card stands half
  behind a wall, as a Python reference predicts;
- blend-with-height shows less double-edge energy between frames than without.

Both need a fixture scene; neither needs a lighting model.

### 4.5 The pictures (brief step 3) and the numbers (step 4)

Three subjects x N=4 and N=8 (N=12 if bake time allows): sheet contact sheets,
orbit strips (12 azimuths x 2 elevations, mesh top / card bottom), an
orbit GIF, a distance strip at the LOD4/8/16/32 switch heights (mesh vs
authored vanilla tree LOD vs card), and one chunk picture with triangle and
draw-call counts. Every number from a named log. **Authored LOD models only,
never decimate. `--road-detail 1`.**

Say in the write-up whether impostors are tree-only and why.

### 4.6 Still to write in phase B

- the chunk placement path (C12, C31): read the manifest's `C` lines and draw
  cards in place of vanilla tree LOD. **This is a feature master: it ships OFF
  and gets a menu row** (`nifskope-ww-panel-style`), plus `WW_IMPOSTOR_OCT=1`.
- the `.lodm` open path, so a `<id>_oct.lodm` opened from File -> Open is that
  file's own viewer. Not a master -- there is nothing else such a file could
  be shown as.
- the mip cap (C19) and the aux-sheet divisor (C32) in the shader.
- the menu rows for the debug channels and the depth/AO/blend toggles.
