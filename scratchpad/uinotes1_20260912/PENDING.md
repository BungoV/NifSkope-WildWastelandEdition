# PENDING -- lane UINOTES1 (written 03:31 for a restart that did not come;
# brought up to date 04:05. ONE thing is left: the build, and the gates and
# pictures that need it. Everything else in the lane is in.)

Tree: **E:/Projects/NifskopeWWE_ui** only. Report: `scratchpad/lane_uinotes1_report.md`.
Brief: `scratchpad/brief_uinotes1.md` (work order), rulings verbatim in
`scratchpad/brief_uinotes_20260912.md`.

## 0. The charter, unchanged (read before touching anything)

- Never open, edit, build or run anything under `E:/Projects/NifskopeWildWastelandEdition`
  (lane TILING4 works there). Never commit, never `git stash`, in either tree.
- Never touch bungo's installed game files. Never rename or kill the NifSkope window that has
  no `--port` (pid 60820 at 01:29 -- his).
- Stay out of TILING4's files: `src/lodgen.*`, `src/nifcli.cpp`, `src/lodgenmanager.cpp`,
  `src/btdterrain.*`, `src/lodtfile.*`, `docs/LODGEN_*`, `tests/spells/lodgen_*`,
  `tools/bake_impostor_cards.sh`. If a ruling needs one: stop that item, write it under Owed.
