# Lane IMPOSTORSHOW -- show bungo the octahedral impostors IN ACTION (he has never seen them drawn)

## Header
- Tree `E:/Projects/NifskopeWildWastelandEdition`, main. `date` for every timestamp. Never commit, never `git stash`,
  never edit `WW_CHANGES.md` / `HANDOFF.md`. Folder `scratchpad/impostorshow_20260919/` (`BUILDING` first, `DONE` last,
  first word `impostorshow`; `report.md` incremental, section 0 inside ten tool calls; `PENDING.md` past half context).
- Game + instance rules: `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` as ITS OWN command before every build
  and exe run. Fallout4 up = no build, no exe run, park at `BUILD PENDING`, never wait-loop. ONE NifSkope at a time,
  `--port <unused>` + `WW_WINDOW_AT=1960,40`, absolute `E:/...` paths. A NifSkope without `--port` is bungo's: never
  kill, rename the exe aside at link time. Wedged harness instance: `NifSkope::open <scene>` UTF-16LE UDP to its port.
- Build (only if step 2 needs it): MSYS2 UCRT64, skill `nifskope-ww-build-verify`; rung ONCE
  `release/NifSkope.before_impostorshow.exe`; never delete any rung.
- Read first: `CONSTITUTION.md`; HANDOFF top block; MISTAKES.md top 20; `docs/LODGEN_IMPOSTOR_SPEC.md`;
  `tests/spells/lodgen_octahedral.sh`, `lodgen_impostor_cards.sh`, `lodgen_card_arrays.sh`; skills
  `nifskope-ww-lodgen`, `nifskope-ww-render-shot`, `ww-texel-picture`, `ww-test-harness-add`.

## bungo's words (2026-09-19 05:4x)
"you still haven't shown me octahedral imposters in action though"

Then, 05:4x: "we need a preview for impostors, that actually works, up to spec from the way it was meant to work in
the documentation". = THE DELIVERABLE IS A REAL, KEPT VIEWER FEATURE, not a one-off picture script: an impostor
PREVIEW in NifSkope that draws the baked set exactly the way `docs/LODGEN_IMPOSTOR_SPEC.md` (and the FO4CS contract
sections that cite it) says the runtime must draw it -- every sheet used for what the spec says it is for (colour +
coverage, normal X/Y + height + sway, the mask sheet, GSAOS / RMAOS per family, emissive), the spec's view grid
mapping, the spec's frame selection + blend, the spec's extents/aspect rule, both material families. Step 1 therefore
includes a CLAUSE TABLE: every normative sentence of the spec about drawing -> implemented at file:line / not
implemented / spec silent or contradictory (a silent clause is reported as a SPEC GAP with your proposed wording for
the director; you may implement the standard hemi-octahedral method for it but mark it, never pass it off as spec).
The preview must open (a) a baked impostor set on its own (`<id>_oct.lodm` opened like any file: orbit it, see the
card turn through its frames, a toggle mesh / impostor / split, a sheet-channel debug view) and (b) inside a native
LOD chunk where `C` manifest lines place them. Harness `WW_IMPOSTOR_PREVIEW` per `ww-test-harness-add`. The preview
of an opened `_oct.lodm` is the file's viewer (not a master); drawing octahedral cards inside a chunk in place of
vanilla tree LOD is a feature master and ships OFF with a menu row.

SCOPE LIMIT, bungo 05:4x: "we can't display the impostors fully though with all the textures, not until we recreate
lighting in nifskope from 76 and fo4cs pbrm" -- corrected by him at once: "not 76", he means FALLOUT 4's own lighting
(the legacy family) and FO4CS PBRM (the pbr family). The PBR renderer in NifSkope waits on his signal (standing rule)
-- do NOT write an FO4 or PBRM lighting model here. So: the preview is COMPLETE in what is geometric -- view-grid mapping,
frame selection + blend, extents/aspect, coverage cut-out, height/parallax if the spec uses it, sway if previewable --
and lit by the viewer's EXISTING lighting from the colour sheet + the baked normal sheet only. The material sheets
(mask, GSAOS / RMAOS, emissive) are loaded, validated against the spec and shown as DEBUG CHANNELS (one per sheet
channel), but not shaded. The clause table gets a fourth verdict: "waits for the PBR renderer", and the code leaves
ONE clearly named seam where the future renderer takes the sheets (no stub shading, no fake specular).

