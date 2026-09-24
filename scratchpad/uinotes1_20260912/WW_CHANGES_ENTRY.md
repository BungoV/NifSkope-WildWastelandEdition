# For WW_CHANGES.md -- paste the block below (lane UINOTES1, 2026-09-12)

## 2026-09-12 — The animation workspace answers bungo's nine UI rulings

The Animation dock was gone over end to end against the rulings of
2026-09-12 01:3x-02:0x, with Blender's dope sheet as the reference and every
divergence from it named.

**The window's bottom bar is gone.** The status bar that showed the loaded NIF
took a row of the window and said nothing that is not in the title. The
messages it carried now appear as a short-lived line at the bottom of the
viewport and fade; nothing lost a message.

**Keys look like Blender's.** A selected key is orange, a key selected on
another row is orange-red, the playhead is a blue box on the ruler with its
frame number in it, and the frames outside the play range are darkened. The
four colours are new `wwskin` tokens taken from the installed Blender 4.5 --
`animKey`, `animKeySel`, `animKeySelOther`, `animPlayhead`, `animPlayheadText`,
`animOutOfRange` -- each with the Blender theme entry it came from written in
its comment, and the two places where Blender's value could not be used
verbatim say why.

**The play range is draggable.** Two grips on the ruler snap to whole frames,
and the same two numbers are Start and End fields in the transport row. Ctrl+Home
and Ctrl+End put the range's ends on the playhead. What used to be the Trim
fields and the Trim button is now one "Trim to range".

**Annotations, anywhere.** Right-click any row or the ruler for "Add annotation
at frame N" -- the frame that was CLICKED, not the playhead -- with an inline
name editor; right-click a marker to rename or remove it. M adds one, Ctrl+M
renames. Dragging an existing annotation no longer throws the timeline's zoom
back to the default; the zoom was being reset by the refresh that followed the
drag.

**Remove transform axes…** sits under Remove track: six boxes (translation
X/Y/Z, rotation X/Y/Z) set the chosen components to the track's frame-0 value on
every key, in one undo step. On the running-to-slide clip's COM, stripping X and
Y holds both constant over all 93 keys and leaves Z and every rotation
byte-identical.

**The Animations list works like a list.** Delete/X, Ctrl+C/V/X, Shift+D,
F2 or double-click, Ctrl+A, Ctrl+Up/Down, multi-select, and drag to reorder with
Qt's own drop line -- each of them also in the right-click menu.

**The fifteen-button bar at the bottom of the dock is gone.** Its actions are in
a header menu bar -- Clip, Key, Channel, Marker, View -- and Save / Save as are
under Clip. The Keys block is gone too: Pose-with-the-gizmo and Auto-key are
toggle buttons in the transport row, where Blender keeps its record button.

**A panel opens from the right edge of the dock** (the header's arrow, View >
Side panel, or the N key, Blender's own sidebar key). It shows only what the
selection is about: Clip, or Clip + Track, or Clip + Key, or Clip + Annotation,
or Clip + Float track. It remembers the width it was dragged to.

**The transport icons are drawings now.** Every glyph in the row -- jump to
start and end, previous and next key, play, pause, stop, loop, the auto-key
record dot -- is drawn on one 16x16 grid from one triangle, one bar and one
diamond, at one weight, inked from the palette, and greyed properly when
disabled. The play button becomes a pause button while it plays. Loop is the
two-arrow cycle; speed reads "1.50x". Every tooltip names its action and its
key, and five of the buttons have a key for the first time: Space, Shift+Ctrl+Space,
Shift+Left, Shift+Right, Up and Down, Blender's own timeline keys, live while
the dope sheet has the keyboard.

**The "Load… / root" header stopped clipping.** The heading had all the stretch
in that row and the buttons paid for it; the stretch is a spacer now and the
column has a minimum width measured from the buttons themselves.

Harnesses: `animws.sh` grew (k1)-(k8), (l), (m), (m2), (n), (o), (p) and (q);
`ui_align.sh` grew (s1) and (s2); `hkxanim_ui.sh`'s (g) follows the dock's new
header. Every one of them carries a floor -- a case the same check must call
red -- named in `tests/spells/animws.sh`'s own WHAT IS MEASURED block.

**Built and measured (added by lane UINOTES1b, 2026-09-12 05:5x).** The above is
no longer code-only: `release/NifSkope.exe` of **2026-09-12 05:48:33,
21,817,856 bytes**, built at `make rc=0` with 0 errors, carries all of it. Its
gate, `animws`, runs **210 checks with 1 failure and 1 skip** (the rung exe of
04:10:38 ran that harness's older 72 checks, 0 failures). The other six UI
harnesses -- `hkxanim_ui` 48/1, `ui_align` 15/0, `water_ui` 84/0, `files_tab`
29/1, `top_bar` 43/5, `skeleton_overlay` 5/1 -- are each **the same count on the
merged exe as on the rung**, so this work added no red to any of them; those six
numbers come from the 05:06:05 exe and have not been re-run on the 05:48:33 one.

**One thing does not work yet and is not claimed to.** Ctrl+A in the Animations
list selects only the current row: the row selection drives the viewport, the
viewport answers back with `setCurrentItem`, and Qt's ClearAndSelect throws the
other rows away. Measured in the same run: with the list's signals blocked the
same select-all takes 4 of 4. So multi-row Delete / Copy / Cut cannot be reached
with Ctrl+A today. Deliberately left red rather than patched around.

Drag-to-reorder is proved from the row move through the commit to the clips, the
playback order and the scene's own list; the very last inch -- Qt delivering the
Drop from a real mouse -- cannot be driven from inside the application and is
owed to one drag by hand.