- Before EVERY GUI launch: `tasklist` for a NifSkope started from the MAIN tree with `--port`
  (TILING4's). If one is up, wait. Never two GUI instances. Own an unused `--port` below 49152.
  Second monitor via `WW_WINDOW_AT`.
- No builds while `Fallout4.exe` runs.
- Line endings: Python byte counts only, never grep. Mixed files spliced in binary.
- No descriptions/blurbs in panels or menus (label + control only). Tooltips allowed and must
  name the action and its shortcut.
- New colours are ADDED as named wwskin tokens with Blender 4.5's own values, read from the
  installed Blender and cited.
- Plain language; every number beside its floor; no "final/true" claims; write to disk after
  every step; keep this file and `CHANGED_FILES.txt` current after EVERY item from here on.

## 1. Where the work stands

| Step | Items | State |
| --- | --- | --- |
| 1 | 1 -- the bottom status bar goes | DONE, gates (s1) (s2) + floor in `src/uialigntest.cpp` |
| 2 | 2, 7b, 8 -- the drawing | DONE, gates in `animws.sh` / `animworkspacetest.cpp` |
| 3 | 9 -- start/end grips + Start/End boxes | DONE, gate (l) |
| 4 | 7, 7a -- right-click at the clicked frame, the zoom reset | DONE, gate (m) |
| 5 | 3 -- Remove transform axes... | DONE, gate (n), report section 5 |
| 6 | 5 -- Animations list shortcuts / menu / drag-and-drop | DONE, gate (o), report section 6 |
| 7 | 6, 6a -- retire the button row + the Keys block; header menus + right-side panel | **DONE** -- code 03:37-03:38, gate (p) 03:42, `animws.sh` line 03:42, report section 7 rewritten to fact 03:47. Syntax-clean; never built |
| 8 | 4 -- transport icons + un-clipped "Load... / root" header | **DONE** -- code 03:46-03:52, gate (q) + `animws.sh` line 03:55-03:58, report section 8 at 03:59. Syntax-clean; never built |
| 9 | build + harness chain | **BLOCKED, not failed.** `qmake NifSkope.pro` DONE 04:00 (it had to be: the robocopied Makefiles pointed at the OTHER tree). `release/NifSkope.before_uinotes1.exe` rung aside 04:00. The build itself is PENDING: `Fallout4.exe` pid 22908 has been up since 03:48:37 and no build runs while the game runs. Exact resume in report section 10 |
| 10 | pictures before/after | OWED, `images/` still empty: both halves need a running NifSkope and no GUI was launched this lane. Gate (q) saves the 1:1 and 2:1 transport pictures itself once it can run |
| 11 | documents | **DONE 04:02-04:04**: `WW_CHANGES_ENTRY.md` (starts `## 2026-09-12 —`), `HANDOFF_BLOCK.md`, `MISTAKES_ENTRIES.md` (four entries), report sections 9-15 |
| 12 | `CHANGED_FILES.txt` | current as of 04:05, after every step |

**Nothing is mid-edit.** Every file on disk is syntactically whole. Step 7's CODE went in at
03:37-03:38 (file mtimes) as one atomic script (`scratchpad/edit_step7.py` in the session scratchpad; all four files
written only after every anchor matched) and all five files that include `animworkspace.h`
(`animworkspace.cpp`, `animworkspacetest.cpp`, `hkxanimuitest.cpp`, `nifskope.cpp`,
`nifskope_ui.cpp`) pass `g++ -fsyntax-only` with zero errors, the checker having been proved able
to fail on the same run. Step 7 owes nothing further: gate (p) went into
`src/animworkspacetest.cpp` and its WHAT IS MEASURED entry into `tests/spells/animws.sh` at 03:42,
and report section `## 7` was rewritten from placeholder to fact at 03:47.

**A timestamp in the 03:31 version of this file was wrong.** It said step 7's code went in at
03:52; `date` reads 03:45:41 and the files' mtimes are 03:37-03:38. I typed 03:52 from a feeling
of elapsed time instead of reading the clock, which is exactly the rule "never type a timestamp
from elapsed-time feel". Corrected here and logged in `MISTAKES_ENTRIES.md`.

## 2. Step 7 (items 6 and 6a) -- WHAT IS ON DISK (do not redo it)

Ruling, verbatim in `brief_uinotes_20260912.md` lines 69-93. Every numbered point below is
IMPLEMENTED and syntax-checked unless it says otherwise; the anchors it describes are gone from
the files, so re-running the old script would assert, not damage anything.

1. **The fifteen `btn*` QPushButtons of `buildActionBar()` become QActions** with the SAME object
   names (`AnimWsInsertKey`, `AnimWsDeleteKeys`, `AnimWsReduce`, `AnimWsAddAnnot`,
   `AnimWsRenameAnnot`, `AnimWsTrim`, `AnimWsRetime`, `AnimWsBake`, `AnimWsUnbake`,
   `AnimWsRemoveTrack`, `AnimWsRenameTrack`, `AnimWsAddFloat`, `AnimWsSetFloat`, `AnimWsSave`,
   `AnimWsSaveAs`, `AnimWsDeleteAnnot`) and the same texts and tooltips. Members rename
   `btn*` -> `act*` (type `QAction *`), which forces every use to be found.
   `buildActionBar()` -> `buildActions()` (no widget) + `buildMenuBar()`.
2. **A header row** `AnimWsHeader` above the transport row, holding a `QMenuBar` `AnimWsMenuBar`
   (`setNativeMenuBar(false)`, each menu `setToolTipsVisible(true)`) and, at its right end, the
   side-panel toggle `AnimWsSidePanelBtn` (checkable QToolButton).
   Menus: **Clip** (Trim to range, Retime, | Bake root, Unbake, | Save, Save as...),
   **Key** (Insert key, Delete, | Reduce), **Channel** (Rename track, Remove track,
   Remove transform axes..., | Add float track, Set float key),
   **Marker** (Add annotation, Rename, Delete annotation), **View** (Side panel (N), | Frame all).
   `Remove transform axes...` needs a new action `AnimWsRemoveAxes` -> `removeTransformAxesDialog()`
   (enabled when a clip and a track are selected); the sheet's context menu keeps its own entry.
   `Frame all` -> `sheet->frameAll()` (public, `animdopesheet.h:147`).
3. **The right-side panel**: `sidePanel` (`AnimWsSidePanel`) becomes the THIRD pane of the existing
   `AnimWsSplitter`; `buildSettings()`'s scroll area (`AnimWsSettings`, keep that name -- gate (j)
   reads it) moves out of the left column into it. Width remembered in QSettings
   `AnimWorkspace/sidePanelWidth` (default 260, min 160) plus `AnimWorkspace/sidePanelOpen`;
   saved on `splitterMoved` and on close. `setSidePanelOpen( bool )` keeps the toggle button and
   the View action in step with a QSignalBlocker.
