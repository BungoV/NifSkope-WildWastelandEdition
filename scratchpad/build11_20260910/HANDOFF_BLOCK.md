<!-- Lane BUILD11, 2026-09-10. TEXT ONLY for the HANDOFF.md top block; the
     director splices it (CONSTITUTION 8). -->

**BUILD11 DONE 17:3x, EXE FREE (release/NifSkope.exe 17:08:39, 20,693,504 B;
scratchpad/build11_20260910/HANDOFF_BLOCK.md).** SKELFIX, HKXEDIT1 and HKXEDIT2
are built into one exe and gated. **RESTART: yes** -- bungo opened his own
window at 17:05:44 (pid 700, no `--port`), three minutes BEFORE this link; the
chain renamed the exe aside instead of killing it, he closed it at 17:16:19,
and the copy was then deleted. His next launch has all three lanes.

**The hook-ups.** SKELFIX applied exactly as predicted (+905 bytes, CR 0,
2 markers). HKXEDIT1's script died half-applied -- it asserts the CR count
UNCHANGED and `src/nifskope.cpp` is CRLF, so inserting CRLF text must raise it;
finished by a driver that imports its own EDITS table (nifskope.cpp +1,608,
CR +36 as expected). HKXEDIT2's script refuses in one pass by construction: its
tier-2 edit anchors on the `DEFINES += WW_HKXCLIP_CANON` line its own base edit
inserts, so the real count is **15 anchors in pass one and the 16th after**, not
the 16 its resume predicted; applied by a two-pass driver reusing its tables.
Then `sx_BUILD11.sh` caught five compile errors of ONE kind -- three hook-ups
inserted CALLS and no INCLUDES (`AnimWorkspace`, `QUndoGroup`, `HkxModel` all
still forward declarations); three lines added, marked `(lane BUILD11)`.
**HKXEDIT1's owed QUndoGroup was not written because HKXEDIT2's hook-up already
is it**: the window's Undo/Redo actions now come from `wwAnimUndoGroup()` and
`installUndoGroup( nif->undoStack, hkx->undoStack )` puts both stacks in it.

**Build hygiene:** qmake before make; `DEPCHECK missing=0`; the three WW defines
each once in `Makefile.Release` and on every compile line; the 7 objects of every
TU reading one of them deleted first (the BUILD9 DEFINES trap); style.qss in
step; `hkclasses_fo4.json` (1,511,662 B) and `hkx_annotation_vocabulary.txt`
(55,299 B) copied beside the exe; exe newer than **113 of 114** changed paths --
the exception is `src/watermark.cpp`, **lane WATER7's live edit in this shared
tree, which is NOT in this exe**.

**Gates.** `animws.sh` **57/0 + 1 skip, PASS on its first run ever** (78 bone
rows x 93 keys, ruler 60 fps, a 30-deg key at frame 46 reading 0 deg off in the
document AND on the viewport node, trim 41 frames, retime 47 with 0 coincident
frames differing, COM 487.643 -> 0 and unbake byte-identical, save re-read bit
for bit, undo/redo). `hkxfile_gates.py` **109/0** (15,320 clips) and
`hkxclipedit_gate.exe` **72/0** + HKXPACK 2/1, both re-derived here.
`hkxanim_play.sh` **27/0 PASS**. `skeleton_overlay.sh` in-app **27 checks** with
every SKELFIX prediction met: segments 110 / stubs 62 / skipped 38, armature
111 / marker-only 19, longest drawn 31.94 <= 33.60, old rule 300.51 red, 0
endpoints outside the bone box against the old rule's 19 at 241.30.
`files_tab.sh` 28/2 and `hkxanim_ui.sh` 48/1 -- BUILD9's same reds by name.

**FOUR REDS, ROUTED, NONE FIXED HERE:**
1. **The .hkx document does not stay in the Blocks tab.** `hkxmodel_test.sh` 3/1:
   it loads (the `ok` is `HkxModel::loadFromFile`'s own return) and
   `tree->setModel( hkx )` runs -- then `emit completeLoading` reaches
   `onLoadComplete` -> `swapModels()` (`src/nifskope.cpp:7478`), which knows only
   `nif` and `nifEmpty` and puts `nif` back. The `.kfm` route escapes it because
   a .kfm has its own view (`kfmtree`). Everything after gate (a) in that harness
   is unreached.
2. **Overlay gate (j) vs the overlay's own rule.** 53 stray pixels, and all 53
   are two grey joint markers 7 px across (colour 134,139,145, the "not a bone"
   colour) belonging to two of the 19 marker-only nodes SKELFIX's rule KEEPS by
   design. bungo's call: relax (j) to segments, or stop drawing a marker for a
   node that projects off the character.
3. **Overlay gates (c) and (e) are flaky.** Four identical runs on one exe:
   (c) 10/0/4/0 pixels outside the mask, (e) 17/0/37/2 after toggling off -- one
   run PASSed 27/0. Exact equality on a framebuffer that is not bit-stable.
4. **`animws.sh` gate (i) has never completed.** It SKIPs on the default
   `10mmPistol.nif` (no `NiControllerSequence`); given one that has one
   (`Meshes/Effects/TeleportInFXLight.nif`) the harness dies inside (i) and
   writes no summary. The NIF opens and renders fine alone.

**PICTURES.** `scratchpad/skeloverlay_20260910/on_frame46.png` **1500x1000** --
the human at frame 46 of the Mixamo clip with the overlay on: the skeleton sits
entirely on the body, spine to fingertips, and **the fanning segments the
director saw are gone**; the only off-body marks are the two dots of red 2.
Beside it `scratchpad/skelfix_20260910/before_on_frame46.png` (BUILD9's, kept --
it is gate (j')'s only floor). Also `off.png` / `on.png` / `off_frame46.png` /
`gates/gate_frame46_mask.png`, and
`scratchpad/hkxedit2_20260910/dock_frame46.png` + `viewport_gizmo.png`.

**WW_RENDER_SIZE, measured** (1000x1000 -> **1293x941**, 1500x1059 ->
**1500x1000**, 1800x800 -> **1800x741**): height is exactly `requested - 59`,
width is `max( requested, 1293 )` because the hook calls `resize()` and a
QMainWindow will not go under its minimumSizeHint. **The floor moves with the
build** -- 1437 on the 15:52:46 exe (SKELFIX's 1437x941), 1293 on this one -- so
it is measured per build. `nifskope-ww-render-shot` amended in BOTH trees.

**Documents:** WW_CHANGES.md gained HKXEDIT1's and SKELFIX's entries with the
measured status blocks and HKXEDIT2's was retitled from BUILD PENDING (CR 19,020
unchanged, asserted); MISTAKES.md gained nine entries in two batches; each of the
three lane reports gained a `## Build (BUILD11)` section. Nothing committed.
