# Lane HKXEDIT2 -- the ANIMATION layer: one animation workspace, keys over a dense clip (2026-09-10)

Repo `E:\Projects\NifskopeWildWastelandEdition`, main, **nothing committed**
(the brief). bungo's rulings, verbatim: *"Just make hkx fully editable in our
nifskope"*; *"Animation manager was one of the first features for nifskope,
and it's pretty old and outdated btw"* -> this lane replaces the Animation
Manager dock with one animation workspace, Blender's timeline + dope sheet as
the reference. **NifSkope.exe was neither built nor launched by this lane**
(lane BUILD10 holds the slot); the hook-up is a refusing script, not applied.
State: **BUILD PENDING** (`scratchpad/hkxedit2_20260910/PENDING.md`).

Skills invoked: `nifskope-ww-panel-style`, `ww-hkx-animation`, `nif`,
`behaivor-graph`, `ww-control-calibration`, `ww-contract-provenance`,
`ww-anchored-hookup`, `ww-test-harness-add`, `nifskope-ww-resume-pending`,
`ww-standalone-writer-gate` (by its procedure: the gate binary below).

(Sections are written as each finishes -- CONSTITUTION 1.)

## 1. The key model (src/hkxclipedit.{h,cpp}) -- DONE, gated standalone

The clip on disk is DENSE (one transform per track per frame); what the user
edits is SPARSE (keys). `HkxClipDocument` holds both: `clip.frames` is what
plays and what is written, `keys` (per track, sorted, unique frames) is what
the dope sheet shows. The one rule between them: a track's frames are
REGENERATED from its keys -- a key's frame reads the key verbatim, a frame
between two keys interpolates them (linear translation/scale, shortest-arc
nlerp with an exact normalise on rotation: the law `HkxPlayback::sampleTrack`
uses between frames, so the sheet and the viewport agree); before the first
key a track holds its first key, after the last its last. **A loaded clip
starts with a key at every frame** (bungo's choice on the default is open;
every-frame keys ship, and the `Reduce` row carries the tolerance: greedy
refinement against the dense frames, keeping first and last, adding the
worst-missed frame until every frame is within tolerance -- units, degrees by
the 4*asin metric, scale). Regeneration touches ONLY the track an edit named.

Operations, each returning a sentence (`HkxEditResult`): insertKey (replace on
an existing frame), deleteKeys (refuses a track's last key), moveKeys (clamped
to the clip; a moved key landing on an unmoved one replaces it -- Blender's
rule), copy = move with `copy`, reduce; annotations add/rename/move/delete
(per track, in seconds, kept in time order); float tracks add/remove/setKey/
deleteKey (document rows only -- see the limit below); trim(first,last)
(slice verbatim, keys re-based, a boundary frame that was not a key becomes
one so the kept frames stay exact; annotations shifted; root motion sliced,
NOT re-based); retime(fps) (round(duration*fps)+1 frames; a new frame
coincident with the old grid within 1e-4 frame is COPIED bit for bit, the
rest interpolate; keys rebuilt at every frame; root motion and float tracks
resampled the same way); removeTrack; renameTrack (binding index follows a
given skeleton, else kept and said); bakeRootMotion(track) (translation
travel -> extracted motion, the track left at frame 0's translation; the bytes
removed are remembered); unbakeRootMotion (restores those bytes VERBATIM when
the track has not moved since, else adds the motion back arithmetically and
says which); toPackfile/save through `src/hkxwrite` (interleaved).

