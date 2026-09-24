# Lane IMPOSTORFIX4 -- what is still wrong with the impostor card, decided OFFLINE (no build, no exe)

## Header
- Tree `E:/Projects/NifskopeWildWastelandEdition`, main. `date` for every timestamp. Never commit, never `git stash`,
  never edit `WW_CHANGES.md` / `HANDOFF.md`. Folder `scratchpad/impostorfix4_20260919/` (`BUILDING` first, `DONE`
  last; `report.md` incremental, section 0 inside ten tool calls; if you cannot write report.md, write
  `DELIVERABLE_TEXT.md`; `PENDING.md` past half context). Pictures only under that folder.
- OFFLINE ONLY. Another lane (CELLVIEW3) owns the build slot and the exe slot and is editing `src/cell*`,
  `src/lodgen.cpp` (material loading), `src/esmdata.cpp`, `NifSkope.pro`. You do NOT build, do NOT run
  `release/NifSkope.exe` or any rung, do NOT run an exe-starting spell, and do NOT edit an existing `src/`, shader or
  `tests/spells` file in place. Python + numpy/PIL on the existing fixture sets and the existing mesh orbit renders.
  Source changes are delivered as ONE refusing anchored script per skill `ww-anchored-hookup` (exact-once anchors,
  `--check` writes nothing, CR delta asserted, the already-applied marker is NOT the anchor), `--check` output quoted.
- Read first: `CONSTITUTION.md`; HANDOFF top block; `scratchpad/impostorfix3_20260919/report.md`;
  `scratchpad/impostorfix2_20260919/report.md` s7; `scratchpad/octf1_20260919/report.md`;
  `docs/LODGEN_IMPOSTOR_SPEC.md`; `res/shaders/impostor_oct.{vert,frag}`; `src/lodgen.cpp` (`lodgenRepairOctHeight`,
  `lodgenDilateFrames`, frameOffset); the bake in `src/nifskope_ui.cpp`; skills `ww-reference-card-diagnose`,
  `ww-simulate-before-build`, `ww-anchored-hookup`, `ww-silhouette-compare`, `ww-texel-picture`.
- State today (24 views, real exe, IMPOSTORFIX3): bare maple N=4 0.57, N=8 0.75, dead tree 0.61, leafy maple 0.37,
  rock 0.83. Fixture sets and mesh renders: under `scratchpad/impostorfix3_20260919/` and
  `scratchpad/impostorshow_20260919/fixture/`. If a needed input does not exist, say which one the next build lane
  must shoot; do not substitute.

## The four defects to decide, each BAKE vs SHEET vs DRAW, by measurement
1. FAT INK: the card's inked area is 1.1-1.9x the mesh's. Split the excess by cause with the reference card, one
   stage at a time: the coverage floor 16/255 (the owed alpha ruling -- report its share, do not apply it), the
   colour dilate bleeding coverage, BC3 alpha ramp error, mip choice, the 3-frame union, parallax smear. A table:
   stage | ink ratio | IoU, per subject. Name the largest share that is NOT the owed ruling and propose its repair.
2. BLOCK CHIPS AT THE TRUNK: 4x4-block-shaped? Show the texels (`ww-texel-picture`) in `_d` alpha and `_n` blue at
   a chip. Is it BC3 endpoint error on the height channel across a silhouette edge (then the 8-ring dilate's fill
   value or ring count is the lever), or the encoder, or the draw's height step? Simulate the candidate repair on
   decoded sheets and give simulated IoU + a before/after crop.
3. LEAFY MAPLE 0.37: why is a full crown the WORST subject when the spec says crowns are the easy case? Look at the
   single-frame known-answer control first (one frame from its own direction): if that is low, the sheet/bake is
   wrong for alpha-tested leaves (two-sided leaves, alpha-test threshold at bake, leaf cards seen edge-on, depth
   written by discarded texels); if it is high, the blend is. Numbers per stage.
4. THIN AT 16 px (IoU 0.450) and N=4 GHOSTING: what does the spec's own floor imply for N at a given screen size?
   Give the curve IoU vs on-screen height for N=4/8 from the reference card with correct mip selection, and say what
   a consumer should do below the knee (spec wording proposal, not a code default).
5. End with a ranked list: repair | file:line | simulated gain per subject | risk | needs a re-bake yes/no | needs a
   ruling yes/no. Prepare the anchored script(s) for every repair that needs NO ruling, plus one gate row each that
   would fail on today's exe (as text for `impostor_draw.sh` / `lodgen_octahedral.sh`, not applied).

## Rules
Simulations are SIMULATIONS: label them; the build lane makes them real. The known-answer control (0.8823 on blast
N=4) must reproduce before any number of yours counts. Never pick the flattering view: all 24 views, mean + worst.
Authored LOD models only, never decimate. No "fixed/final/true" -- mechanism + refuter. Plain words.

## Report
Verdict line per defect (BAKE / SHEET / DRAW / SPEC LIMIT, n.nn -> n.nn simulated); the ranked list; script paths +
`--check` output; three most telling pictures by path; your MISTAKES appended to root MISTAKES.md (top of file CRLF,
byte splice, print CR before/after); skill update text for `ww-reference-card-diagnose` if earned (both trees, equal
sha1). Final message under 250 words.
