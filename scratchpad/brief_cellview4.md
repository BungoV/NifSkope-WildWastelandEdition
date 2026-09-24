# Lane CELLVIEW4 -- blended ground + the black shape, CODE-ONLY (no build, no exe run)

## Header
- Tree `E:/Projects/NifskopeWildWastelandEdition`, main. `date` for every timestamp. Never commit, never `git stash`,
  never edit `WW_CHANGES.md` / `HANDOFF.md`. Folder `scratchpad/cellview4_20260919/` (`BUILDING` first, `DONE` last;
  `report.md` incremental, section 0 inside ten tool calls; fallback name `DELIVERABLE_TEXT.md`; `PENDING.md` past
  half context). Pictures only under that folder.
- ANOTHER LANE (IMPOSTORFIX5) OWNS THE BUILD AND EXE SLOTS and edits `src/lodgen.cpp`, `src/impostor*`,
  `res/shaders/impostor_oct.*`, `tests/spells/impostor_draw.sh`. YOU DO NOT BUILD, DO NOT RUN `release/NifSkope.exe`
  or any rung, DO NOT run an exe-starting spell, and DO NOT EDIT any existing `src/`, shader, `NifSkope.pro` or
  `tests/spells` file in place. Deliverable: NEW files where new files serve + ONE refusing hook-up script per skill
  `ww-anchored-hookup` (exact-once anchors with the file's real line ending, `--check` writes nothing, CR delta
  asserted, the already-applied marker is NOT the anchor). Run `--check` last thing and quote it. Keep anchors out of
  `src/lodgen.cpp`.
- Read first: `CONSTITUTION.md`; HANDOFF top block; `scratchpad/cellview3_20260919/report.md` (its VTXT costing, the
  24 bare quads, the black shape, `dump_downtown.txt`); `src/cellground.*`, `src/cellview.*`; xEdit
  `wbDefinitionsFO4.pas` LAND/ATXT/VTXT; skills `ww-anchored-hookup`, `ww-simulate-before-build`,
  `ww-independent-placement-check`, `ww-test-harness-add`.

## The work
1. GROUND BLENDING. Today each 1/32 quad takes ONE texture, so the ground is a hard-edged mosaic. The game blends up
   to N layers per quadrant by the VTXT per-vertex opacity (17x17 per quadrant). Design and write it: the data path
   (what `cellground` already reads vs must read), the mesh/texture representation (per-quadrant splat: base + layers
   with per-vertex weights; say how many texture units / passes and what NifSkope's existing draw path allows), and
   the vertex budget effect against the 12M refusal cap. SIMULATE FIRST in python: from the plugin's LAND record for
   Sanctuary -20,7 build the blended ground top-down with the real diffuse textures (loose files under
   `E:/Tools/Fallout 4/DataUnpacked/Data/textures`), next to today's mosaic rebuilt the same way -- that picture is
   the target the build lane must match, and is the known-answer for a gate row (mean colour difference per quad).
2. THE 24 STILL-BARE QUADS: from the record bytes, what do they carry (no BTXT, no ATXT at all? opacity all under the
   threshold?) and what does the game draw there -- quote the definition / a vanilla example; propose the rule.
3. THE SOLID-BLACK SHAPE DOWNTOWN (right of centre in `scratchpad/cellview3_20260919/images/after_downtown.png`):
   from `dump_downtown.txt` + the NIF and its material files, name the reference, model, shape and material, and
   the likeliest cause (effect material with no base texture? vertex colour 0? alpha property? an untextured decal?)
   with the file facts that support it. Propose the repair; include it in the script only if the facts are decisive.
4. `PENDING.md` = exact director commands: `--check`, `--apply`, build, gates, the pictures to shoot.

## Rules
Simulations are labelled SIMULATIONS. No "fixed/final/true" -- mechanism + refuter. No toggle for a repair. Plain words.

## Report
The target picture path; the hook-up script + `--check` output; gate row text (NOT RUN); WW_CHANGES + HANDOFF text;
MISTAKES appended to root MISTAKES.md (top CRLF, byte splice, CR before/after); skill text to both trees if earned.
END with `BUILD PENDING`. Final message under 200 words.
