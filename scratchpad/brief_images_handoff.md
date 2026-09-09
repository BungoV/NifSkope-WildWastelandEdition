# LANE IMAGES3 -- pictures of what the generator makes today, for the FO4CS handoff

Header: tree E:\Projects\NifskopeWildWastelandEdition, main, ~78 uncommitted paths,
NO commits. Read CONSTITUTION.md, HANDOFF.md top block, then invoke via the Skill
tool: `nifskope-ww-render-shot` (mandatory), `nifskope-ww-vanilla-compare`,
`nifskope-ww-lodgen` (CLI: --terrain-region, object chunks, --slot-fallback,
impostor cards, the harness rules), `nif` if a block needs inspecting. Model Opus 5.
Exe: release/NifSkope.exe 15:30:27 (the current build; do NOT build). bungo's
own NifSkope window may be open: never touch it, one harness instance at a time,
second monitor (WW_WINDOW_AT), unused --port each launch, absolute paths on every
exe argument. Check `tasklist | grep -i Fallout4; echo rc=$?` prints rc=1 before
each launch. When you are done, create scratchpad/images_20260909/DONE (empty
file): another lane waits on it before building.

bungo's words, verbatim: "before handoff to fo4cs, I need images of the terrain
chunks we generate now (the ones with geometry) and especially the octahedral
impostors generated too".

## The work -- generate fresh with the CURRENT exe into scratchpad/images_20260909/gen/
1. Terrain chunks (.btr) with geometry: regenerate one region at EACH level 4, 8,
   16, 32 (pick a region with relief and a shoreline, e.g. the one containing
   cell (0,0) or the mountain tiles of lane IMAGES). Photograph each: lit mesh
   with its diffuse and normal sheets, and a wireframe or WW_LOD_CHANNEL view
   that shows the triangulation, vanilla left / ours right, same pinned camera,
   labels burned in (the vanilla-compare skill's procedure and caption rules;
   captions must fit -- open every PNG).
2. Object chunks (.bto) with geometry: one ring-0 chunk (dim 4) and one far-ring
   chunk (dim 16, --slot-fallback, the merged + simplified proxies), vanilla vs
   ours where vanilla has the chunk, ours alone where it does not (say so).
3. The octahedral impostors, ESPECIALLY: bake the cards for the tree bases the
   impostor spec names (tools/bake_impostor_cards.sh, OCT=8, 8x8 = 64 frames)
   into gen/; photograph (a) the card SHEETS themselves: base colour, normal,
   mask, emissive, each as a straight image with the 8x8 frame grid visible;
   (b) one card placed in a chunk, rendered from three view angles so the frame
   selection is visible; (c) the source tree model beside its card from the
   same angle. Label everything.
4. A contact sheet: scratchpad/images_20260909/handoff_contact_sheet.png with
   every picture above thumbnailed and captioned, for the FO4CS handoff document.

## Gates
- Every picture regenerates from a command written in the report.
- Every composed PNG opened and looked at before delivery; captions inside the
  frame.
- The exe used is 15:30:27 and no build happened.

## Report
scratchpad/lane_images_handoff_report.md, incremental: 1. Regions and bases
chosen, why. 2. Commands. 3. Picture list with paths. 4. What the pictures show,
two sentences each, no cause verdicts. 5. Mistakes (append to MISTAKES.md too).
6. Finished-work skill review. Final message under 25 lines, contact-sheet path
first, then the picture paths.
