# Lane UINOTES1 -- bungo's nine animation-workspace rulings of 2026-09-12 01:3x-02:0x, built in a SEPARATE COPY of the tree, merged by the director

## Header
- Tree: `E:/Projects/NifskopeWWE_ui` -- a robocopy of `E:/Projects/NifskopeWildWastelandEdition` taken 2026-09-12
  01:53 (everything but scratchpad/, of which only the *.md and *.py files came along). YOU WORK ONLY IN THE COPY.
  Never open, edit, build or run anything under `E:/Projects/NifskopeWildWastelandEdition`; lane TILING4 is running
  there at the same time (bungo's ruling 02:05: "you can run a second agent on it then merge"). Never commit, never
  `git stash`, in either tree. Never bungo's installed game files.
- Exe in the copy at launch: `release/NifSkope.exe` 2026-09-11 23:26:29, 21,484,032 B. Rung ONCE before your first
  build: `release/NifSkope.before_uinotes1.exe`. Markers `scratchpad/uinotes1_20260912/BUILDING` / `DONE` (in the
  copy). Report `scratchpad/lane_uinotes1_report.md` (in the copy), incremental, PENDING.md past half context.
- The rulings: `scratchpad/brief_uinotes_20260912.md` -- his words verbatim, items 1-9 with 6a, 6b, 7a, 7b. Every item
  is a ruling; the six "timeline guesses" in it are NOT (he did not confirm them; only what item 8 answers, guess 3,
  and the "Load... / root" header of item 4 are in scope).
- Read first: `CONSTITUTION.md`, HANDOFF.md top block (the DIRECTOR NOTE 01:4x and the UI NOTES paragraph), the
  rulings file, then the code: `src/animworkspace.cpp/.h`, `src/animdopesheet.cpp/.h`, `src/ui/widgets/timeline*.cpp`,
  `src/hkxanimui.cpp`, `src/hkxclipedit.cpp` (the document + undo), `src/wwskin.h` (the palette), `res/style.qss`,
  `src/ui/nifskope.ui` (the status bar), the harnesses `tests/spells/animws.sh`, `hkxanim_ui.sh`, `ui_align.sh`,
  `src/animworkspacetest.cpp`, `src/hkxanimuitest.cpp`, `src/uialigntest.cpp`, and `WW_CHANGES.md` entries for
  ANIMWS / HKX3 / BUILD9 / BUILD12 (how those harnesses and the workspace were built).
- Skills (copy's `.claude/skills`): `nifskope-ww-build-verify`, `nifskope-ww-resume-pending` (qmake before make in a
  moved tree), `ww-test-harness-add`, `ww-anchored-hookup`, `ww-texel-picture`, `ww-spec-gate-audit`, and the
  Blender-reference rule (uncertain design -> Blender's equivalent, state divergences).
- Palette: only `wwskin.h` tokens (the PBR Material Editor's skinVars). New colours (Blender's selection orange,
  the orange-red of secondary selection, the playhead blue, the out-of-range darkening) are ADDED as named tokens
  with Blender 4.5's own values: read them from the installed Blender's default theme (find blender.exe, then its
  `scripts/presets/interface_theme/` XMLs or the documented defaults) and cite the source; if you cannot find the
  file, use the documented defaults and say so.
- No descriptions/blurbs in panels or menus (label + control only); tooltips are allowed and must name the action and
  its shortcut.

## The work, in this order (each step: code, harness check, picture, report section, then the next)
1. Item 1: the status bar goes. The messages it carried get a home that is not a bar (a short-lived overlay line at
   the bottom of the viewport is acceptable; say what you chose). The two harnesses that read
   `statusBar()->currentMessage()` are repointed. Gate: window client height reclaimed = the bar's height, from
   `ui_align.sh`'s geometry dump, before/after.
2. Items 2, 7b, 8: the drawing. Selected key/annotation active = orange, other selected = orange-red, unselected =
   plain; playhead = blue box on the ruler + blue line; outside the clip range darkened, inside unchanged. Gate: pixel
   samples asserted by the harness (three distinct colours for the selection states; ruler/inside/outside).
3. Item 9: draggable start/end grips on the ruler + Start/End boxes in the transport row; replaces the Trim controls.
   Gate: +10/-10 frame drags and box edits assert frame count, darkened-zone edge, HKX duration on save.
4. Items 7, 7a: right-click on any row or the ruler -> "Add annotation at frame N" (clicked frame, inline name
   editor); on a marker -> Rename / Remove; Insert key uses the clicked frame; M / Ctrl+M. Find and fix the
   zoom-reset on marker drag (measure it first: harness zooms, drags, asserts the visible range).
5. Item 3: "Remove transform axes..." under Remove track: six checkboxes (translation X/Y/Z, rotation X/Y/Z), chosen
   components set to the track's frame-0 value on every key (state the Euler convention in the dialog), undoable.
   Gate on his fixture: COM of Running_To_Slide_And_Back_To_Running on fixtures/human_male_vanilla.nif: after
   stripping X and Y, COM X/Y constant over 93 keys, Z byte-identical, HKX round-trips.
6. Item 5: the Animations list: Delete/X, Ctrl+C/V/X, Shift+D, F2/double-click, Ctrl+A; the same in a right-click menu
   plus Move up/down; drag-and-drop reorder with a drop line; multi-select. Gate: every shortcut and menu entry driven
   on a three-clip fixture, row order/count/names asserted after each.
7. Items 6, 6a: the button row and the Keys block go; a panel opens from the RIGHT edge of the Animation dock (header
   toggle + N key), collapsible sections shown only for the selected kind (Clip incl. root motion bake/unbake and
   Save/Save as, Key incl. reduce, Annotation, Track, Float tracks); header menus (Key, Channel, Marker, View) hold
   every former button's action; Pose-with-gizmo and Auto-key become transport-row toggles. Gate: dock chrome height
   before/after; one section per selection kind; every former button's action found in a menu by text.
8. Item 4: the transport icons: Blender's set, one weight and size, our own SVG in the palette, tooltips with
   shortcuts; loop and speed recognisable; "Load..." / "root" header no longer clipped. Gate: picture at 1:1 and 2:1,
   every tooltip asserted.
9. Build: `qmake NifSkope.pro` FIRST (the tree moved), then one build + counted relinks; game check before the link.
   GUI harnesses: second monitor (`WW_WINDOW_AT` via `tests/spells/_harness.sh`), your own unused `--port`, and
   before EVERY GUI launch check `tasklist` for a NifSkope started from the MAIN tree with `--port` (TILING4's) --
   if one is up, wait for it to end; never two GUI instances at once; never touch bungo's own window (no `--port`,
   his, pid 60820 at 01:29 -- it runs the MAIN tree's exe, so nothing of yours collides with it, and you never rename
   or kill it). Chain: `animws.sh`, `hkxanim_ui.sh`, `ui_align.sh`, `water_ui.sh` (shares the dock area),
   `files_tab.sh`, `top_bar.sh`, `skeleton_overlay.sh`; name the skipped ones with the reason.
10. Pictures: one before/after per item (before = the 23:26:29 exe via the sibling spell that already photographs the
    region; after = yours), `scratchpad/uinotes1_20260912/images/`, and one overview of the finished dock at 1:1.
11. Documents in `scratchpad/uinotes1_20260912/`: `WW_CHANGES_ENTRY.md` (starts with a `## 2026-09-12 — <title>` line,
    em dash), `HANDOFF_BLOCK.md`, `MISTAKES_ENTRIES.md` (`## ` entries), the report with sections `## 0. Rulings and
    gates`, `## 1-9` one per work step, `## 10. Build and chain`, `## 11. Pictures`, `## 12. MERGE LIST`, `## 13. Owed /
    red / bungo's calls`, `## 14. Mistakes`, `## 15. Finished-work skill review`. Skills you add or amend go in the
    copy's `.claude/skills`; list them in the merge list.

## The MERGE LIST (section 12) is the deliverable the director merges by
- `scratchpad/uinotes1_20260912/CHANGED_FILES.txt`: every file you created or changed under src/, res/, tests/,
  tools/, docs/, .claude/skills/, one path per line, relative to the tree, with `A` or `M` in front. Produce it from
  `find ... -newer scratchpad/uinotes1_20260912/BUILDING` and check it against your own notes; a file you touched and
  did not list is a merge that will be lost.
- For each file also the byte counts of CR and LF before and after (the main tree's copy must be spliced at the same
  line endings; `src/nifskope.cpp` is mixed CRLF, `src/nifskope_ui.cpp` LF-only, `WW_CHANGES.md` is never edited by
  you -- entry text only).
- Stay OUT of these files (TILING4 owns them; a change there cannot be merged): `src/lodgen.cpp`, `src/lodgen.h`,
  `src/nifcli.cpp`, `src/lodgenmanager.cpp`, `src/btdterrain.*`, `src/lodtfile.*`, `docs/LODGEN_*`, `tests/spells/lodgen_*`,
  `tools/bake_impostor_cards.sh`. If a ruling needs one of them, stop that item and write it under Owed.

## Rules
Plain language; every number beside its floor; no "final/true" claims; incremental writes; the item order above is
the priority order -- if context runs out, PENDING.md with the exact resume steps and a DONE-so-far list, and the
merge list for what is finished. No new rows in menus beyond the rulings; no tone or renderer changes.
