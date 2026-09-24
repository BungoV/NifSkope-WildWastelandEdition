# Lane IMPOSTORDEPTH2 -- BC7 depth sheet, the edge fix always on, the smooth end = stipple + search 16

Director brief, 2026-09-23 11:4x. Model: Opus 5.5. Follows IMPOSTORDEPTH1 (PARTIAL, exe e294ae80, not committed;
read scratchpad/impostordepth1_20260923/FOR_BUNGO.md, DELIVERABLE_TEXT.md and progress.md first). Folder:
scratchpad/impostordepth2_<date>/ . Write progress lines to progress.md INCREMENTALLY.

## bungo's rulings, 2026-09-23 11:4x (asked with explanations; RULED, not optional)
1. The card's depth sheet is stored as BC7. It was offered against depth in the DXT5 alpha block and an
   uncompressed `_n`. Today the depth sits in the DXT5 B channel, a 5-bit colour endpoint, with a mean error of
   2.81 levels (34 units).
2. The coverage edge fix (decode, then filter) is ALWAYS ON with NO switch. It is a repair. Standing rule: no
   fix-only toggles. Retire `WW_IMPOSTOR_COVFILTER` as a switch; the fixed path is the only path.
3. The transition slider's SMOOTH end is stipple + depth search 16 (plan docs/FO4CS_IMPROVED_LOD_PLAN.md, R4
   "TRANSITION SMOOTHNESS SLIDER"). The crisp end keeps the standing ruling: choppy is fine, and noise and tearing are
   defects. Crisp + search draws the trunk too thin, so it is NOT the crisp end. Report what the crisp end should be,
   with measurements; do not decide it.

## Jobs
1. Card bake writes `_n` as BC7. The encoder must be in-tree or already vendored; say which. It must be
   deterministic, so the same input gives the same bytes. Keep every other sheet's format unless BC7 is needed for
   the same reason; name it if so. Any `.lodm` or contract change is written up in docs/LODGEN_CARD_SHEETS.md and
   docs/LODGEN_IMPOSTOR_SPEC.md, including what FO4CS must decode.
   - Bump the version only if the meaning changes. The FO4CS reader is built last, so this is contract text only.
   - Measure depth decode error vs the bake's own PNG: mean, p95, and the "heights surviving" count (DXT5 today:
     2.81 / 8 levels, 28 of 59).
   - Measure bake time and file size, before and after.
2. The edge fix becomes the only code path. The default picture changes on purpose, so re-pin every gate that pinned
   the old picture, and say which ones and by how much. Never lower a bar.
3. The smooth end (stipple + search 16) is wired as the slider's 1.0 end, per the plan's slider contract.
   `WW_IMPOSTOR_SEARCH` stays a harness override only.
4. Gates:
   - `impostor_trunk.sh` must go green on the NEW default sheets at el 0 AND el 20. DEPTH1 missed 4 of 360 views at
     el 20 near az 113 with uncompressed height; if BC7 still misses, report the cause and do not lower the bar.
   - `impostor_draw.sh`, `impostor_aa.sh`, `lodgen_octahedral.sh`, `impostor_shrubs.sh`, and `native_lighting.sh` as a
     control (2 known reds).
   - The trunk rows fail on the rung (e294ae80).
5. Pictures, as the same four-panel GIF as DEPTH1 (<= ~15 MB): "3D model" | "8x8 snap" | "crisp end (current)" |
   "smooth end: stipple + search 16", on BC7 sheets. Also the trunk strip before and after.

## Rules
- Read CONSTITUTION.md first.
- Skills: ww-shader-devloop-nobuild (shader iteration before any build), nifskope-ww-build-verify,
  nifskope-ww-render-shot, nifskope-ww-lodgen, ww-reference-card-diagnose, ww-test-harness-add.
- The game must be down before any build.
- bungo's NifSkope window: never kill it; rename it aside, and check for an existing `NifSkope_inuse_<pid>.exe` first
  (DEPTH1 MISTAKES).
- Rung release/NifSkope.before_impostordepth2.exe first.
- One harness NifSkope at a time, on the second monitor.
- Clear release/impostor_trunk_tmp (924 MB) when done.
- Do not commit. Do not edit HANDOFF, WW_CHANGES or MISTAKES; put the text in DELIVERABLE_TEXT.md.
- The DONE marker's first word is DONE, PARTIAL or PENDING.
- Final report under 300 words, in plain words.