**Limit, stated:** float tracks. Lane HKX1's reader does not decode the four
shipped float tracks and lane HKX5's writer refuses a clip with `numFloatTracks
> 0` by name, so a document carrying a float track cannot be saved: `save()`
refuses in words ("The document carries N float track(s), which the .hkx
writer does not carry yet"). `CHANGE_NEEDED.md` beside the lane names the two
edits (hkxwrite: `floats` array frame-major + binding `floatTrackToFloatSlotIndices`;
hkxanim: decode the float blocks).

## 2. The standalone gates (tests/hkxclipedit_gate.cpp) -- 72 checks, 0 failures

`release/hkxclipedit_gate.exe` (Qt6Core + Qt6Gui, HKX1's flags,
`scratchpad/hkxedit2_20260910/build_gate.sh`, BUILD-RC=0, 16:24:54), run on
`fixtures/Running_To_Slide_And_Back_To_Running.hkx` + HKX1's `skeleton.hkx`
(names) + `jog.hkx`; log `scratchpad/hkxedit2_20260910/out/gate_run.txt`.

| gate | pre-registered | measured |
|---|---|---|
| (a') | 95 tracks, 93 frames, 60 fps, a key at every frame, dense == regen | 95/93/60, 0 tracks off, consistent |
| (b) | insert 30 deg about X at frame 46 on LLeg_Thigh: reads back within 0.01 deg; 45/47 as predicted; all other bones/frames byte-identical | **0 deg off** (verbatim), 30.00 from identity; 45 and 47 are keys themselves so they are unchanged bit for bit; every other track/frame byte-identical; FLOOR: 46 differs from the original decode. Then keys 45 and 47 deleted: frame 45 = the harness's OWN nlerp(key44, key46, 0.5) to **0 deg / 0 units**, frame 47 to 5.1e-6 deg; 45 sits 86.8927 deg from key 44 = half the 173.785 deg arc |
| (c) | delete that key -> the pre-edit decode exactly | **NOT as written, and it cannot be**: with a key at every frame, deleting the key at 46 makes 46 the interpolation of 45 and 47, which is 0.877 deg from the original frame 46 (the clip's own frame 46 was never on that chord). Every other track/frame stays byte-identical. Re-inserting the original key restores the pre-edit decode EXACTLY (first diff none); the dock's Undo restores a snapshot, so Ctrl+Z is the exact way back -- gate (h). FLOOR: the last key of a track cannot be deleted |
| (d) | "FootLeft" at frame 30, save, reload -> present at 30; jog.hkx's events survive name-and-time | present at frame 30 after reload (our reader); count 4 -> 5 (the Mixamo clip already carries 4 EMPTY annotations at t=0); rename/move/delete ok; jog's 4 annotations survive save/reload name-and-time; FLOOR: a renamed one is reported. **HKXPACK caveat, section 2a** |
| (e) | trim 10..50 -> 41 frames, frame 0 == old 10 exactly; retime 60->30 -> 47, coincident frames bit-identical | 41 frames, frame 0 == old frame 10 on every track (FLOOR: differs from old 9), duration 40/60 s, consistent; 47 frames, frame i == old frame 2i on all 95 tracks **0 differ**, frameDuration 1/30, root motion 47 samples; back 30->60: 93 frames, even frames 0 differ, odd 2,701 differ (interpolations, informational) |
| (f) | bake COM: travel 0, extracted motion 487; unbake byte-identical | COM travel before **487.643** (FLOOR), after **0**; extracted motion **487.643**, 93 samples; unbake: frames byte-identical to the original decode, extracted motion byte-identical, COM keys back to their translations |
| (g) | save as .hkx; HKXPACK reads it; our reader decodes it equal | direct packfile 6 objects, 429,696 B; our reader reads it back **bit for bit** equal to the edited document, frame 46 = 30 deg about X, binding explicit 95; HKXPACK unpacks it (rc 0): class `hkaInterleavedUncompressedAnimation`, `transforms numelements=8835`, 95 tracks |
| (t) | track ops, float tracks, reduce, copy | rename refuses a duplicate; remove -> 94 tracks; float track add + key (value at 23 = 0.5 between keys 0 and 46); save refuses while a float track exists, then saves; reduce at 0.01 units / 0.05 deg / 0.001 scale: 8,742 -> 3,923 keys, regeneration within 0.0086 units / 0.0499 deg; copy 30 keys by +1: 32 -> 59 |

### 2a. HKXPACK prints EMPTY text for every annotation in a file lane HKX5's writer emits

Found by gate (d)'s HKXPACK step: `mixamo_annot.hkx` unpacked shows
`<hkparam name="text"/>` for all 5 annotations, and `jog_resaved.hkx` (jog
through HKX5's writer, untouched) shows the same where the vanilla jog shows
`FootLeft`. The BYTES are there (`FootLeft` once in the file); HKX1's C++
reader and HKXEDIT1's independent Python oracle both read the names back; only
HKXPACK does not. HKXEDIT1's canonical writer round-trips the HKX5 file to a
file that differs from it only in the tail (first diff 0x1aa50 of 0x1ab40,
"outside every chunk" = the fixup tables), i.e. HKX5 writes the LOCAL FIXUPS
in a different ORDER, and HKXPACK resolves a string only when its fixup comes
where it expects. Not this lane's file to fix (HKX5's). What this lane does
about it: see section 4 (Save routes the writer's bytes through the canonical
`Hkx::File` read->write when that layer is linked, so the saved file is the
one HKXPACK and the whole-archive gate already read). Whether the GAME reads
the names either way is the flight nobody has flown.

**What this lane did about it:** `HkxClipDocument::toPackfile` passes the
writer's bytes through `Hkx::File::read` -> `write` behind `WW_HKXCLIP_CANON`
(the hook-up defines it; the standalone gate builds with it and links
`src/hkxfile.cpp`). Re-run with the canonical route: HKXPACK sees `FootLeft`
in the saved Mixamo file (2) and in the resaved jog (1); the two-run log is
`scratchpad/hkxedit2_20260910/out/build_gate_run2.log`, 72/0 both times.

## 3. The dock (src/animworkspace.{h,cpp}, src/animdopesheet.{h,cpp}) -- WRITTEN, syntax-checked, NOT RUN

`AnimWorkspace` (object name `AnimWorkspace`), meant for a dock "Animation"
beside -- and later instead of -- the Animation Manager. Three bands
(nifskope-ww-panel-style): the transport row on top; a horizontal splitter
(`AnimWsSplitter`, sizes persisted) between the left column and the dope sheet;
the summary-or-refusal line (`AnimWsNote`, `textMuted` / `danger`) and the
action bar (`AnimWsActionBar`) pinned under both, outside the splitter.

* **The list** (`AnimWsClipList`, `wwSelectionTreeQss`): the NIF's
  NiControllerSequence blocks (a triangle glyph) then the loaded .hkx clips (a
  diamond; refused ones in `danger` with the reason as the tooltip -- the hub's
  registers, HKX3). One selection: a sequence -> `sequenceActivated` ->
  `GLView::setSceneSequence`; a clip -> `WwHkxAnimHub::activate` (measures the
  binding). `setSequenceByName` follows the render toolbar without re-driving.
  Its header row carries Load… / ✕ (per-entry menu + Unload all, "No animation
  loaded" when empty) / root, as HKX3's interim did.
* **The settings** in a `QScrollArea` (`AnimWsSettings`), one `label | field`
  grid per section, `wwHeading`, every number through `wwMakeScrubField`,
  every selector `wwMatchFieldStyle`, wheel guarded. The **Sequence** section
  (shown for a NIF sequence, hidden for a clip -- "hides what it cannot use"):
  Cycle type (`AnimWsCycleType`, Loop/Reverse/Clamp), Frequency, Start time,
  Stop time, written to the block as `ChangeValueCommand`s on the NIF's own
  undo stack in one transaction, and mirrored into `Scene::animCycle` /
  `animTags`. The **edit sections** (shown for a clip): Keys (Pose with the
  gizmo, Auto-key gizmo transforms, Reduce tolerance units / degrees),
  Annotation (Name: an editable combo offering the clip's own names first,
  then the archive vocabulary, free text allowed), Range (Trim from / Trim to /
  Retime to fps), Root motion (Track, defaulting to COM), Float track (Value).
* **The dope sheet** (`AnimWsDopeSheet`): ruler at the clip's own rate with
  "60 fps, 93 frames" at its left and nice frame steps; the marker row with
  triangles + names; rows: one per BOUND bone under the NIF's hierarchy
  (depth indent, fold arrows, collapsed ancestors hide descendants), a folded
  group "N track(s) with no node in this NIF" with the unbound rows, float
  rows; keys as diamonds (`toggle` when selected); the playhead in `accent`
  with the frame number; a NIF sequence's controlled blocks as read-only rows
  with their key times at 30 fps. Gestures: click the ruler / drag = scrub;
  click a row label = select the row (-> `indexSelected` of its NiNode);
  click a diamond = select (Ctrl toggles), drag = move (Shift = copy), drag
  empty space = box select, double-click a marker = rename, Delete/X = delete
  selection, Ctrl+A = every key of the visible rows, Ctrl+wheel = zoom, wheel
  = scroll rows, Home = frame all, middle drag = pan, right-click a row = a
  menu (insert key, select all keys, rename / remove track, isolate).
* **The transport** (one row): `|◀ ◆◀ ◀ ▶ ■ ▶◆ ▶|`, loop (the render
  toolbar's action via `setDefaultAction`), Speed (scrub, chrome off),
  the rate label, the Frame field (scrub, integral), the readout
  `frame 46 / 92 · 0.767 s`. Play/back/stop go out as `playPauseRequested`
  (the hook-up forwards it into the old dock's signal, whose lambda owns the
  application clock -- no second clock); the buttons FOLLOW the application
  (`setPlayingState` from `aAnimPlay` and `sequenceStopped`).
* **Selection both ways**: row -> `NifSkope::select( NiNode index )`;
  `setCurrentIndex` from the window -> `rowOfNodeBlock` -> the row, quietly.
  `Node::id()` is the block number, so the same integer names the row, the
  block and the playback's held node.
* **Posing with the gizmo**: the Pose row holds the selected bone's node
  (`HkxPlayback::setHeldNode`; `applyLocal` leaves it alone), so the gizmo's
  writes to the NIF block are what the viewport shows; Insert key reads the
  block's Translation / Rotation (`Matrix::toQuat`) / Scale (uniform x3) and
  keys them at the playhead; turning Pose off puts the block's pre-hold TRS
  back (`nif->set`, not undoable -- the gizmo's own ChangeValueCommands are on
  the NIF stack regardless). Auto-key: `GLView::transformCommitted` ->
  `keyNodeTransform` when the block is one of the rows.
* **Undo**: `AnimWsCommand` = the document before and after (a snapshot,
  ~400 KB for the Mixamo clip), `redo`/`undo` -> `applyDocument` -> the
  playback's `replaceClip` (range re-registered, re-bound), rows rebuilt,
  viewport updated. `wwAnimUndoGroup()` (one per process) holds the
  workspace's stack; `installUndoGroup( nif, hkx )` adds the other two and
  the active stack follows `QApplication::focusChanged` (inside the workspace
  = ours; the .hkx model's while its clip is selected; else the NIF's). The
  hook-up creates Edit > Undo / Redo from the group.
* **The .hkx document** (`WW_ANIMWS_HKXMODEL`, tier 2 of the hook-up, needs
  HKXEDIT1's `HkxModel * hkx` in the window): the entry whose name is the
  model's file stem is "the document's clip"; a `dataChanged` / `modelReset`
  on the model re-decodes it (`animFile()`) into the workspace (the undo
  stack is cleared: its snapshots describe a document that no longer exists);
  Save from the workspace reloads the model from the saved bytes (`load(
  QBuffer )`) so the Blocks tab shows the interleaved document just written.
  This is "the same document" in the weak sense -- one clip, two views kept in
  step at edit and save time -- not one object graph. Stated.

## 4. What is NOT measured

Everything about the dock's behaviour: NifSkope.exe was neither built nor
launched. `WW_ANIMWS_TEST` (`src/animworkspacetest.cpp`, `tests/spells/animws.sh`)
is written, syntax-checked with the real flags, and has NEVER RUN; its
numbers are pre-registrations (`PENDING.md` section 3). Named refuters:
(a) fails if `rebuildRows` matches names differently from the playback (it
uses its own case-insensitive map over NiAVObject blocks); (b)'s node half
fails if `Node::update` does not re-read the block into `Node::local` before
`applyLocal` sees the held node, or if `grabFramebuffer` is black headless;
(h) fails if `QUndoGroup::undo()` on a group whose active stack is another's
(the harness sets it explicitly); (i) is only as good as `SEQNIF` (the
resume must pass a NIF that `nifskope-cli list -t NiControllerSequence` shows
has one); the wheel floor's focused half cannot fire in an inactive harness
window (HKX3's finding, printed beside the check); the real Explorer drop is
HKX3's and still bungo's to try.

## 5. Divergences from Blender, stated

1. **Every-frame keys on load.** Blender keys an imported action per sample
   too, and "Clean Keyframes" is its reduce; here Reduce is a button with two
   tolerance rows. bungo decides the default.
2. **No graph editor.** Values are changed by posing the bone with the
   gizmo and keying, not by dragging an F-curve. The old dock had a value
   graph for NIF keys; the new sheet has none (yet).
3. **A NIF sequence's keys are read-only** in the sheet (times at 30 fps);
   editing NIF keys stays in the Blocks tab and the old dock until the
   follow-up writes a NIF-key layer for the new sheet. This is the one
   divergence that costs the user something; the old dock stays until then.
4. **Frames everywhere, at the CLIP's rate.** Blender has a scene frame rate;
   here the ruler ticks at the selected clip's own rate (60 vs 30 is a gate),
   and a NIF sequence's ruler is at 30 fps (the FO4 rate; Blender's default
   is 24).
5. **A moved key landing on a key replaces it** -- Blender's rule, kept.
6. **"Pose" is an explicit row.** Blender always shows the posed value in
   pose mode; here the clip's sample wins in the viewport unless the bone is
   held, because the playback writes `Node::local` every frame.
7. **Annotations are one marker row** across all tracks (Blender's markers
   are per scene); an added annotation goes on track 0, where every vanilla
   clip keeps its events (HKX1's census).
8. **Delete of a replaced key** yields the interpolation, not the pre-edit
   value (gate (c)); Undo is the exact way back, as in Blender.

## 6. The hook-up and the resume

`scratchpad/hkxedit2_20260910/hookup.py` (ww-anchored-hookup): 15 edits over
4 files (16 with tier 2), every anchor tried with both line endings and used
only when it matches ONCE; `--check` on this tree: **every anchor matches
once** (`src/nifskope.cpp`'s is CRLF, CR 9,520), and the script REFUSES to
apply until `src/hkxfile.cpp` is in the .pro -- lane HKXEDIT1's hook-up goes
first (the order is in `PENDING.md`). What it wires is in the script's
docstring; the old dock is left in place and its `playPauseRequested` lambda
is reused by forwarding the new dock's signal into it. The vocabulary file
is copied beside the exe with nif.xml. `PENDING.md` has the qmake-before-make
chain, the DEFINES object trap, the dependency read-back, the sequential
harness chain and the four documents.

## 7. Deliverables (sha256 16, bytes, CR by Python byte count; `scratchpad/hkxedit2_20260910/stamps.py`)

| file | sha256 (16) | bytes | CR | what |
|---|---|---|---|---|
| `src/hkxclipedit.h` / `.cpp` | `6a1f23c5f6b68d09` / `771f10e007dad7df` | 9,231 / 37,193 | 0 | the key model and every edit |
| `src/animdopesheet.h` / `.cpp` | `01598b17630a77e5` / `64bc65d040fba594` | 6,279 / 22,177 | 0 | the dope sheet widget |
| `src/animworkspace.h` / `.cpp` | `6cdc95d974d4e853` / `950acb491c76776f` | 11,327 / 68,132 | 0 | the dock |
| `src/animworkspacetest.cpp` | `8ab4a5376077b206` | 32,405 | 0 | WW_ANIMWS_TEST (never run) |
| `src/hkxplayback.h` / `.cpp` (changed) | `7fcd2fc3873272fb` / `f8f3ebd2a4287e4e` | 9,801 / 26,105 | 0 | +replaceClip, +held node |
| `tests/hkxclipedit_gate.cpp` -> `release/hkxclipedit_gate.exe` (16:28:39) | `190a3832b5353dbd` | 22,667 | 0 | the standalone gate, 72/0 |
| `tests/spells/animws.sh` | `717646280757567b` | 4,194 | 0 | the in-app spell |
| `res/hkx_annotation_vocabulary.txt` | `b4e773b7b4c881ce` | 55,299 | 0 | the generated vocabulary (1,640 names; 2 with control characters skipped, see 8) |
| `sx_HKXEDIT2.sh` | `f33052ca475e69b8` | 595 | 0 | the syntax pass (sx_tmp.sh flags) |
| `scratchpad/hkxedit2_20260910/` | | | 0 | `hookup.py` (15/15 once), `PENDING.md`, `CHANGE_NEEDED.md`, `WW_CHANGES_ENTRY.md`, `MISTAKES_ENTRIES.md` (appended to MISTAKES.md, append-only verified), `build_gate.sh`, `annot_vocab.py`, `stamps.py`, `append_mistakes.py`, `out/` (gate runs, saved files, HKXPACK XML) |

Final syntax pass (`syntax_final.log`): the eight touched translation units,
with and without `-DWW_ANIMWS_HKXMODEL -DWW_HKXCLIP_CANON`: 16 RC=0, ALL-RC=0
twice, zero warnings printed.

## 8. Mistakes (appended to MISTAKES.md; text in `MISTAKES_ENTRIES.md`)

1. Bash heredocs halved backslashes twice -- once in an anchor COUNT (a
   false "0 matches"), once in a PATCH of hookup.py (a SyntaxError and a
   silent 0-of-1 replacement); the sixth recording; the rule now says checks
   too. 2. Gate (c) as pre-registered cannot hold on an every-frame key model;
   found by running it; the honest statement is Undo's. 3. HKXPACK prints
   empty annotation text for HKX5-written files and three lanes never asked;
   a third-party round trip compares every field family. 4. A container
   declared by value in the header, used by pointer in the source (six
   compile errors, caught by the syntax pass). 5. Four `std::min/max( int,
   qsizetype )` errors. Also, not a mistake but a finding: the generated
   vocabulary carried two carriage returns INSIDE shipped annotation names --
   the generator now skips names with control characters and says how many.

## 9. Finished-work skill review (CONSTITUTION 1a)

Loaded and used in earnest: `nifskope-ww-panel-style` (the three bands,
`wwHeading` / `wwMakeScrubField` / `wwMatchFieldStyle` / `wwGuardWheel` on
every control, the pinned summary-or-refusal line, the hide-what-it-cannot-use
rule for the Sequence vs edit sections, and gate (j)'s counts with floors);
`ww-hkx-animation` (sections 8, 11, 13 saved re-deriving the quaternion
convention, the playback path, `Node::local`'s protection, the frame
epsilon, the class layer and its gate binary -- and 13's `hkxfile_oracle.py
layout` is what localised the HKXPACK annotation defect to the fixup tables);
`ww-test-harness-add` (the whole harness shape, widgets by object name, the
1.5 s wait, the SKIP rule, the fixture-path rule, the focus floor that cannot
fire); `ww-anchored-hookup` (the refusing script, the Write-tool rule -- broken
once and paid for -- the compile-time switch with both halves syntax-checked);
`nifskope-ww-resume-pending` (PENDING's shape, qmake before make, the DEFINES
object trap, rule 9 on predictions); `nif` (the NifModel / NifItem /
ChangeValueCommand idiom read from `gizmoEnd`, `Node::id()` = block number);
`behaivor-graph` (what an annotation name is to a graph -- an event -- which is
why the vocabulary is offered and free text allowed); `ww-standalone-writer-gate`
(by procedure: the Qt6Core+Gui link, the independent nlerp in the gate, the
floors). Declined with the reason: `ww-control-calibration` -- no ours-vs-
vanilla scalar here; the controls are bit-identity and a harness-side
interpolation, both shown failing on the wrong input. `ww-contract-provenance`
-- no contract page was written (the key model lives in the header's own
comment and the WW_CHANGES entry; a `docs/HKX_EDIT_MODEL.md` is owed only if
the director wants the model as a contract page).

Written: `ww-hkx-animation` section 14 (the key model, the delete-vs-undo
fact, the HKXPACK annotation trap and the canonical route, the generated
vocabulary, the retime/bake arithmetic, the playback additions) and
`ww-anchored-hookup` section 3a (candidate anchors, either line ending, the
order refusal, the heredoc-for-checks trap) -- both in the REPO tree; the
director mirrors them. Should exist and does not, declined here: a skill for
"a Blender-reference dock" -- the panel-style skill already carries it, and
this lane's divergence list (section 5) is the part that is new each time.

## 10. For the HANDOFF top block (text only; the director splices)

**HKXEDIT2 LANDED CODE, BUILD PENDING 16:5x** (scratchpad/lane_hkxedit2_report.md):
src/hkxclipedit.{h,cpp} = the key model (sparse keys over the dense clip,
every-frame keys on load, Reduce with a tolerance; insert/move/copy/delete,
annotations, float-track rows, trim, retime, remove/rename track, root-motion
bake/unbake, Save through hkxwrite + the canonical hkxfile layout) gated
standalone 72/0 + HKXPACK (release/hkxclipedit_gate.exe); src/animworkspace +
src/animdopesheet = the "Animation" dock replacing the Animation Manager
(one list NIF sequences + clips, dope sheet at the clip's rate with bone rows
under the NIF hierarchy, markers, float rows, one transport row, the old
sequence controls as rows, selection both ways, gizmo pose hold + Insert key
+ auto-key, one QUndoGroup); WW_ANIMWS_TEST + tests/spells/animws.sh written,
NEVER RUN; hookup.py 15/15 anchors, refuses until HKXEDIT1's is applied
(order in PENDING.md); FOUND: HKXPACK prints empty annotation text for every
hkxwrite-emitted file (fixup order; our readers fine) -> the workspace saves
canonical; CHANGE_NEEDED: float tracks in reader+writer, HKX5's fixup order,
the timeline.* retirement follow-up. bungo's call owed: every-frame keys vs
reduced on load.

## Build (BUILD11)

**The hook-up refused, and the refusal was its own.** `hookup.py` printed
`tier 2 (WW_ANIMWS_HKXMODEL): yes` and 15 anchors `count=1`, then:

    NifSkope.pro  after  NO anchor matches once:
      [('DEFINES += WW_HKXCLIP_CANON\n', "'\n'", 0), (..., "'\r\n'", 0)]

The TIER2 edit anchors on the `DEFINES += WW_HKXCLIP_CANON` line that base edit
#3 INSERTS, and both `--check` and `--apply` resolve every anchor against the
bytes on disk before anything is written -- so it can never count 1 in one pass.
`PENDING.md` predicted "16 anchors"; the measured truth is **15 in pass one and
the 16th only after pass one has landed**.
`scratchpad/build11_20260910/hookup2_apply.py` splits the two passes and
imports `EDITS`, `TIER2`, `DOCK_BLOCK` and `vocab_text` from this lane's own
script, keeping every rule (LF then CRLF per anchor, the inserted text must not
already be present, CR moves by exactly the inserted CRs, HKXEDIT1 first).
Applied: NifSkope.pro 4 + 1 edits (19,613 -> 20,185), nifskope.h 3 (-> 48,915),
nifskope_ui.cpp 7 (-> 1,488,609), nifskope.cpp 1 (CR 9,556 -> 9,558, +2 as
predicted). Then two includes the hook-up did not carry
(`<QUndoGroup>` for the Undo/Redo actions, `"animworkspace.h"` in nifskope.cpp
for the `select()` edit), found by `sx_BUILD11.sh`, marked `(lane BUILD11)`.

**`tests/spells/animws.sh`, first run ever: 57 checks, 0 failures, 1 skip,
PASS.** Not one of the four first-run defects this lane named actually fired
except the one guarding gate (i). Measured green, among others: 78 bone rows
with 93 keys each and 0 rows off, the ruler at 60 fps and the rate row reading
`60 fps`, the readout `frame 46 / 92 - 0.767 s` with the playhead at 46,
`LLeg_Thigh` as track 3 / row 4, row -> viewport selection (block 10 both ways)
and viewport -> row (`COM` row 2), the 30-degree key at frame 46 reading back
**0 deg off** from the document AND **0 deg off** from the viewport node with
every other bone and frame byte-identical, trim 10..50 -> **41 frames** with
frame 0 equal to old frame 10 exactly and the scene range following to
0.666667 s, retime 60->30 -> **47 frames** at ruler 30 with **0** coincident
frames differing, root motion **487.643 -> 0** on bake and byte-identical on
unbake in both the track and the extracted motion, Save -> 6 objects, 95 tracks
x 93 frames, 429,728 bytes on disk, re-read bit for bit, and Undo of the retime
back to 93 frames at 60. Pictures written:
`scratchpad/hkxedit2_20260910/dock_frame46.png` (34,589 B) and
`viewport_gizmo.png` (47,217 B).

**The one skip is not a pass, and chasing it found a real defect.** Gate (i)
prints a named SKIP on the default `SEQNIF`, `10mmPistol.nif`, which carries no
`NiControllerSequence` -- exactly as `PENDING.md` allowed for. Re-run with one
that does, `E:/Tools/Fallout 4/DataUnpacked/Data/Meshes/Effects/
TeleportInFXLight.nif`, the harness **dies inside gate (i)**: the log stops
after gate (f) and no `N checks, M failures`, no `PASS`/`FAIL` and no `done`
line is ever written. The NIF is not at fault -- it opens and renders on its own
in this exe (`scratchpad/build11_20260910/seqnif_probe.png`, rc=0). This lane
named the branch itself: "the `(i)` branch opens a second file through
`NifSkope::openFile` and waits 2.5 s -- if the load is slower the checks read
the old model". Gate (i) is unproven in either direction and is not fixed here.

**The standalone document gate, re-derived rather than accepted:**
`scratchpad/hkxedit2_20260910/build_gate.sh` rebuilt
`release/hkxclipedit_gate.exe` and ran it -- **72 checks, 0 failures**,
`GATE-RC=0`, and with HKXPACK `FootLeft` seen 2 in the saved file and 1 in the
resaved jog, `GATE-RC-WITH-HKXPACK=0`.

**Neighbours the change reaches**, since `src/hkxplayback.{h,cpp}` moved:
`tests/spells/hkxanim_play.sh` **27 checks, 0 failures, PASS**;
`tests/spells/hkxanim_ui.sh` **48 checks, 1 failure** -- BUILD9's own known
red, gate (g)'s wheel floor, which cannot fire because a WW harness window is
never activated; `tests/spells/files_tab.sh` **28 checks, 2 failures**, and
both are BUILD9's two pre-registered reds by name (Qt's own
`QLineEditIconButton` clear buttons untipped, 2 of 6; `PipboyBone`, which the
fixture drives with its own `NiTransformController`). The total is one below
BUILD9's 29 and which check moved was not traced -- neither number is a
failure.

### The one table of clocks

| artefact | time |
|---|---|
| `release/NifSkope.exe` (this build) | 2026-09-10 **17:08:39**, 20,693,504 B |
| the exe this replaced (lane BUILD10) | 16:45:53, 20,007,936 B |
| `release/style.qss` (copied at link time) | 17:08:39, equal to `res/style.qss` |
| `release/hkclasses_fo4.json` | 17:08:39, 1,511,662 B |
| `release/hkx_annotation_vocabulary.txt` | 17:08:39, 55,299 B |
| bungo's own window, opened mid-lane | started 17:05:44, pid 700, no `--port` |

qmake ran before make; `DEPCHECK missing=0` over eleven objects; the three WW
defines are each once in `Makefile.Release` and on every compile line; the
exe-newer sweep is 1 stale of 114 paths and the exception is
`src/watermark.cpp`, lane WATER7's live edit in this shared tree, which is NOT
in this exe. `WW_HKXANIM_UI`, `WW_HKXCLIP_CANON` and `WW_ANIMWS_HKXMODEL` are
new or newly-read flags, so the objects of every translation unit that reads
one were deleted before make (the BUILD9 DEFINES trap: make compares mtimes,
not flags).

**Skipped harnesses, named with the reason** (`nifskope-ww-resume-pending` s5):
`loaded_nifs.sh`, `top_bar.sh` and `ui_align.sh` -- this build renamed no
user-visible string and moved no bar, so the sibling-label class of red
(BUILD9 s10) cannot have been introduced; `lodgen_*`, `water_*`, `lodl/lodt`
and the impostor gates -- nothing in these three lanes reaches the generator,
the water tool or the terrain readers. `WW_POSEDRAW_TEST` was left alone: it
was already failing at "clicking a bone did not make it the active object" on
both fixtures BEFORE lane SKELFIX, and SKELOVERLAY's report says one run on the
70-bone facial rig is what settles it.
