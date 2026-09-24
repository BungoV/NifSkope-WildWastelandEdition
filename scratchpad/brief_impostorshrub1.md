# Lane IMPOSTORSHRUB1 -- bushes and small foliage bake empty cards

Director brief, 2026-09-23. Model: Opus 5.5. QUEUED: launches when IMPOSTOR16's N8 round lands (one build
lane at a time). Folder: scratchpad/impostorshrub1_20260923/ . Progress lines to progress.md INCREMENTALLY.

## bungo's words
Asked what happens to bushes and shrubs, heard that three small foliage models bake empty cards, and said
"sure" to fixing it next.

## Known facts (verify, do not trust)
- IMPOSTORTEAR1 (scratchpad/impostortear1_20260923/DELIVERABLE_TEXT.md:25): Sapling01,
  TreeElmUndergrowth01, ShrubGroupLarge05 bake an EMPTY card on the rung too: `oct` halfW 1.077, 0 covered
  texels. Not chased. A halfW of ~1 unit suggests the bounds/extent measured nothing (wrong source model,
  every shape hidden as an _L1/_L2 detail level, a first-range cut of a BSMeshLODTriShape that is empty,
  an alpha threshold that kills all texels, or a material the bake does not draw).
- The shrub that DID bake (shrub05 in the IMPOSTORTEAR1 contact sheet) reads muddier than its model.
- Candidate selection and source rules: docs/LODGEN_IMPOSTOR_SPEC.md:195-213; the frame law :606-642.

## Jobs
1. CENSUS FIRST, offline: every vanilla base `--list-impostor-candidates --candidates all` returns that is a
   bush / shrub / sapling / undergrowth (name it by record type and model path; vanilla corpus =
   E:\Tools\Fallout 4\DataUnpacked\Data). Bake each on the current exe; table: covered texels, halfW, source
   model, hidden shapes, ranges. How many are empty, and one named cause per empty class.
2. FIX the causes in the bake (src/), smallest change per class; rebake; zero empty cards among the census,
   or each remaining one explained with its record.
3. MUDDY: shrub05 card vs model -- name the stage (albedo sheet, mip, lighting, AO) with numbers; fix if it
   is the bake's.
4. Gates: impostor_draw.sh, lodgen_octahedral.sh, native_lighting.sh as control (2 reds pre-existing). A new
   row: no candidate in the census bakes an empty card -- it FAILS on the rung. Never lower a bar.
5. PICTURES: contact sheet of every shrub/bush: 3D model | card, labelled "3D model" / "Octahedral impostor",
   elevation 0 and 20. Keep rocks out of his pictures (the director offered; rocks stay as gate controls).

## Rules
CONSTITUTION.md first; skills nifskope-ww-lodgen, nifskope-ww-build-verify, nifskope-ww-render-shot,
ww-reference-card-diagnose, ww-test-harness-add. Game down before any build. bungo's NifSkope window may run
release/NifSkope.exe directly: never kill it; rename aside. One harness NifSkope at a time, second monitor,
no focus steal. Rung release/NifSkope.before_impostorshrub1.exe first. Do not commit; do not edit
HANDOFF/WW_CHANGES/MISTAKES -- text into DELIVERABLE_TEXT.md. DONE marker first word DONE/PARTIAL/PENDING.
Final report under 300 words, plain words: verdict, census numbers, causes, gates, picture path, exe
size/time/sha1.
