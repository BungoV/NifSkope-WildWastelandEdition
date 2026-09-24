# Lane IMPOSTORLOOK1 -- bungo: "Impostors look off". Say exactly HOW, at a size a person can judge (OFFLINE)

## Header
- Tree `E:/Projects/NifskopeWildWastelandEdition`, main. `date` for every timestamp. Never commit, never `git stash`,
  never edit `WW_CHANGES.md` / `HANDOFF.md`. Folder `scratchpad/impostorlook1_20260919/` (`BUILDING` first, `DONE`
  last; `report.md` incremental, section 0 inside ten tool calls; fallback name `DELIVERABLE_TEXT.md`; `PENDING.md`
  past half context). Pictures only under that folder.
- OFFLINE ONLY. Lane CELLWORK1 owns the build and exe slots. You do NOT build, do NOT run `release/NifSkope.exe` or
  any rung, do NOT run an exe-starting spell, do NOT edit existing `src/`, shader or `tests/spells` files in place.
  Python + numpy/PIL on existing sheets and existing grabs. Source changes = ONE refusing anchored script (skill
  `ww-anchored-hookup`), `--check` quoted.
- Read first: `CONSTITUTION.md`; HANDOFF top block; `scratchpad/impostorfix5_20260919/DELIVERABLE_TEXT.md`;
  `scratchpad/impostorfix4_20260919/DELIVERABLE_TEXT.md`; `docs/LODGEN_IMPOSTOR_SPEC.md`;
  `res/shaders/impostor_oct.frag`; skills `ww-reference-card-diagnose`, `ww-silhouette-compare`, `ww-texel-picture`.

## What bungo was shown
`scratchpad/impostorfix5_20260919/images/00_before_after_blast_n8.png` and `..._maple_n4.png` (card / card / mesh,
12 azimuths, each tree ~130 px tall). His whole verdict: "Impostors look off". The director, looking at the same
strips, sees: (1) the card's trunk is FATTER and its outline LUMPY where the mesh is smooth; (2) the card is FLAT and
dull -- the mesh has a bright lit side / highlight that turns with azimuth, the card looks evenly lit from every
side; (3) thin branches come out thicker and shorter; (4) on the fine-twig maple the crown is blobs. All the work so
far scored SILHOUETTE overlap only -- nothing has ever measured colour or shading.

## The work
1. BIG PICTURES FIRST. From the existing IMPOSTORFIX5 grabs (card and mesh, same views) make side-by-side pairs at
   the grabs' native size, 4 azimuths x 2 subjects (bare maple N=8, fine-twig maple), plus a x3 nearest crop of the
   trunk and of one fork. These go to bungo; caption each with one plain sentence on what differs. Never pick the
   flattering view: choose azimuths 0/90/180/270.
2. SHADING, measured for the first time: per view, inside the intersection of the two silhouettes, mean luma, luma
   contrast (left half vs right half of the trunk = the lit-side signal), mean colour (Lab delta), and how those
   move with azimuth for mesh vs card. Is the card unlit, lit from a fixed baked direction, or lit with the wrong
   normal frame? Decide BAKE / SHEET / DRAW with the reference card: decode `_n` normals, rotate them by each
   frame's bake basis to world, light them with the viewer's light the way the mesh is lit (read the mesh path's
   light setup from source), and compare with (a) the mesh grab and (b) the viewer's card grab. (a) good + (b) bad =
   the draw's lighting is wrong, name the line. Check specifically: are normals stored in frame (view) space and
   used as world space? is the albedo sheet baked WITH lighting already in it (double lighting / baked highlight)?
   is sRGB applied twice or not at all? is AO/depth used as his ruling said ("depth and AO are used; material sheets
   debug-only until the FO4/PBRM renderer exists")?
3. OUTLINE: lumpy trunk edge -- at native size, is it 4x4 block shaped (BC), texel shaped (frame resolution: how
   many sheet texels across the trunk?), or parallax tearing (turn parallax off in the reference card and look at
   the edge only)? Numbers: edge roughness (std of trunk-edge x position along height) mesh vs card, per stage.
4. Ranked list: defect | what bungo would see | BAKE/SHEET/DRAW | repair file:line | simulated before/after picture |
   needs re-bake? | needs a ruling? Anchored script(s) for the repairs that need no ruling. Alpha cut-off, `_n`
   channel swap and frame size are owed rulings: measure their share of "looks off", apply none.

## Rules
Simulations labelled SIMULATIONS; the known-answer control must reproduce first (0.88 on blast N=4 single frame).
Authored LOD models only, never decimate. No "fixed/final/true" -- mechanism + refuter. Plain words.

## Report
The picture paths for bungo (most telling first); verdict per defect; ranked list; script + `--check`; MISTAKES
appended to root MISTAKES.md (top CRLF, byte splice, CR before/after); skill update for
`ww-reference-card-diagnose` (shading arm) to both trees, equal sha1. Final message under 250 words.
