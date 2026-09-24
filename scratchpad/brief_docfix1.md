# Lane DOCFIX1 -- fix the contract-page defects PLANSYNC1 found (docs only, no build)

Director brief, 2026-09-23 22:2x. Model: Opus 5.5. Folder: scratchpad/docfix1_<date>/ ; progress.md INCREMENTALLY.

## Input
scratchpad/plansync1_20260923/DELIVERABLE_TEXT.md, section "Defects in contract pages" (27 items across 7 docs).

## Scope
- Fix the defects in: LODGEN_NATIVE_LODO_LODI.md (13), LODGEN_CARD_SHEETS.md (3), LODGEN_LODM_FORMAT.md (1),
  LODGEN_IMPOSTOR_SPEC.md (1), LODGEN_CENSUS.md (3), LODGEN_BTD_FORMAT.md (1).
- DO NOT touch docs/LODGEN_TERRAIN_VT.md: lane VTNORMAL1 is editing it now. Carry its 5 items forward in your
  deliverable as "owed after VTNORMAL1".
- Leave "Source strings (not docs, for a source lane)" alone. Only re-list them.
- The tree carries uncommitted changes from IMPOSTORDEPTH1/2, DEFAULTS2 and BLENDSEAM1: BC7 `_n`, the edge fix,
  blend on by default, and cards 8x8 at 2k. Some defects may already be fixed, or changed by those lanes. Re-verify
  EVERY item against the CURRENT source (file:line) before editing. The source is the truth; the doc follows it.
  Mark each item: fixed / already right / stale / needs a ruling.

## Rules
- Docs only. No build, no NifSkope launch, no src/ edits.
- search-lean before any search: src\, docs\, never the root or scratchpad.
- Preserve each file's line endings. Measure them with Python byte counts before and after, and use a binary splice
  if a file is mixed.
- Do not commit. Do not edit HANDOFF, WW_CHANGES or MISTAKES; put their text in DELIVERABLE_TEXT.md.
- The DONE marker's first word is DONE, PARTIAL or PENDING.
- Final report under 200 words, plain words: counts per status, and anything that needs bungo's ruling.