4. **The N key**: two QShortcuts, one on `sheet` and one on `list`, context
   `Qt::WidgetWithChildrenShortcut`, names `AnimWsSidePanelKey` / `AnimWsSidePanelKey2`. NOT on the
   workspace itself, or the letter n could not be typed into the annotation Name field. That
   divergence from Blender goes in the report.
5. **The Keys block goes**: `Pose with the gizmo` and `Auto-key gizmo transforms` become checkable
   QToolButtons in the transport row (same object names `AnimWsPose`, `AnimWsAutoKey`; the member
   type changes from `QCheckBox *` to `QToolButton *`; `isChecked()/setChecked()` uses at
   animworkspace.cpp 1204, 1205, 1621, 1634 keep working unchanged).
6. **Sections, one per selected kind**, all inside the existing `editSection` (so gate (i)'s
   "the edit rows are hidden for a NIF sequence" keeps working):
   `AnimWsClipSection` (Root motion track, Retime to) -- shown whenever a clip is selected;
   `AnimWsKeySection` (Reduce tolerance, Reduce rotation) -- keys selected;
   `AnimWsAnnotationSection` (Name) -- an annotation selected;
   `AnimWsTrackSection` (Bone, a QLineEdit committing a rename on editingFinished) -- a bone row;
   `AnimWsFloatSection` (Value) -- a float row.
   Exclusive by priority marker > keys > float row > track row, with Clip always on for a clip.
   New method `updateSections()`, called from `refreshSummary()`.
7. **`refreshSummary()` at animworkspace.cpp 1750-1764**: every `btn*->setEnabled` becomes
   `act*->setEnabled`, plus `actRemoveAxes`, plus a call to `updateSections()`.
8. **Harness repointing (must not be forgotten):**
   - `src/animworkspacetest.cpp:370` `widget<QPushButton>( ws, "AnimWsInsertKey" )` -> the QAction
     (`ws->findChild<QAction *>(...)`; the `widget<T>` helper at line 157 is just `findChild<T*>`
     and works for QAction too).
   - `src/animworkspacetest.cpp:1126` `AnimWsUnbake` / `AnimWsBake` enabled state -> QActions.
   - `src/animworkspacetest.cpp:1748` and `src/hkxanimuitest.cpp:370` look up `AnimWsActionBar`,
     which stops existing -- repoint both to `AnimWsMenuBar` (same question: is it pinned outside
     the splitter).
   - gate (j) floor "buttons >= 20": fifteen push buttons leave the dock. COUNT the QAbstractButtons
     after the change and lower that floor to what is actually there, saying so.
9. **Gate (p) -- WRITTEN at 03:42** (this paragraph is the specification it was written to, kept
   so the merger can check the gate against the intent). It had to match the ruling's own words: the dock's fixed chrome height
   (transport + header + note) before/after -- take the BEFORE number from the old exe run
   (`release/NifSkope.before_uinotes1.exe`, step 9) or from the harness's own arithmetic;
   one section visible per selection kind and none of the other four; every former button's text
   found in a menu (walk `AnimWsMenuBar->actions()` -> `a->menu()->actions()` -> texts) -- with the
   floor that a made-up text is NOT found. Document (p) in `tests/spells/animws.sh`'s
   WHAT IS MEASURED block.
