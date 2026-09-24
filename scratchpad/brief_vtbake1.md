# Lane VTBAKE1 -- first whole-worldspace terrain texture pyramid (Commonwealth, --vt)

Director brief, 2026-09-23 08:0x. Model: Opus 5.5. QUEUED. Folder scratchpad/vtbake1_<date>/, progress.md incremental.

bungo 2026-09-23: "Let's do clipmaps then for textures" (docs/FO4CS_IMPROVED_LOD_PLAN.md 8.5). FO4CS R2's clipmap
rings are fed from the .lodt pyramid, and none has ever been written for a whole worldspace (plan section 5 row 14).

Jobs: 1. Bake Commonwealth with --vt (plus --vt-height, row 12) through the lodgen CLI on the current exe, into a
scratch output tree, NOT bungo's mod folder; skill nifskope-ww-lodgen first; report wall time, disk size per level
and per sheet. 2. Validate every container with lodvValidate / the 22 refusal rules (docs/LODGEN_TERRAIN_VT.md 3.4);
index .lodm against VT 4. 3. Pictures: each level's colour, normal and mask downscaled, NORTH-UP, labelled with its
units-per-texel; one crop at the finest level over Sanctuary beside what the stock chunk sheet shows there.
4. Numbers R2 needs for the clipmap design: texel size per level in metres, and the resident cost of an N-ring stack
at 1024 and 2048 windows per sheet format. No ring count is chosen here.

Rules: CONSTITUTION.md first. No GUI NifSkope unless a picture needs it (then the one harness instance, second
monitor). Do not commit; no HANDOFF/WW_CHANGES/MISTAKES edits -- text into DELIVERABLE_TEXT.md. DONE marker first word
DONE/PARTIAL/PENDING; report under 300 words, plain words.
