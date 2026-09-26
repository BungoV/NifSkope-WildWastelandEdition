# FLAT1 -- text for the overseer to splice (the lane wrote none of the ledgers)

## HANDOFF.md (top block entry)
**FLAT1 (2026-09-26), branch flat1-20260926 (head: see DONE.md section 5), NOT merged, NOT installed.**
The terrain colour now paints flat ground objects (pads, railway track, paths, decals, flat debris) beside the
road stamp. The rule is measured from each placed mesh: on the ground (median underside <= 16), top between -8
and 64, standing sides / top <= 0.35, not under water. It reads no kinds and no paths.

- Default ON; `--no-flat-objects`, or the panel row *Paint flat ground objects*, turns it off.
- User override file: `lodgen_flat_objects.txt` beside NifSkope.exe, with `bake`/`nobake` lines. It ships
  header-only, and `--flat-objects-file` names another file.
- Each bake writes `<ws>.flat_objects_report.txt` beside the VT sheets.
- Boston box: 10,834 of 144,655 placements painted, 730,420 texels.
- Confinement: 0 changed texels outside the 4x4 blocks, and the mask moves only there.
- Faithfulness per kind: 2.6-3.4 levels, against 9-35 for the off bake.
- Override: nobake reverts exactly, and bake paints a refused model.
- Owed: bungo's look at pics/oblique_*.png and the close-ups; a merge; a real install bake; his eye in game.
- Owed: one ordinary GUI start of the merged exe. The FLAT1 run copy exited at GUI start (rc 0, no output) on
  23 of 24 picture tries, while the pre-FLAT1 rung started. FLAT1 changes nothing on the start path; the
  suspect is the new exe being refused its UDP port. Not proven.

## WW_CHANGES.md
- **Flat ground objects in the far terrain** (lane FLAT1). Slabs, floors, railway track, foot paths, trash
  decals, leaf piles and flat rubble now show on the distant terrain in the colour they wear in game. Standing
  things (fences, guard rails, jersey barriers, cars, benches, walls) stay out.
  - Your own list of exceptions lives in `lodgen_flat_objects.txt` beside NifSkope: `bake <path>` or
    `nobake <path>`, one per line, a model or a folder.
  - Every bake leaves a report beside its output: one line per model, saying painted or why not.
  - Panel: *Paint flat ground objects* (Roads section, on). Command line: `--no-flat-objects`,
    `--flat-objects-file <file>`.

## MISTAKES.md (newest at the top)
- **2026-09-26 FLAT1: a patch script wrote literal TAB characters into two C++ string literals.** The Python
  escape was meant to be `\t` in the source, not a TAB byte. The compiler accepted it, and the report still
  looked tab-separated. Found by reading the diff; fixed in patch 6. Rule: after a patch script, grep the diff
  for TAB bytes inside quotes.
- **2026-09-26 FLAT1: a patch went through a bash heredoc again, and the backslashes were halved.** The anchor
  failed, so nothing was written wrongly. The standing rule (skill nifskope-ww-lodgen, editing traps) already
  says: no backslash or apostrophe through a heredoc. Patch scripts go through the Write tool.
- **2026-09-26 FLAT1: the report and the census disagreed on the override count (141,603 vs 141,397).** A model
  that would not load still carried the override that matched its path. Fixed: an unloadable model carries
  none. Rule: a decision that is refused before measurement takes no override.
- **2026-09-26 FLAT1: two bakes were spent on `--land-shade 0` as a "different" bake.** That is already the
  value in the VT bake mode (the log says `landShade 0.000`), so the bakes were byte-identical to the default.
  Rule: read the census line for the switch's current value before baking a variant of it.