10. Report section `## 7` -- REWRITTEN to fact at 03:47 (the "not typed yet" placeholder was
    honest for 03:31 and wrong from 03:38 on). Nothing owed.
11. `scratchpad/flags.rsp` now lives IN THE TREE (copied there at 03:37), so the syntax check
    below works after a restart.

## 3. How to syntax-check (the one tool that lied to me all lane -- see MISTAKES_ENTRIES.md)

```
export PATH="/c/msys64/ucrt64/bin:$PATH"
cd /e/Projects/NifskopeWWE_ui
g++ -fsyntax-only @scratchpad/flags.rsp src/animworkspace.cpp
```
`scratchpad/flags.rsp` already exists (CXXFLAGS minus `$(DEFINES)`, DEFINES, INCPATH out of
`Makefile.Release`, with any argument containing a quoted define re-wrapped in single quotes).
Exit codes are unreliable under the sandbox -- judge by OUTPUT only. **Prove the checker still
fails**: put `return notAThing;` in a file, see the error, take it out. A good file prints nothing.
If `scratchpad/flags.rsp` is missing after the restart, rebuild it from `Makefile.Release`; if
`Makefile.Release` is missing, `qmake NifSkope.pro` writes it (that is step 9's first act anyway).

## 4. Steps 8-12, in order, unchanged

8. Item 4: Blender's transport icon set as our own SVGs, one weight and size, tooltips naming the
   action and its shortcut; the "Load... / root" header must stop clipping.
9. Build: `qmake NifSkope.pro` FIRST, then check `tasklist` for `Fallout4.exe` and for TILING4's
   `--port` NifSkope, ring the baseline aside ONCE as `release/NifSkope.before_uinotes1.exe`
   (baseline: `release/NifSkope.exe` 2026-09-11 23:26:29, 21,484,032 B), ONE build, count the
   relinks, then the harness chain: `animws.sh`, `hkxanim_ui.sh`, `ui_align.sh`, `water_ui.sh`,
   `files_tab.sh`, `top_bar.sh`, `skeleton_overlay.sh` -- naming every skip with its reason.
10. Pictures before/after per item into `scratchpad/uinotes1_20260912/images/` (before = the
    23:26:29 exe) plus a 1:1 dock overview.
11. Documents in `scratchpad/uinotes1_20260912/`: `WW_CHANGES_ENTRY.md` starting
    `## 2026-09-12 - <title>` with a real em dash, `HANDOFF_BLOCK.md`, `MISTAKES_ENTRIES.md`
    (must include the syntax-checker mistake -- it is already in there), and the report's sections
    `## 0. Rulings and gates`, `## 1`-`## 9`, `## 10. Build and chain`, `## 11. Pictures`,
    `## 12. MERGE LIST`, `## 13. Owed / red / bungo's calls`, `## 14. Mistakes`,
    `## 15. Finished-work skill review`.
12. `CHANGED_FILES.txt` final pass, then the DONE marker.

## 5. Owed / open questions for bungo (carry these into section 13)

- Item 3: the reference value is FRAME 0, not zero -- a stripped axis stops moving instead of
  teleporting to the origin. His call whether that is what he wanted.
- Item 5: should Rename also rewrite the clip's internal name when it is saved? Should Move
  up/down ever reorder the NIF's own sequence BLOCKS (it refuses today -- that is Blocks-tab work)?
- Item 6b: he sent the Material Manager dock with no words; I read it as the reference for a
  right-edge panel. Its footer help line breaks his own no-descriptions rule and its empty preview
  pane eats half the height -- both named in the report, neither copied.
- Item 6, second bullet: "Ctrl+C/V, Shift+D on keys, G to move, box select" for KEYS (not clips)
  is a new document operation (a key clipboard) that step 7 does not include. Owed, in writing.
- No build has run this lane, so nothing here has been proven to compile beyond
  `g++ -fsyntax-only`, and no gate number exists yet. Say exactly that to him.
