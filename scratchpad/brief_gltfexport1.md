# Lane GLTFEXPORT1 -- the glTF exporter becomes usable for characters + clips, and the viewer gets the body-build triangle

## Header
- Tree `E:/Projects/NifskopeWildWastelandEdition`, main. `date` before every timestamp. Never commit, never
  `git stash`, never edit `WW_CHANGES.md` / `HANDOFF.md`. Folder `scratchpad/gltfexport1_20260919/` (`BUILDING` first,
  `DONE` last, first word `gltfexport1`; `report.md` incremental, section 0 within ten tool calls; `PENDING.md` past
  half your context).
- BUILD SLOT: lane HORIZONOUT is finishing (uncommitted edits across src: lodofile/lodifile/esmdata/nativeemit/
  lodinative/nifcli ... -- never revert, reformat or tidy a hunk you did not write; you should not need those files).
  Until the director tells you HORIZONOUT has landed you do PHASE A: read, design, write code, `g++ -fsyntax-only`
  (see `scratchpad/horizon3_20260919/syn.sh` for the include flags); NO make, NO exe run. Headless BLENDER and Python
  are free to run at any time. If you reach the end of phase A first, write `PENDING.md` headed `BUILD PENDING` and
  stop. The director resumes you; then you own the build slot (the next lane, IMPOSTORSHOW, waits for you).