DEPTH + AO ARE IN SCOPE (bungo 05:5x: "the impostor also carries baked AO and depth, right?" -- yes, spec lines
~284-293: `_n` blue = height, 0.5 = card plane, units = (v - 0.5) x depthspan; third sheet blue = AO). Neither needs
a material lighting model, so the preview USES them, not only shows them: height -> the spec's ghost-free frame
blend + pixel depth offset (gl_FragDepth, so a card intersects terrain/meshes/other cards as a volume -- prove it
with a card standing half behind a wall) ; AO -> multiplied into the viewer's existing ambient term, with a toggle.
Both also get their debug channel. Gate rows: depth-offset on/off changes the intersection line as a Python
reference predicts; blend with height shows less ghosting than without (measure double-edge energy between frames).

VARIABLE GRIDS (bungo 09:4x: "The impostor bakes have variable rows / tiles"): nothing in the preview may assume a
grid size or a frame size. N (2..16), frameW, frameH, extents and depthspan are read PER SET from the set's own
`_oct.lodm` / manifest `oct` + `C` lines; a chunk may hold sets of different N and different frame size classes side
by side (a per-card uniform/instance attribute, never a global). Gate rows in phase B: the same subject baked at N = 4,
5 (odd: a true top frame exists), 8 and 12 all preview with IoU over the floor; a non-square frame (a tall pine:
frameH > frameW) keeps its aspect; two sets of different N drawn in ONE scene each pick their own frames (red
control: force one set's N onto the other -> IoU collapses). If the bake can emit a NON-SQUARE grid (rows != columns)
anywhere, say where (file:line) and support it; if the spec says N x N only, say so in the report for bungo.

## The work
1. **State of play, no edits** (report s1): what the bake writes today (sheets, N x N grid, `_oct.lodm`, `C` manifest
   lines) and -- the question that matters -- does ANY code in the viewer DRAW an octahedral card: pick the frame(s)
   from the camera direction, blend neighbours, use the baked normal/depth? Or does the viewer draw only a flat card
   with one frame? Quote file:line. If a draw path exists, go to 3.
2. **If no draw path exists: write it** in the native LOD viewer (the `.lodo/.lodi` scene path), feature master OFF by
   default per house rule (viewer option + env/harness switch `WW_IMPOSTOR_OCT=1`; menu row per `nifskope-ww-panel-style`):
   camera-facing card, hemi-octahedral direction -> grid cell, 3-frame barycentric blend (say what the spec says; if
   the spec is silent follow the standard hemi-octahedral impostor method and state it), alpha test from coverage,
   lighting from the baked normal sheet with the viewer's sun. No mesh decimation anywhere. Gate
   `tests/spells/impostor_draw.sh`: (a) frame index chosen for 8 named camera azimuths matches a Python reference;
   (b) the card's silhouette IoU against the real mesh rendered from the same camera >= a floor you set from
   measurement, at 8 azimuths x 2 elevations; (c) red control: frames shuffled -> IoU collapses.
3. **The show** (`images/`): three subjects -- a maple (the gate's Sanctuary candidate), a pine/dead tree, and one
   non-tree candidate if the bake accepts it (a car or a water tower; say if impostors are tree-only and why).
   For each, at N=4 AND N=8 (and N=12 if bake time allows):
   - the baked sheets as a contact sheet (colour + normal);
   - an ORBIT STRIP: 12 azimuths x 2 elevations, top row the real near mesh, bottom row the impostor card, same
     camera, same light -- one PNG per subject per N;
   - an animated GIF or APNG of the orbit, mesh left / impostor right (Python/PIL from the frames);
   - a DISTANCE STRIP: the subject at the pixel heights it would have at LOD4 / LOD8 / LOD16 / LOD32 switch
     distances, mesh vs authored vanilla tree LOD vs octahedral card, three rows.
   - one CHUNK picture: a tree-heavy chunk drawn with vanilla tree LOD cards vs octahedral cards, same camera, plus
     the triangle + draw-call counts of each.
4. Numbers beside the pictures: bytes per impostor set per N (after BC compression), bake seconds, silhouette IoU,
   mean colour error vs the mesh render. Every number from a named log.

## Rules
Authored LOD models only, never decimate; `--road-detail 1`; masters ship OFF; no "fixed/final/true" -- mechanism +
refuter; plain words.

## Report
0 exe at launch; 1 state of play; 2 draw path (if written: files, lines, build mtime/size/sha1, gate counts,
neighbours `render_shot.sh`, `native_open.sh`, `lodgen_octahedral.sh` before/after); 3 the pictures list; 4 numbers;
5 WW_CHANGES + HANDOFF text for the director; 6 MISTAKES; 7 skill text. END with `DONE` + five plain sentences for
bungo (last: whether his open window needs a restart). Final message under 300 words.
