# Lane PLANSYNC1 -- bring docs/FO4CS_IMPROVED_LOD_PLAN.md up to date with what the generator writes TODAY

Director brief, 2026-09-23 08:1x. Model: Opus 5.5. LIVE. Folder scratchpad/plansync1_20260923/, progress.md incremental.

bungo: "was the spec for fo4cs up to date? For the new LOD". Director found: NO. The plan's section 8 version table
says .lodo 3 / .lodi 3 / .lodl writer 2; the source says LODO_VERSION 4 (src/lodofile.h:78), .lodi up to 9
(src/lodifile.h: 3 base, 4 aggregate, 5 placement AO, 6 vertex AO, 7 group sky, 8 horizon, 9 scrappable),
LODL_VERSION 3 (src/lodtfile.cpp:57), .lodt LDTX 2, .lodm 1; the plan's own section 5 contradicts its table.
A FO4CS lane (IMPROVEDLOD-R0) is building the loaders NOW and has been told the source wins over the plan.

## Jobs (skill ww-contract-provenance FIRST; it is the procedure for exactly this)
1. Versions: re-derive every container's version and refusal list from the writer source + the contract pages;
   rewrite section 8's table; every .lodi/.lodo bump since v3 gets one line: what it added, which rung reads it.
2. Rung by rung (R0-R5, sections 1, 3, 4, 5, 7): every field, offset, file path, pinned number and refusal word
   checked against the current contract pages (docs/LODGEN_*.md) and the files on disk. Pinned gate numbers
   (e.g. R0 gate 1 sizes/counts) re-measured from the current files; old value kept struck-through with the date.
3. Section 5 (what the generator owes): each open row re-checked -- closed rows marked DONE with the lane/date that
   closed them, new owed rows added (e.g. the card PBRM specular/tint home from IMPOSTORPBRM1, wind channel from
   IMPOSTORWIND1, the Commonwealth --vt pyramid from VTBAKE1 -- as PENDING, not invented).
4. R4 cards against today's impostor work (docs/LODGEN_IMPOSTOR_SPEC.md + HANDOFF 2026-09-22/23 lines): N x N
   hemi-octahedral grids, the frame law (size ladder 1, 1/2, 1/4, 1/8, floor 32 px; aspect ladder; --card-half-aux),
   sheet channels, families legacy/pbr, the crisp-over-smooth ruling + the TRANSITION SMOOTHNESS SLIDER block
   (already in R4), what is still UNRULED (default grid, default cut/slider, card AO/gloss, _n swap) listed in
   section 6 as open rulings.
5. R2: section 8.5 (clipmap textures, quadtree geometry) folded into R2's READS/DOES as the ruled design; ring
   count/window stay unchosen.
6. A short "CHANGED SINCE 2026-09-16" block at the top of the page listing every correction, so the FO4CS side
   can re-check what it already built.

## Rules
CONSTITUTION.md first. Docs only: no src edits, no builds, no GUI. You MAY edit docs/FO4CS_IMPROVED_LOD_PLAN.md
(LF; binary-safe Python splices, byte counts checked); do not edit other contract pages -- a contract page that is
itself wrong goes into DELIVERABLE_TEXT.md as a finding. Do not commit; no HANDOFF/WW_CHANGES/MISTAKES edits.
Never invent a ruling; unruled = listed as open. DONE marker first word DONE/PARTIAL/PENDING; report under 300 words,
plain words: how stale it was (count of corrections by kind), anything that changes what R0 must build.