- Phase B rules: `tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` as ITS OWN command before every build and
  exe run; Fallout4 up = no build, no exe run, never wait-loop. One NifSkope at a time, `--port <unused>` +
  `WW_WINDOW_AT=1960,40`, absolute `E:/...` paths; light `-no-gui` CLI conversions are fine. A NifSkope with no
  `--port` is bungo's: never kill, rename the exe aside at link time. Wedged harness instance: `NifSkope::open
  <scene>` UTF-16LE UDP to its port. Build MSYS2 UCRT64 per skill `nifskope-ww-build-verify`; rung ONCE
  `release/NifSkope.before_gltfexport1.exe`; never delete rungs.
- Read first: `CONSTITUTION.md`; HANDOFF top block -- the `SPEC 04:xx 2026-09-19 GLTFCLIP1 grows into GLTFEXPORT1`
  line IN FULL (nine findings from a live Blender session) and the GLTFCLIP1 landing it follows; `MISTAKES.md` top 20;
  the existing glTF export/import code + the hkx reader/writer + the `gltf-export` / `gltf-import` CLI verbs; the
  director's workaround scripts (they are the behaviour to absorb, then retire):
  `E:/Projects/Claude/tools/gltf_units/` (gltf_to_game_units.py, gltf_merge_skin.py, gltf_unify_skin.py,
  bake_empties_to_armature.py, gltf_bake_bind.py, _check_import.py = the headless Blender 4.5 importer at
  `E:/Tools/3D/Blender 4.5/blender.exe`) and `E:/Projects/Claude/tools/body_build_anim/` (dump_race_bones.py,
  race_bone_data.json, build_build_cycle.py, blender_gate.py, verify_hkx.py) + `C:/Users/bungo/Downloads/
  HumanMale_BuildCycle_README.txt`. Skills: `nifskope-ww-panel-style`, `ww-test-harness-add`,
  `nifskope-ww-build-verify`, `nifskope-ww-render-shot`, `ww-contract-provenance`. Blender is the design reference
  where a choice is open; state divergences.

## bungo's words
- 2026-09-19 04:xx: "when we export gltf, we need a few toggles in there before the export. One would be to only export
  actual bones on the mesh for the visible character."
- 2026-09-19 05:3x: "did you build the other stuff for nifskope? the exporter? the skin preview?" -- 08:2x: "whole cell
  viewers waits, gltf, body build is first".
- Body build, 05:0x: "Fallout 4 has 'skin' bones, those are not used for animations, but only for the character's
  build? ... could you build a hkx animation that basically is the build slider ... thin to muscular to fat to thin".

## Part 1 -- the exporter
An export OPTIONS DIALOG on File > Export > glTF (flat Name | Value, label + control, no description text, palette from
skinVars only) and a CLI flag for every option on `gltf-export`; the dialog and the CLI drive ONE options struct.
DEFAULTS: bungo has NOT ruled the toggle defaults. Every option therefore defaults to the value that reproduces
TODAY's output byte-for-byte (gate row: no-flag export of three fixture NIFs is byte-identical to the rung exe's),
except item (1) below which is a defect repair, not an option. Deliver a ROWS FOR BUNGO table: option, what it does
in one plain sentence, today's value, your recommended default.
 1. The menu export carries the clip that is loaded/playing in the viewer (a clip provider is registered; no clip
    loaded -> no animation, as today). Repair, no toggle.
 2. `skeleton`: none (today) | path to a skeleton.nif (auto-found beside the character's `CharacterAssets` when the
    option is "auto"). The hierarchy comes from the skeleton; the body's flat `*_skin` bone list is hung on it so a
    clip drives every track it has (today 13 of 95).
 3. `joints`: weighted bones only (today) | the whole skeleton as ONE skin / one joint list per character, so Blender
    builds one armature and no loose empties. Inverse bind matrices correct for bones that carry no weights.
 4. `units`: metres (today) | game units (x69.9913 -- quote where the constant lives) -- PyNifly-compatible.
 5. `parts`: several NIFs (body, hands, head, rear head, eyes, mouth ...) exported onto one skeleton into one file:
    CLI takes a list; the dialog an "add part" list. One skin per part, all sharing the joint list.
 6. `bones` (HIS toggle): body bones only | + helpers (Camera, Camera Control, CamTarget, CamTargetParent,
    CharacterBumper, AnimObject*, Weapon* ...). Give the exact name rule and the count each way on the human skeleton.
 7. `textures`: reference only (today) | copy the referenced .dds beside the file (relative URIs) | convert to PNG if
    a converter already exists in the tree (else leave that choice out and say so).
 8. `root motion`: keep on the root bone | bake onto the armature object | strip. Say what today does.
 9. `gltf-import` accepts a game-unit file back (detects or is told the unit) and round-trips: NIF -> glTF (game
    units, whole skeleton) -> NIF vertex positions within 1e-4 u and the clip's tracks within the hkx quantisation.
 10. SCALE tracks: the hkx writer carries non-uniform scale (the body-build clip proved it); the exporter must write
    glTF scale channels and the importer read them -- gate it with Part 2's clip.
Gate `tests/spells/gltf_export_options.sh`: the byte-identity row; for each option a measured row (joint count, track
count driven 95/95, unit scale of a known bone length, part count, texture files present, root track location); the
HEADLESS BLENDER row: import the character + clip export, assert exactly one armature, zero loose empties, the mesh
deforms (a named vertex moves between frame 0 and frame N by the expected amount); a red control per class (wrong
skeleton -> the gate names the unmatched bones). The glTF validator if one is on the machine; else say none.

## Part 2 -- body build in the viewer (the "skin preview")
The character-creator build triangle, live in NifSkope, from the game's own numbers.
- Data: RACE `Bone Scale Data` (BSMP gender, BSMB bone, BSMS 36 B = thin / muscular / fat xyz; BMMP range modifiers)
  read through `esmdata` from the loaded data root's Fallout4.esm (HumanRace 00013746 default; any RACE selectable;
  male/female). Layout authority = xEdit wbDefinitionsFO4.pas wbBoneDataItem; cross-check your parse against
  `race_bone_data.json` (48 bones per gender, X always 1.0).
- UI: a dock panel "Body Build" (panel-style rules): race, gender, a TRIANGLE control (thin / muscular / fat corners,
  barycentric weights -- the game's own control) plus three numeric fields; applies the weighted bone scales to the
  `_skin` bones of the loaded character live. State and quote (Todd's treat first if it is a vanilla-behaviour question:
  the Todd's treat tooling (kept outside this repo)) how the game combines the three corners
  (weighted sum of the three scale vectors vs lerp from 1.0) -- do not guess; if unproven say UNPROVEN and give the
  refuter.
- Works with an animation playing (scale applied on top of the clip's pose on `_skin` bones only).
- Requires the skeleton.nif hierarchy (the `_skin` bones live only in skeleton.nif, not skeleton.hkx) -- reuse
  Part 1 item 2's loader.
- Export: the current build is baked into a glTF export as bone scale (static) and, as an option, the director's
  6-second THIN -> MUSCULAR -> FAT -> THIN cycle as a clip (absorbs build_build_cycle.py). An `.hkx` of that clip is
  NOT game-valid (skeleton.hkx lacks the `_skin` bones) -- never offer it as one.
- Harness `WW_BODY_BUILD=<race>|<gender>|<t>,<m>,<f>` + gate `tests/spells/body_build.sh`: bone scales applied ==
  the Python table at the three corners and the centroid; a named belly vertex moves outward thin -> fat by the
  predicted amount; red control (female table on the male body -> the gate names the differing bones). Pictures:
  thin / muscular / fat / centroid, front + side, male and female, one contact sheet.
- It is a viewer panel (preview), not a behaviour change to files: nothing is written unless he exports.

## Rules
An owed ruling never ships as a default. No "fixed/final/true": mechanism + refuter. Every number from a named log.
Harnesses force the state they measure (never inherit QSettings).

## Report
0 state at launch; 1 what exists today (file:line) vs the nine findings; 2 the options struct + dialog + CLI;
3 Part 2 design + the corner-combination evidence; 4 code (files, lines; any shared/modified file touched); 5 gates;
6 PENDING or build (mtime, size, sha1, `find src tests res -newer` empty, object-vs-header); 7 pictures + the Blender
gate log; 8 ROWS FOR BUNGO (defaults table); 9 WW_CHANGES + HANDOFF text for the director; 10 MISTAKES; 11 skills:
a new skill `nifskope-ww-gltf-character-export` (the procedure: NIF parts + skeleton + hkx -> glTF -> Blender, and
back) and `fo4-body-build-bone-scales` (the RACE data) -- text delivered for the director to sync to both skill
trees and `E:/Tools/AISkills`. END with `DONE` (or `BUILD PENDING`) + five plain sentences for bungo, the last one
about restarting his open window. Final message under 300 words.
