# Lane PBRR2B -- PBR renderer stage R2b: lookdev + W1 weather (BUILD lane)

Director brief, 2026-09-24. Model: Opus 5.5. Folder: scratchpad/pbrr2b_<date>/ ; progress.md INCREMENTALLY.
Chain rules: scratchpad/brief_pbrchain.md. Skills: nifskope-ww-pbr-shade-ab, the render-shot skill.
PRECHECK: free space on E: >= 2 GB, else stop at once with DONE.md "PENDING disk" (a Steam update filled it at 05:2x).

## Read first
docs/NIFSKOPE_PBR_RENDERER.md: row R2b (~741), the W1 section (~491) and gates W G1-G5 + G7, the RULINGS.
HANDOFF.md RULED 00:5x 2026-09-24 line (lookdev = sky/cube background, sun, ground plane, W1 weather).
PBRR2A lane text (scratchpad/pbrr2a_20260924/DELIVERABLE_TEXT.md): the Scene window and its section skeleton.

## Scope = the R2b row
`LookdevStage`; ground quad (Ground section: plane toggle row, live, default ON in Lookdev); cube-only background;
`EsmWeather` W1 = sun + ambient/DALC + cube source from a WTHR in the loaded plugins; hour row; master refusal.
Scene window rows: Mode gains Lookdev; Weather (plugin, weather picker, hour) made real for W1 only (sky colours,
clouds, moon, fog are LATER stages -- leave their rows disabled). Every row live.

## Gates = the R2b row
W G1-G5 and G7 as the doc defines them; `WW_LOOKDEV` unset -> `zero` (pbr_shade_ab zero set); ground toggle off ->
pixel-identical to no ground; each with a red control. Harness settings isolated.
Rung before_pbrr2b from the exe PBRR2A left (b985beac). DONE.md + DELIVERABLE_TEXT.md as in the chain rules.
