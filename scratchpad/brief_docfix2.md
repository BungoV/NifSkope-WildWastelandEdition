# Lane DOCFIX2 -- terrain VT doc + lodgen skill after VTNORMAL1 (DOCS ONLY, no build)

Director brief, 2026-09-24 01:4x. Model: Opus 5.5. Folder: scratchpad/docfix2_20260924/ ; progress.md INCREMENTALLY.

## Jobs
1. Apply VTNORMAL1's doc text to docs/LODGEN_TERRAIN_VT.md: scratchpad/vtnormal1_20260923/DELIVERABLE_TEXT.md section
   "docs/LODGEN_TERRAIN_VT.md" (lines ~88-149). Check every claim against src (search-lean: src/lodgen*.cpp,
   src/ui/ only) before writing; the source wins over the lane text; list any correction.
2. Apply the 5 items owed after VTNORMAL1: scratchpad/docfix1_20260923/DELIVERABLE_TEXT.md section "Owed after
   VTNORMAL1" (line ~36). Re-check each against the NEW source (VTNORMAL1 changed it).
3. Apply the "Skill review (nifskope-ww-lodgen)" section to .claude/skills/nifskope-ww-lodgen/ (SKILL.md and its
   references): Editing traps + CLI section additions. Keep the skill's style.
4. Do NOT touch src/, tests/, or anything the build lane PBRR0 is editing. Do not edit HANDOFF, WW_CHANGES,
   MISTAKES: text in DELIVERABLE_TEXT.md.

## Rules
- Line endings: measure each file with Python byte counts before editing; keep each file's own style (binary splice
  for mixed files). No commit.
- DONE.md first word DONE/PARTIAL/PENDING. Final report under 150 words.
