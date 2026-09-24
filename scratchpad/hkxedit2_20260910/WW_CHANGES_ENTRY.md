## 2026-09-10 -- lane HKXEDIT2: the animation workspace, and the .hkx clip fully editable (BUILD PENDING)

bungo, verbatim: *"Just make hkx fully editable in our nifskope"*; *"Animation
manager was one of the first features for nifskope, and it's pretty old and
outdated btw"*. One new dock, "Animation" (`src/animworkspace.{h,cpp}`,
`src/animdopesheet.{h,cpp}`), Blender's timeline + dope sheet as the
reference, replacing the Animation Manager (the old dock stays until the new
one is proven in the app; the retirement is the follow-up in
`scratchpad/hkxedit2_20260910/CHANGE_NEEDED.md`).

* **One list**: the NIF's NiControllerSequences (triangle) and the loaded
  .hkx clips (diamond, refused ones in the danger colour). Selecting a
  sequence drives the scene as before; selecting a clip goes through the hub's
  `activate`, which measures whether it bound. The old manager's per-sequence
  controls are rows under the list (Cycle type, Frequency, Start time, Stop
  time), written to the block through `ChangeValueCommand` on the NIF's undo
  stack.
* **The dope sheet**: a ruler at the clip's OWN rate (60 for the Mixamo
  fixture, 30 vanilla), the annotations on a marker row with their names, one
  row per tracked bone under the NIF's hierarchy (collapsible), the unbound
  tracks under a folded group, float tracks as rows, keys as diamonds, the
  playhead; click a bone row = the bone is selected in the viewport, and the
  other way round. Box select, drag to move (Shift = copy), Delete, Ctrl+wheel
  zoom, wheel scrolls rows. A NIF sequence's key times are shown read-only.
* **The transport** in one row: to start, previous key, play back, play,
  stop, next key, to end, loop (the render toolbar's own action), speed, the
  rate, the frame field, the readout `frame 46 / 92 · 0.767 s`.
* **Editing** (`src/hkxclipedit.{h,cpp}`, the key model): keys are sparse
  over the dense per-frame clip; a loaded clip starts with a key at EVERY
  frame (bungo's default is still his call; Reduce with a tolerance row thins
  them); a track's frames regenerate from its keys (verbatim at a key, linear
  translation/scale + shortest-arc nlerp between, first/last held outside),
  and only the edited track is touched. Insert key (a bone posed with the
  viewport gizmo -- the "Pose" row holds the bone out of the clip so the gizmo's
  result is what you see -- or auto-key), move/copy/delete keys, box select,
  Reduce; annotations add (name from the game's own vocabulary --
  `res/hkx_annotation_vocabulary.txt`, 1,642 names tallied from every shipped
  clip, `FootLeft` 5,464 / `weaponFire` 5,397 / `FootRight` 5,380 first -- or
  free text) / rename / move / delete; float tracks add / key / delete (rows
  of the document only: the reader and writer do not carry them yet,
  CHANGE_NEEDED 1, and Save refuses in words while one exists); Trim, Retime
  (coincident frames copied bit for bit), Remove / Rename track, Bake root
  motion (COM travel -> extracted motion) / Unbake (byte-identical when the
  track was not touched since), Save / Save as .hkx (interleaved, through
  `src/hkxwrite` and then the canonical packfile layout of `src/hkxfile` --
  `WW_HKXCLIP_CANON` -- because HKXPACK 0.1.6 prints EMPTY annotation text for
  the writer's own fixup order: measured, 16 bytes in the fixup tables, both
  our readers fine). Undo: one command per edit holding the document before
  and after; one `QUndoGroup` for the window (the NIF's stack, the .hkx
  document's, the workspace's; active follows focus); Edit > Undo is created
  from the group by the hook-up.
* `src/hkxplayback.{h,cpp}` gained `replaceClip` (an edited clip back into
  the playback, range re-registered, re-bound) and the held node.

**Measured standalone** (`release/hkxclipedit_gate.exe`, HKX1's flags +
`src/hkxfile.cpp`, 72 checks / 0 failures, plus HKXPACK): insert 30 deg about X
at frame 46 reads back 0 deg off, every other track/frame byte-identical,
neighbours by the harness's own nlerp to 0 / 5e-6 deg; delete -> the 45/47
interpolation (0.877 deg from the original -- not the pre-edit bytes, which
only Undo restores exactly); "FootLeft" at frame 30 survives save/reload and
HKXPACK sees it; jog's annotations survive name-and-time; trim 10..50 = 41
frames with frame 0 == old 10; retime 60->30 = 47 frames, 0 coincident frames
differ; bake COM 487.643 -> 0 and the motion 487.643, unbake byte-identical
(frames and motion); save -> our reader reads it back bit for bit, HKXPACK
unpacks it (`transforms 8835`, 95 tracks); reduce 8,742 -> 3,923 keys within
0.0086 units / 0.0499 deg.

**NOT measured**: the dock itself, gates (a)-(j) in the app (`WW_ANIMWS_TEST`,
`tests/spells/animws.sh`, written and syntax-checked, never run), the gizmo
hold, selection both ways, the QUndoGroup binding, the NIF sequence rows, the
pictures. NifSkope.exe was neither built nor launched; hook-up
`scratchpad/hkxedit2_20260910/hookup.py` (after HKXEDIT1's), resume
`PENDING.md`. **Divergences from Blender**, stated: every-frame keys on load
(Blender's imported actions are keyed per sample too; the reduce is Blender's
"Clean Keyframes"); no graph editor / F-curves in the new sheet (values are
edited by posing, not by curve); a NIF sequence's keys are read-only in the
sheet (the Blocks tab and the old dock keep the NIF-key editors); frames, not
seconds, everywhere; the ruler's rate is the clip's, not a scene setting
(Blender: a scene frame rate); a moved key landing on a key replaces it
(Blender's behaviour); "Pose" is an explicit row where Blender always shows
the posed value in pose mode.
