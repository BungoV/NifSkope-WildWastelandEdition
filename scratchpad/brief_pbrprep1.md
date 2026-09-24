# Lane PBRPREP1 -- specs for the later PBR chain stages (RESEARCH ONLY: no build, no src/tests edits)

Director brief, 2026-09-24. Model: Opus 5.5. Folder: scratchpad/pbrprep1_20260924/ ; write each spec file as soon as
it is done (incremental). Runs beside the build lane PBRR2B -- never build, never launch NifSkope, never touch src/,
tests/, docs/, HANDOFF/WW_CHANGES/MISTAKES.

## Why
After R3/R4 the chain builds: weather preview (sky colours, clouds, hour, moon + phase + moonlight), fog (vanilla
parity), cascaded shadows (vanilla 3 cascades 800/3000/dist, 16-tap Poisson), contact shadows (FO4CS Bend), SSAO
(FO4CS GTAO), SSGI (FO4CS), bloom (weather ImageSpace + FO4CS). Each build lane needs the exact data and maths first.
Full ruling: HANDOFF.md line "RULED 00:5x 2026-09-24". Existing W1 design: docs/NIFSKOPE_PBR_RENDERER.md ~line 491.

## Deliver one spec per feature (spec_<feature>.md), each with:
1. DATA: the exact record fields (WTHR, CLMT, IMGS/ImageSpace, sun/moon textures) with subrecord signatures, types,
   units and per-time-of-day slots -- from xEdit's wbDefinitionsFO4.pas (authoritative; find the cached copy, search
   lean: E:\Tools first) -- and how NifSkope's existing ESP/plugin reader (src/, READ-only) can reach them.
2. MATHS: the vanilla shader behaviour to match. Vanilla research = Todd's treat FIRST (the Todd's treat tooling (kept outside this repo);
   quote the RVA and build), then FO4CS's own HLSL for the FO4CS effects
   (E:\Projects\Fo4CommunityShaders -- one folder per search). Time-of-day blend rules, moon phase from the game day,
   fog near/far/power/max + height fog, cloud layer scroll/alpha/colour, bloom thresholds.
3. PORT NOTES: what maps onto NifSkope's GL renderer (the PBR program from R1-R2a; the Scene window rows), what does
   not, the smallest faithful version.
4. GATE PROPOSAL: a measured invariant per feature that fails on broken code (toggle off = pixel-identical; on = a
   named numeric check), with its red control.
Features: weather+sky, clouds, moon, fog, cascaded shadows, contact shadows, SSAO, SSGI, bloom.

## Rules
Mark every claim SOURCE (file:line / RVA) or INFERRED. Nothing extracted from Fallout 76 ships. Finish with DONE.md
(first word DONE/PARTIAL) and a report under 200 words.
