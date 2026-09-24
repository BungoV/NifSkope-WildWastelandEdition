# Lane UINOTES1 — bungo's nine animation-workspace rulings of 2026-09-12

Tree: `E:\Projects\NifskopeWWE_ui` (robocopy of the repo taken 2026-09-12 01:53).
Brief: `scratchpad/brief_uinotes1.md`. Rulings verbatim: `scratchpad/brief_uinotes_20260912.md`.
Marker written 2026-09-12 01:56 (`scratchpad/uinotes1_20260912/BUILDING`).
Baseline exe for every "before" picture: `release/NifSkope.exe` 2026-09-11 23:26:29, 21,484,032 bytes,
rung aside once as `release/NifSkope.before_uinotes1.exe` before the first build of this lane.

## 0. Rulings and gates

| # | Ruling (bungo, 2026-09-12 01:3x-02:0x) | Work step | Gate |
|---|---|---|---|
| 1 | "pic rel needs to be removed, that entire bottom bar that shows you the loaded nif, waste of space." | 1 | `ui_align.sh` (s1)(s2): no QStatusBar child; client-bottom gap back to <= 2 px; floor: a 24 px gap fails the same test |
| 2 | "When clicking on the keyframes, they go invisible, instead of appearing like in Blender, so as orange." | 2 | `animws.sh` pixel samples: three distinct selection colours |
| 7b | "Selected annotation should also be orange, and orange redish for secondary annotation selection" | 2 | same |
| 8 | "Timeline marker where you're at ... should also be blue." / "Areas inside of an animation should be as they are, outside like in Blender, darkened" | 2 | same: ruler box, inside unchanged, outside darkened |
| 9 | "Allow me to drag the starting and ending frame, add markers for them of some kind I can drag" | 3 | +10/-10 frame drags and box edits: frame count, darkened-zone edge, HKX duration on save |
| 7 | "Now, why can't I right click and insert an annotation anywhere?" | 4 | context menu on row / ruler / marker, clicked frame honoured |
| 7a | "when I zoom into the timeline and try to drag an already existing annotation, the zoom resets to default zoom" | 4 | zoom in, drag a marker 5 frames: visible range unchanged, marker moved exactly 5; measured RED first |
| 3 | "Add an option here, under remove track, to remove all transforms in specific directions" | 5 | COM of `Running_To_Slide_And_Back_To_Running` on `fixtures/human_male_vanilla.nif`: X/Y constant over 93 keys, Z byte-identical, HKX round-trips |
| 5 | "For these animations, I should be able to use a shortcut to delete, copy, paste, etc." | 6 | every shortcut and menu entry driven on a three-clip fixture |
| 6 | "it's getting pretty crowded in here, isn't it?" | 7 | dock chrome height before/after |
| 6a | "Why not add another panel in the animation manager, that opens from the right side" | 7 | one section per selection kind; every former button's action found in a menu by text |
| 4 | "Most of these icons are very bad looking, and unclear to what they do." | 8 | picture at 1:1 and 2:1, every tooltip asserted |

Standing rules obeyed throughout: no descriptions or blurbs in panels and menus (label + control
only, tooltips allowed and they name the action and its shortcut); new colours are ADDED as named
`wwskin` tokens with Blender 4.5's own values, read out of the installed Blender and cited; every
number is quoted beside its floor; nothing is called fixed or final before bungo sees it live.

### The colours added for this lane

Read out of the installed Blender, 2026-09-12 02:0x:

* dark column: `E:/Tools/3D/Blender 4.5/blender.exe -b --factory-startup --python-expr` printing
  `bpy.context.preferences.themes[0].dopesheet_editor.*` — the shipped
  `4.5/scripts/presets/interface_theme/Blender_Dark.xml` is 71 bytes and EMPTY, because dark IS the
  factory default and the preset carries no overrides, so the values had to come from the running
  program.
* light column: `4.5/scripts/presets/interface_theme/Blender_Light.xml` (49,887 bytes).

| token | dark | light | Blender source |
|---|---|---|---|
| `animKey` | `#bfbfbf` | `#3c3c3c` | `dopesheet_editor.keyframe` (light `#e8e8e8` inverted for our lighter ground — divergence, see below) |
| `animKeySel` | `#ffbe33` | `#ffbe33` | `dopesheet_editor.keyframe_selected` |
| `animKeySelOther` | `#ff8c00` | `#ff8c00` | `dopesheet_editor.long_key_selected` (`#FF8C0099` dark, `#ff8c00cc` light; alpha dropped, we paint opaque) |
| `animPlayhead` | `#4772b3` | `#5680c2` | `dopesheet_editor.frame_current` |
| `animPlayheadText` | `#ffffff` | `#ffffff` | Blender draws the frame number white in that box |
| `animOutOfRange` | `#17191c` | `#cfcfcf` | NOT a theme entry: `ANIM_draw_framerange` shades TH_BACK by -25 and alpha by -100; that arithmetic applied to our own ground, painter uses alpha 155/255 |

Two divergences, stated as the Blender-reference rule requires:

1. light-theme unselected key: Blender's `#e8e8e8` is nearly white against its `#6b6b6b` ground;
   our light ground is much lighter, so the same relationship needs a dark diamond (`#3c3c3c`).
2. `animOutOfRange` has no theme entry in Blender at all. The brief's fallback applies ("if you
   cannot find the file, use the documented defaults and say so"): the shade/alpha arithmetic from
   Blender's own drawing code, applied to our background, and that is written into the token's
   comment in `src/nifskope_ui.cpp`.

## 1. Item 1 — the bottom bar goes

Ruling, verbatim: "pic rel needs to be removed, that entire bottom bar that shows you the loaded
nif, waste of space."

### What it was

A `QStatusBar` named `statusbar` in `src/ui/nifskope.ui`, pinned to 24 px by its own
`minimumSize`/`maximumSize`, carrying: the loaded file's path (`filepathStatusbarWidget`), the load
progress bar, a size grip, and 23 transient messages shouted at it from all over the program.

### Where the messages went

A short-lived line at the BOTTOM OF THE VIEWPORT that takes no window height at rest. Not a bar:
when there is nothing to say it is not there, and the window's client area is 24 px taller than it
was.

It had to be a frameless top-level (`Qt::Tool | Qt::FramelessWindowHint`) positioned in GLOBAL
coordinates, not a child widget: the viewport is `QWidget::createWindowContainer( ogl, parent )`
around a `GLView : QOpenGLWindow`, i.e. a NATIVE window, and a native window paints over any
sibling or child QWidget. This is the same shape and the same reason as the four operator redo
panels already in the tree (`positionRedoPanel()`), and the new line stacks above whichever of
those is visible. Divergence to note for the merge: it is a floating tool window, the codebase's
established overlay idiom, not a widget overlay.

New on `NifSkope` (`src/nifskope.h`, implemented in `src/nifskope_ui.cpp`):

* `showTransientMessage( const QString &, int ms = 4000 )`
* `clearTransientMessage()` — keeps the line up while the progress bar is visible
* `QString transientMessage() const` — empty when the line is hidden
* `QProgressBar * loadProgressBar()`
* private `ensureTransientLine()`, `positionTransientLine()`, and the three members

The load progress bar is re-parented into that line, so a long load still shows a bar, just not a
permanent one.

### The files touched

| file | what |
|---|---|
| `src/ui/nifskope.ui` | the whole `<widget class="QStatusBar" name="statusbar">` block deleted: 87 lines, 2187 bytes, including the `#statusbar` stylesheet, the 24 px min/max size and the progress-bar / size-grip / filepath children |
| `src/nifskope.h` | the five new members declared, with the rationale and the warning below |
| `src/nifskope_ui.cpp` | the line implemented; `initToolBars()`'s status-bar setup replaced by `ensureTransientLine()`; 5 message call sites repointed; `GLView::gizmoStatus` rewired; the two harness reads repointed |
| `src/nifskope.cpp` | 18 message call sites repointed; the now-vacuous `ui->statusbar` guards cleaned out |
| `src/uialigntest.cpp` | the gate below |

23 call sites in total. `grep` for `statusBar()` and `ui->statusbar` across `src/` comes back empty.

**The trap, written into the code at both places that could fall into it:**
`QMainWindow::statusBar()` CREATES a status bar on demand. After the .ui deletion, any call to it —
including from a gate — would silently build the very bar the ruling removed. So the gate asks
`skope->findChild<QStatusBar *>()` instead.

### The gate

`src/uialigntest.cpp`, run by `tests/spells/ui_align.sh`. A new block prints, unconditionally (so
the number can also be read out of the OLD 23:26:29 exe's log, which knows nothing about this
check, because it is computed from rectangles that build already dumps):

```
  bottom: client h <H>; lowest visible = <widget> at bottom <B>; GAP <H-1-B> px
```

then four lines:

* floor — something was actually measured at the bottom (a run that measured nothing cannot pass)
* `(s1) the window has no QStatusBar child` — via `findChild`, deliberately not `statusBar()`
* `(s2) the bar's height is back: bottom gap <= 2 px`
* floor — `(floor) a 24 px gap FAILS the same test`, i.e. the old geometry is shown going red
  through the same comparison the verdict comes from

Numbers to be filled in from the two logs after the build: before (old exe) GAP expected ~24 px,
after expected 0-2 px.

### Line endings

`src/nifskope.cpp` is a MIXED file and was edited by binary splice only: CR 9559 -> 9557,
LF 10708 -> 10705 (three guard lines removed). `src/nifskope.h`, `src/nifskope_ui.cpp`,
`src/ui/nifskope.ui`, `src/uialigntest.cpp` are LF-only and still measure CR 0. All counts taken
with Python byte counts, never grep.

### Status

Code done. Gate written, not yet run — the build comes at work step 9, once all nine items are in,
per the brief's one-build rule.

## 2. Items 2, 7b, 8 — the drawing

Three rulings, one painter: `src/animdopesheet.cpp`.

### What was wrong, measured before it was touched

A selected key diamond was filled with `wwSkinColor("toggle")` = `#4772b3`. The row it sits on,
when it is the current row, is filled with `wwSkinColor("selBgActive")` = `#4a7ab0`. Those two are
3, 8 and 3 apart in R, G and B — 14 apart added up. Blue ink on a blue plate. That is bungo's "they
go invisible": the key is still drawn, it just cannot be seen. The playhead was `accent`, the same
orange he now wants for the selection, which is why the two had to swap families rather than just
be recoloured.

### What it draws now

* keys and annotation markers, three states: the ACTIVE member of the selection (the one last
  clicked) `animKeySel` #ffbe33, every other member of the same selection `animKeySelOther`
  #ff8c00, anything unselected `animKey`. Three states need two pieces of state, so the sheet now
  carries an active key (`activeKey()`) beside the selection list, and an active annotation
  (`selectedMarker()`) beside a new list of selected annotations (`selectedMarkers()`).
  Ctrl+click adds an annotation to the selection and makes the clicked one active, the same way
  Ctrl+click already worked for keys.
* playhead: a 2 px `animPlayhead` line down the whole sheet and a rounded `animPlayhead` box on the
  ruler carrying the frame number in `animPlayheadText`.
* outside the clip's range: `animOutOfRange` at alpha 155 over the ruler band, the annotation band
  and every row band, between the key area's left and right edges only. Inside the range NOTHING is
  painted over — "as they are" is literal. The overlay goes down right after each band's own
  background and BEFORE that band's keys, so a key outside the range is as bright as one inside,
  which is what Blender does (`ANIM_draw_framerange` runs in the background pass).
* the range itself lives on the sheet as `rangeStart()` / `rangeEnd()` / `setRange()`, defaulting to
  the whole clip, so a clip nobody has trimmed has no darkened zone at all. Item 9's draggable
  grips are what will move those two numbers.

One divergence from what was there: float-track keys were painted `accent` (blue) and are now
`animKey` like every other key. A blue key next to a blue playhead is the confusion ruling 2 is
about. Read-only rows — a NIF sequence's interpolator keys, and tracks not bound to the NIF — stay
`textMuted`, which is how the sheet says "you cannot edit this".

### The gate

`src/animworkspacetest.cpp`, gate (k), run by `tests/spells/animws.sh` on the 60 fps, 93-frame
Mixamo fixture. A colour cannot be read off a widget's state, so the gate reads the PIXELS: it
grabs the sheet and samples at the coordinates the painter itself computes (`frameToX`, and a new
public `rowCenterY`), then compares them to the named tokens.

Per pass — and there are two passes, the whole 93 frames in view and zoomed to frames 15..45,
because a colour can survive at one scale and not the other:

* `(k2)` the active key is `animKeySel`; `(k7b)` the other selected key is `animKeySelOther`;
  `(k2)` an unselected key is `animKey`; `(k2)` the three are three DIFFERENT colours
* `(k2)` the active key stands off the selected row underneath it: >= 60 apart in R+G+B
* `(k8)` the ruler's playhead box and the line below it are both `animPlayhead`
* `(k8)` inside the range the row background is still exactly `selBgActive`
* `(k8)` outside the range it is >= 20 apart and darker

Then the annotations, with a second annotation added at frame 60 (and undone again, so the document
the save gate reads is the one gate (d) left): active orange, other selected orange-red, deselected
plain.

Two floors:

* `(k floor)` the colour pair that WAS here — `toggle` over `selBgActive` — goes through the same
  contrast test and must come back red. It is 14 apart, so it does.
* `(k8 floor)` with the range set to the whole clip, the two samples that just differed must be the
  same colour, and that colour must be `selBgActive`. So the darkening check is shown able to fail.

Picture: `sheet_colours.png` into `WW_ANIMWS_OUT`, the sheet at 1:1 with two keys selected and the
range at 10..50.

### Status

Code and gate written, not yet run (one build, at work step 9). Numbers to be pasted in from
`release/ww_animws_test.log`.

## 3. Item 9 — the play range (start/end grips + Start/End boxes)

**The ruling, verbatim:** "Allow me to drag the starting and ending frame, add
markers for them of some kind I can drag, and the animation should only play
that range."

### What was there

Nothing draggable. The settings panel had two number boxes, `Trim from` and
`Trim to`, and a `Trim` button that cut the clip down to them immediately and
destructively. There was no way to say "play 10 to 70" without throwing the
other frames away, and no mark on the ruler for either number.

### What the range is now

A range OVER the clip, not a cut of it. Two new plain members on the clip
document (`src/hkxclipedit.h`):

    int rangeFirst = 0;
    int rangeLast  = -1;   // < 0 reads as the clip's last frame

`rangeLast < 0` means "the last frame", so a clip nobody has touched reports
`0 .. numFrames()-1` and `rangeIsPartial()` is false. Four accessors sit next
to `trim`: `rangeFirstFrame()`, `rangeLastFrame()`, `rangeIsPartial()`,
`setPlayRange(first,last)`. `setPlayRange` refuses a range that would keep
fewer than two frames and returns a sentence with both the frame numbers and
the seconds.

Where the range shows up:

| Place | What it does |
| --- | --- |
| The sheet's ruler | two pale tabs at the range's outer edges, dragged with the mouse; the pointer turns into a horizontal resize arrow over one |
| The sheet's body | everything outside the range is darkened (that is item 8, section 2) |
| The transport row | `Start` and `End` boxes, scrub-draggable like every other WW field, no native spin arrows |
| Ctrl+Home / Ctrl+End | put the range's start / end on the playhead |
| The scene | `animTags` gets `start` and `end` at `timeOfFrame(range...)`, so playback obeys the range -- the ruling's third clause |
| A SAVE | writes the range alone, so the .hkx duration is what the grips show |
| `summary()` | "Play range 10..70 (61 frames); a save writes the range." when partial |

The save does it on a COPY:

    if ( rangeIsPartial() ) {
        HkxClipDocument out = *this;
        out.rangeFirst = 0; out.rangeLast = -1;
        const HkxEditResult r = out.trim( rangeFirstFrame(), rangeLastFrame() );
        if ( !r.ok ) { error = r.message; report.error = error; return false; }
        return out.save( path, report, error );
    }

so the open document still has all its frames after a save. That is the whole
point of the ruling: non-destructive.

### The drag

`gripAt(x,y)` answers only inside the ruler band, catching 6 px either side of
the edge, and the END grip wins a tie (that is the grip a hand reaches for when
the range is one frame wide). The press installs `DragRangeStart` /
`DragRangeEnd` BEFORE `DragScrub`, so grabbing a grip can never scrub by
accident. The move calls `setRange` live so the darkened zone follows the hand;
the undoable command is pushed ONCE ON RELEASE, through
`AnimWorkspace::sheetRangeDragged` -> `edit(...)` -> `d.setPlayRange(a,b)` --
one command per gesture, not one per pixel.

`setDocument()` deliberately starts the sheet clean (no selection, whole clip
in range), so `rebuildRows()` puts the document's range straight back in after
it; without that line every edit would silently widen the range back to the
whole clip. `applyDocument()` does the same for the two boxes.

### What replaced the Trim controls

`Trim from` / `Trim to` are gone from the Range group; `Retime to` moved up to
grid row 0. The button is now "Trim to range" -- the same immediate,
destructive cut, reading the range instead of two boxes of its own, and it
refuses with a sentence when the range is already the whole clip (there is
nothing to cut). Gate (e) still gets its 41 frames, now by setting
`AnimWsRangeStart` / `AnimWsRangeEnd` and calling `setRangeFromBoxes()` first.

### Range invalidation -- stated, not silent

`trim()` and `retime()` both reset `rangeFirst = 0; rangeLast = -1`, because
the range was frame numbers on the OLD grid and those frames no longer exist.
A retime at a different rate would otherwise leave the grips pointing at
arbitrary times.

### Divergence from Blender, stated

Blender sets the preview range with bare S and E. A bare letter next to a
marker's inline name editor is a trap (it types into the field), so the range
keys here are Ctrl+Home and Ctrl+End. The comment in `src/animdopesheet.cpp`
says so at the point of use. The grips themselves are drawn the way Blender
draws its scrub-bar handles, in `textBright`, the nearest existing token to
Blender's `time_marker_line_selected` #FFFFFFB3.

### The gate: (l) in `src/animworkspacetest.cpp`

Driven as a hand drives it -- real `QMouseEvent` press / move / release on the
ruler at the pixel the grip is painted on. Every assertion checks FOUR numbers
at once (document, sheet, both boxes) plus the clip's frame count, because any
one of them can be right while the others drift apart:

1. the clip starts 0..92 of 93 frames and `rangeIsPartial()` is false;
2. start grip +10 -> 10..92, clip still 93 frames;
3. end grip -10 -> 10..82, clip still 93 frames;
4. typing End = 70 -> 10..70;
5. three gestures put exactly THREE commands on the undo stack;
6. the darkened-zone EDGE read out of the pixels: frames 5 and 80 darker,
   frames 15 and 65 not, and 15 == 65;
7. a save to `inapp_range.hkx` comes back 61 frames and 60 x frameDuration
   seconds, its frame 0 bit-for-bit the clip's frame 10, and the open document
   still 93 frames;
8. three undos -> back to 0..92, one gesture at a time.

THE FLOOR: the same save with nothing out of range (`inapp_range_whole.hkx`)
must come back 93 frames -- so the duration check is shown able to fail.

### Files touched (all LF-only; the edits added no CR)

| File | CR | LF | bytes |
| --- | --- | --- | --- |
| `src/hkxclipedit.h` | 0 | 214 | 10,359 |
| `src/hkxclipedit.cpp` | 0 | 1,125 | 39,466 |
| `src/animdopesheet.h` | 0 | 210 | 8,819 |
| `src/animdopesheet.cpp` | 0 | 999 | 30,597 |
| `src/animworkspace.h` | 0 | 312 | 12,334 |
| `src/animworkspace.cpp` | 0 | 2,059 | 74,041 |
| `src/animworkspacetest.cpp` | 0 | 1,105 | 59,385 |
| `tests/spells/animws.sh` | 0 | 98 | 4,653 |

Note for step 12: this tree's HEAD (720762a) does NOT contain the
animation-workspace files at all -- they were uncommitted work in the main tree
when the robocopy was taken, so `git show` has no baseline for them. The
"before" counts in `CHANGED_FILES.txt` are the counts I printed with Python
before each file's first edit, not git's.

**Status:** code and gate written, not yet compiled. The numbers above are byte
counts; the gate's own numbers get read out of `release/ww_animws_test.log` at
step 9. Pictures owed at step 10.

## 4. Items 7 and 7a — right-click anywhere, and the zoom that reset itself

**The rulings, verbatim.** 7 (01:56): "Now, why can't I right click and insert
an annotation anywhere?" 7a (01:58): "we need to add and remove annotations
with right click, also, when I zoom into the timeline and try to drag an
already existing annotation, the zoom resets to default zoom for some reason".

### 7a first, because it was a real bug with a measurable cause

`AnimDopeSheet::setDocument()` ended with `frameAll()`, which hard-sets
`view0` / `view1` to the whole clip. Every edit reaches it:

    AnimWorkspace::edit -> AnimWsCommand::redo -> applyDocument ->
    rebuildRows -> sheet->setDocument( THE SAME POINTER )

So it was never about annotations. **Any** edit threw the zoom away, and the
selection with it; the marker drag is just where he met it, because that is the
one gesture whose own feedback is the thing that moves.

The fix reads the pointer: an edit is not a new document. When the same
document comes back, the view, the selection and the play range stay --
the view clamped to the new length (a trim can leave the old window past the
end of the clip), and the selection pruned by a new `pruneSelection()` so no
orange diamond is left sitting on a key an edit deleted. A DIFFERENT document
(another clip picked in the list) still starts clean, which is correct: its
zoom and its selection are not this one's.

### 7: the menu comes up anywhere, and it names the clicked frame

`contextMenuEvent` answered only when `rowAtY( y ) >= 0` -- a bone row -- and
passed no x at all, so every entry it built read the playhead. It now fires
everywhere (the ruler and the marker row included) and carries three things:
the row or -1, the frame under the cursor (snapped, clamped), and the
annotation under the cursor or an invalid ref.

What the menu holds, in order:

| Where | Entries |
| --- | --- |
| on an annotation | `Rename annotation '<name>'...` (Ctrl+M), `Remove annotation '<name>'` |
| anywhere | `Add annotation at frame N` (M) |
| a bone row | `Insert key at frame N`, Select all keys on this track, Rename track..., Remove track, Isolate in the viewport |
| a float row | `Set float key at frame N`, Remove float track |
| anywhere | Put the playhead on frame N, Play range starts / ends at frame N |

N is the clicked frame throughout. `insertKeyAtPlayhead()` and
`addAnnotationAtPlayhead()` are now one-line wrappers over new
`insertKeyAtFrame(frame)` / `addAnnotationAtFrame(frame, name)`, so the button
and the menu cannot drift apart.

### The inline name editor, and why nothing is added until Return

`AnimDopeSheet::beginNewMarker( frame, vocabulary )` draws a hollow **ghost**
triangle at the frame and opens a child `QLineEdit` named
`AnimWsMarkerNameEdit` over the marker row, with the annotation vocabulary as
completions. The annotation is created only when Return commits
(`markerNameEntered` -> `sheetMarkerNameEntered` -> `addAnnotationAtFrame`), so:

- one gesture is **one** undoable command, not an add plus a rename;
- Escape (or clicking away) leaves **nothing** behind -- no empty annotation
  and no entry on the undo stack;
- the new annotation becomes the active one, so ruling 7b's orange lands on it.

`beginMarkerRename` uses the same editor. Double-click on a marker now renames
it in place instead of putting up a modal `QInputDialog` in the middle of the
window; the dialog code is gone, not left dead.

### Keys, and the divergence from Blender

- **M** adds an annotation at the playhead (Blender's marker key).
- **Ctrl+M** renames the active one. Blender uses **F2**; F2 is spoken for in
  this window, and Ctrl+M sits next to M, which is the key the same hand just
  pressed. Stated at the point of use.
- Escape cancels, Return commits -- Blender's marker-name popup behaves the
  same way.

The sheet's tooltip now names the right-click, M and Ctrl+M, as the tooltip
rule requires.

### The gate: (m) in `src/animworkspacetest.cpp`

The menu is modal (`QMenu::exec`), so it cannot be read from the line that
opens it. A 0 ms timer queued **before** the context-menu event fires inside
the menu's own event loop, copies out every entry's text, triggers the one
asked for and closes it. That reads the text a hand would read, which is
exactly where the clicked frame has to appear.

1. right-click on a bone row at frame 12 -> the menu says `Add annotation at
   frame 12` **and** `Insert key at frame 12`;
2. right-click on the **ruler** at frame 37 -> a menu exists at all (there was
   none before) and says 37;
3. right-click at frame 70 -> says 70 and **not** 37 (the floor: a menu still
   built from the playhead cannot pass all three);
4. right-click on the fixture's `FootLeft` marker -> Rename and Remove, both
   naming it;
5. triggering `Add annotation at frame 37` opens the editor with the ghost at
   37 and **nothing** in the document yet; Return puts `SlideStart` at frame 37
   for exactly **one** undo command, and it is the active marker;
6. M at frame 50 opens the editor; Escape adds no annotation and no command;
7. Ctrl+M opens the editor holding `SlideStart`; Return renames it to
   `SlideGo`;
8. `Remove annotation 'SlideGo'` from the marker's own menu takes it out;
9. **the zoom**: `setView( 20, 60 )`, then the `FootLeft` marker dragged 5
   frames with real mouse events -> the annotation moved exactly 5 frames, the
   visible range is unchanged to 0.01 frame, the dragged marker is still
   selected, and the clip still has 93 frames and the same annotation count;
10. every command the gate pushed is undone, back to `FootLeft` at frame 30,
    so gates (g) and (h) downstream see the fixture they expect.

**The floors:** the frame in the menu text differs at each of three x
positions, and `frameAll()` must **fail** the same zoom comparison (check 9),
so that comparison is shown able to catch a reset.

### Files touched (all LF-only)

| File | CR | LF | bytes |
| --- | --- | --- | --- |
| `src/animdopesheet.h` | 0 | 253 | 11,337 |
| `src/animdopesheet.cpp` | 0 | 1,240 | 39,012 |
| `src/animworkspace.h` | 0 | 324 | 13,122 |
| `src/animworkspace.cpp` | 0 | 2,156 | 78,554 |
| `src/animworkspacetest.cpp` | 0 | 1,345 | 73,923 |
| `tests/spells/animws.sh` | 0 | 115 | 5,909 |

**Status:** code and gate written. All ten files edited so far pass
`g++ -fsyntax-only` with the release flags (`-Wall -Wextra`, nothing printed),
which is not a build -- the one build and the gate numbers come at step 9.

## 5. Item 3 -- Remove transform axes (03:06)

His words: "Add an option here, under remove track, to remove all transforms in
specific directions, so x, y, z or a combination of them. Same goes for rotation
direction. That way, I can make it so that the slide stays in the center, but the
COM still moves downward when the player crouches."

### What it does

`src/hkxclipedit.h` gains `struct HkxAxisMask` -- six flags, translation X/Y/Z and
rotation X/Y/Z -- and `HkxClipDocument::removeTransformAxes( int track, const
HkxAxisMask & mask )`.

The reference value is FRAME 0, NOT ZERO. Zeroing a translation would teleport the
bone to the skeleton's origin; frame 0 is where the animation already stands, so a
stripped direction simply stops moving and everything else carries on. That was a
judgement call inside his ruling; it is written in the code comment and here. If he
wants a plain zero instead it is a one-line change and I will add it as a second
option. OPEN QUESTION FOR BUNGO, repeated under section 13.

Rotation uses NifSkope's own Euler triple (Matrix::fromQuat -> toEuler -> replace
the ticked components with frame 0's -> fromEuler -> toQuat) -- the same three
numbers a rotation row shows in the Blocks tab. A key whose rotation nobody ticked
is NOT CONVERTED AT ALL, so untouched quaternions stay byte for byte what they
were; that is what makes the "Z byte-identical" claim provable rather than a
tolerance. Keys at gimbal lock (toEuler returns false, the triple is not unique)
are counted and named in the sentence the workspace prints.

The operation ends with regenerateTrack( track ), so the dense frames the viewport
plays are rebuilt from the edited keys, and it returns a sentence: "Held
translation X, translation Y at their frame-0 value on track 0 (COM): N of 93 keys
changed, by up to X units and Y deg."

### How it is reached

`src/animworkspace.cpp`: the sheet's right-click menu carries "Remove transform
axes..." DIRECTLY UNDER "Remove track", which is where he put it. It opens a small
dialog, object name AnimWsAxisDialog: a Translation row and a Rotation row of three
boxes each (AnimWsAxisTX/TY/TZ/RX/RY/RZ), one line naming the rotation convention
(AnimWsAxisConvention), Cancel and Remove. Nothing else -- no blurb. The convention
line is the one sentence the ruling itself asks for; it states a fact about the
numbers, not a description of the feature.

Menu, dialog and harness all end at one public slot,
AnimWorkspace::applyAxisStrip( int track, const HkxAxisMask & ), which wraps the
document call in the workspace's usual one-gesture-one-command edit(...), so it is
undoable like everything else and the three ways in cannot drift apart.

### Gate (n), in `src/animworkspacetest.cpp`

On his fixture -- Running_To_Slide_And_Back_To_Running on
fixtures/human_male_vanilla.nif, COM track, 93 keys:

- floor first: before the strip COM must travel more than 1 unit in X, Y AND Z,
  and the three spans are printed, so a no-op cannot pass as a success;
- the right-click menu must carry "Remove transform axes..." at exactly the index
  after "Remove track" (read out of the live QMenu by a 0 ms timer queued before
  the modal exec, the same trick gate (m) uses);
- the dialog is driven the way a hand drives it: a second queued timer finds
  AnimWsAxisDialog in its own modal loop, checks that all six boxes and the
  convention line exist, ticks translation X and Y, and clicks Remove;
- after it: X and Y have span exactly 0 over all 93 keys and equal frame 0's
  values; Z's span is unchanged and still greater than 1 (his downward motion
  survives); Z is compared with memcmp key by key and must be bit-identical, and
  so must every rotation quaternion;
- the summary line names both held directions and is not a refusal;
- exactly one undo command was added, and consistent() holds (dense frames agree
  with the keys);
- the clip is saved as inapp_axisstrip.hkx, read back with our own reader, and
  must equal the edited document bit for bit;
- one Undo restores the clip byte for byte and the undo count returns.

`tests/spells/animws.sh` documents (n) and its floor in WHAT IS MEASURED.

### A real problem found on the way: my syntax checks were doing nothing

I had been "verifying" each edit with g++ -fsyntax-only using the flags out of
Makefile.Release, and reading "no output" as "clean". At 02:5x I applied to the
checker the rule I apply to everything else -- PROVE IT CAN FAIL -- and fed it a
file containing `return notAThing;`. It printed nothing. The check had never run:
Git Bash's PATH does not carry /c/msys64/ucrt64/bin, so g++ started but cc1plus
could not load its own DLLs and died silently (exit 1, no message, which is exactly
what a clean run looked like to me).

With PATH fixed and the flags passed through a response file (the Makefile's \" around
NIFSKOPE_VERSION needs quoting the shell was eating), the checker now prints the
deliberate error and stays silent on good files. Re-run over every file this lane has
touched, it found TWO REAL DEFECTS that would have broken the one build I am allowed:

1. src/animworkspacetest.cpp:514 (gate (k), written earlier in this lane):
   std::max( 0, sheet->visibleRows().indexOf( thighRow ) ) -- int against qsizetype,
   no matching std::max. Fixed with an int(...) cast.
2. src/nifskope.cpp:8609 (item 1, the status bar's replacement):
   showTransientMessage(...) called from the const reader extractConfiguredNifBytes.
   The old code reached the status bar through the const statusBar() accessor, which
   is why it compiled before. Fixed with a const_cast at that one call site and a
   comment saying why the transient line is window furniture, not document state.

All seven touched .cpp files are clean now (nifskope.cpp, nifskope_ui.cpp,
uialigntest.cpp, animworkspace.cpp, animdopesheet.cpp, animworkspacetest.cpp,
hkxclipedit.cpp), with -Wall -Wextra on. This is still NOT the permitted build and
nothing is linked; it is a compile of each file on its own.

Mistake recorded for section 14 and for MISTAKES_ENTRIES.md: A VERIFICATION TOOL GETS
THE SAME TREATMENT AS A MEASUREMENT -- run it against a case it MUST fail before
believing a pass.

## 6. Item 5 -- the Animations list: shortcuts, right-click menu, drag-and-drop (03:18)

His words: "For these animations, I should be able to use a shortcut to delete,
copy, paste, etc. them, same goes with reordering with a drag and drop, same goes
with right clicking and selecting each such option."

### Where the order lives (he asked for this to be said)

The list shows two kinds of row:

- the NIF's own NiControllerSequence blocks, at the top. Their order IS a file
  order -- it is the order of the blocks in the NIF. Changing it means moving
  blocks and re-pointing the links that name them, which is a Blocks-tab edit, so
  these rows REFUSE every action here with one sentence saying where to do it.
- the loaded .hkx clips, under them. Each of those is its own file, so their order
  is the session's and not any file's: a reorder writes nothing to disk, and
  nothing is lost by putting them back in another order tomorrow. If he ever wants
  that order remembered between sessions, say so and it becomes a setting.

A row dropped above the sequences comes back at the top of the clips, and the app
says so in one line rather than pretending the drop did nothing.

### What is there now

All of it lands on one slot per action, so the keyboard, the menu and the mouse
cannot drift apart:

| action | key | menu entry |
| --- | --- | --- |
| Delete | Del, and X (Blender's) | Delete |
| Copy | Ctrl+C | Copy |
| Paste (after the selected row) | Ctrl+V | Paste |
| Cut | Ctrl+X | Cut |
| Duplicate | Shift+D | Duplicate |
| Rename in place | F2, or a double-click | Rename |
| Select all | Ctrl+A | Select all |
| Move up / down | Ctrl+Up / Ctrl+Down | Move up / Move down |
| Reorder | drag the row (Qt draws the drop line) | -- |

Shift and Ctrl pick several rows; the CURRENT row is still the one that drives the
scene, so multi-select never changes what is playing by itself. The list's tooltip
names every action with its key, which is what the no-descriptions rule allows.

New code underneath, so the list is not editing things behind the app's back:
`HkxPlayback::setOrder / duplicateClip / renameClip / insertClip` (plus a private
`syncSceneOrder`), each keeping the SCENE's own animations list -- the one every
picker reads -- in the same order, and moving the active binding with a rename.
`WwHkxAnimHub` gains `duplicateEntry / pasteEntry / renameEntry / setEntryOrder /
clipEntry / clipIndex`, which is the one door the workspace knocks on.

A copy keeps the clip and its track names, not the binding: a paste works the
binding out again against the NIF that is open now. A rename carries the edited
document with it by hand, because the hub's "a clip went away" signal cannot tell
a rename from an unload.

### Gate (o), in `src/animworkspacetest.cpp`

The fixture has one clip, so the gate makes its three clips with the Duplicate it
is testing; if Duplicate is broken, nothing after it can pass, which is the right
way round. After every action the row names are read back in order from THREE
places -- the list widget, the loaded clips themselves, and the scene's animations
list -- so a row that moved only on screen fails.

Two things cannot be driven from inside the application, and are measured in
halves rather than pretended:

- a key press only reaches a QShortcut when it comes from the platform, so the
  gate asserts each shortcut's KEY (the binding a hand would press: Del, X,
  Ctrl+C, Ctrl+V, Ctrl+X, Shift+D, F2, Ctrl+A, Ctrl+Up, Ctrl+Down) and then fires
  that same shortcut object's activated signal, which is the connection the key
  would have used;
- QDrag::exec() runs a nested platform loop that never returns inside a harness,
  so the gate moves the rows the way the view's internal move leaves them and
  delivers the Drop event to the viewport. Everything of ours then runs for real:
  the event filter, the order push, the hub, the clips, the scene list. What is
  NOT measured by that is Qt's own drag loop and the drop line it paints; the gate
  says so and asserts instead that the rows are drag-enabled and the view is in
  internal-move mode.

The right-click menu is opened for real (a context-menu event on the viewport),
every entry is read out of the live QMenu, the menu is photographed to
`animws_list_menu.png` in the harness's out directory, and one entry ("Move down")
is chosen IN the menu so the menu path itself moves a row.

The gate ends by putting the fixture back exactly as it found it (one clip, 93
frames), so the gates after it see what they expect.

`tests/spells/animws.sh` documents (o) and its floor.

### Owed on this item

- Nothing here is written to disk, so nothing can be lost; but a clip that was
  renamed keeps its new name only for the session. A save writes the clip's own
  internal name, which is a separate question -- bungo's call whether Rename
  should also rewrite the name inside the file on the next save.
- The NIF sequence rows refuse; if he wants Move up / Move down to reorder NIF
  blocks I need his word first, because that rewrites the file's block order.

## 7. Items 6 and 6a -- the button row and the Keys block (code 03:37-03:38, gate 03:42)

His words (item 6, 01:49), said over the Keys panel with fifteen buttons under
it: "Also, for this... it's getting pretty crowded in here, isn't it?" And
item 6a (01:51): "Why not add another panel in the animation manager, that
opens from the right side, that contains more stuff."

### The bottom button row is gone; its fifteen actions are menus now

Every button became a `QAction` **under the same object name and the same
text**, so anything that looked a button up by name still finds it, and the
context menus that already pointed at them did not change:

| Menu (object name) | Entries, in order |
| --- | --- |
| Clip (`AnimWsMenuClip`) | Trim to range, Retime -- separator -- Bake root, Unbake -- separator -- Save, Save as... |
| Key (`AnimWsMenuKey`) | Insert key, Delete -- separator -- Reduce |
| Channel (`AnimWsMenuChannel`) | Rename track, Remove track, Remove transform axes... -- separator -- Add float track, Set float key |
| Marker (`AnimWsMenuMarker`) | Add annotation, Rename, Delete annotation |
| View (`AnimWsMenuView`) | Side panel (N, checkable) -- separator -- Frame all |

The menu bar is `AnimWsMenuBar` inside `AnimWsHeader` at the top of the dock,
`setNativeMenuBar( false )` so it stays in the dock instead of jumping to the
system menu bar, and `setToolTipsVisible( true )` so the sentence each button
used to carry is still one hover away. The menu row itself is label-only,
which is the no-descriptions rule.

Two entries are new in kind rather than in code: **Delete annotation** used to
exist only on a marker's own right-click menu, and **Remove transform axes**
(item 3) had no home except the Channel menu -- both are reachable from the
header now.

### The Keys block is gone

Its two gizmo switches are two checkable buttons in the transport row,
`AnimWsPose` ("pose") and `AnimWsAutoKey` (a filled dot), which is where
Blender keeps its auto-key record button. The two reduce tolerances moved into
the Key section of the new panel. Nothing was dropped.

### The panel opens from the right edge

It is the third pane of the dock's own splitter (`sidePanel`, minimum width
160), so it opens on the right edge, drags to any width, and the width survives
a restart: `AnimWorkspace/sidePanelWidth` and `AnimWorkspace/sidePanelOpen` in
QSettings, written on every splitter move and on every toggle. It is opened
three ways -- the arrow button at the right of the header
(`AnimWsSidePanelBtn`), View > Side panel, and the **N** key.

`setSidePanelOpen( bool )` takes the width it needs from the sheet rather than
from the whole dock, never lets the panel fall below 160, and blocks the
button's and the action's signals while it syncs them, so the three ways cannot
fight each other.

### One section per selection -- the part that actually answers "crowded"

| What is selected | What the panel shows |
| --- | --- |
| a clip open, nothing selected | Clip (root motion track, retime rate) |
| a bone row | Clip + Track -- the track's name in an editable field |
| keys | Clip + Key -- reduce tolerance, reduce rotation |
| an annotation | Clip + Annotation -- the event name |
| a float row | Clip + Float track -- the value |
| a NIF sequence | Sequence only (cycle type, frequency, start, stop), unchanged |

Clip stays visible while a clip is open because it is the subject of the whole
dock; the other five are exclusive, in the order annotation > keys > float row
> bone row. `updateSections()` runs off the same refresh the summary line uses,
so it follows the selection with no new signal wiring.

The Track section's field renames the track on `editingFinished`
(`trackNameEdited()`): empty or unchanged reverts the text and does nothing,
otherwise it is one undo step named "Rename track <name>". It refills itself
from the document whenever it does not have the keyboard, so it never fights
what the user is typing.

### Divergences from Blender, named

* **N works while the sheet or the list has the keyboard, not everywhere in the
  dock.** Blender's N works anywhere in the editor. A shortcut with Qt's
  `WidgetWithChildrenShortcut` context on the workspace would swallow the letter
  n from the annotation Name field and the track Name field, and one has to be
  able to type "n" there. So the shortcut is installed twice, on the dope sheet
  and on the list (`AnimWsSidePanelKey`, `AnimWsSidePanelKey2`).
* Blender's sidebar sections are collapsible headers one can fold shut. Ours
  are shown or hidden by what is selected. Folding a section that is only
  present when it is relevant buys nothing, and his sentence is about crowding.
* The transport row is not Blender's header row and did not move; item 4 (the
  icons) is about that row and is the next item.

### The gate: (p)

Written into `src/animworkspacetest.cpp` between gates (n) and (o), documented
in `tests/spells/animws.sh`. What it measures:

1. **the row is really gone** -- no `AnimWsActionBar`, and the number of
   `QPushButton`s left anywhere in the dock is printed and must be zero;
2. **nothing was lost on the way to the menus** -- all sixteen former button
   texts are found by walking `menuBar->actions()` and each top action's
   `menu()->actions()`; Save and Save as are read specifically out of the Clip
   menu, because that is where the ruling puts them;
3. **the panel shows one section per kind** -- with the panel forced open, a
   bone row, keys, an annotation and a float row are each selected in turn and
   **every visible section is named in the log line**, so a panel that showed
   all five would fail the check a correct one passes;
4. the Track field reads `COM` when the COM row is selected;
5. the panel is dragged to 240 px, closed, reopened, and must come back within
   8 px of 240;
6. the N shortcut's `key()` is asserted to be N and then its `activated` signal
   is invoked -- a synthesized key event never reaches a `QShortcut`.

**Its floors.** "Frobnicate" is put through the same menu search and must NOT
be found, so a search that returns true for anything cannot pass item 2. And
the height of the row that was retired is not remembered from the old source:
one real `QPushButton` is built with the same text, its `sizeHint()` is taken,
the old bar's margins and spacing are added, and the new header must be no
taller than that -- a measurement, not a number typed from memory. The dock's
whole fixed chrome (header + transport + note) is printed, so the before/after
his ruling asks for can be read off the log next to the before-exe picture at
step 10.

### Four harness lookups repointed in the same change

A rename that left these pointing at retired widgets would have left green
gates measuring nothing:

* `animworkspacetest.cpp` (b) "the Insert key button is enabled" and (f)
  "Unbake is the enabled one" -> `findChild<QAction*>` by the same names;
* `animworkspacetest.cpp` (j) and `hkxanimuitest.cpp` (g) asked whether
  `AnimWsActionBar` sits outside the splitter -> they ask it of `AnimWsHeader`,
  which is the thing that must stay pinned now, with a comment saying why the
  name changed;
* gate (j)'s floor "at least 20 buttons in the dock" was only true while
  fifteen push buttons sat at the bottom. It is **12** now (eight transport
  buttons, the two gizmo toggles, the list bar's three, the panel toggle), the
  check prints the count it saw, and the reason for the lower floor is in the
  check's own text so nobody later reads it as a weakened test.

Gate (i) reads the Sequence rows' visibility and those rows live inside the
panel now; a closed panel would have made them invisible and turned (i) red on
correct code. (i) and the panel-style block therefore call
`setSidePanelOpen( true )` first -- a harness forces the state it measures.
That is also why `setSidePanelOpen` is a **public** slot; the header says so in
a comment.

### Files touched (all LF-only, no CR anywhere)

| File | CR | LF | bytes |
| --- | --- | --- | --- |
| `src/animworkspace.h` | 0 | 415 | 17,357 |
| `src/animworkspace.cpp` | 0 | 2,835 | 105,969 |
| `src/animworkspacetest.cpp` | 0 | 2,124 | 114,779 |
| `src/hkxanimuitest.cpp` | 0 | 794 | 32,450 |
| `tests/spells/animws.sh` | 0 | 152 | 8,687 |

**What is proven and what is not.** `g++ -fsyntax-only` with the release flags
prints nothing for `animworkspace.cpp`, `animworkspacetest.cpp`,
`hkxanimuitest.cpp`, `nifskope.cpp` and `nifskope_ui.cpp`, and on the same run
a deliberate `return notAThing;` was rejected, so the checker was awake. That
is not a build and gate (p) has never executed. The one build and every gate
number come at step 9.

### Owed out of this item

Item 6's second bullet also asks that item 5's clip rules reach keys and
tracks: "Delete, Ctrl+C/V, Shift+D on keys, G to move, box select". Delete,
G-move and box select are already on the sheet. A key **clipboard** -- copy
keys with their frames, paste at the playhead, onto which track -- is a new
document operation, it is not in this item, and it is listed in section 13 as
owed rather than quietly dropped.

## 8. Item 4 -- the transport icons and the clipped header (03:46-03:59)

His words (01:45): "Most of these icons are very bad looking, and unclear to
what they do. They need to be better." And the note at the end of the same
ruling: "Load…" and "root" in the Animations header row are clipped.

### The icons

The row was eight buttons whose faces were typed characters -- `|◀`, `◆◀`,
`◀`, `▶`, `■`, `▶◆`, `▶|` and the word "loop". A character's size and weight
belong to the font, not to us, which is exactly why they were mixed: the
key-jump pair read as a play arrow with a speck next to it.

They are drawings now. One private function, `wwTransportPixmap`, draws all ten
glyphs on the SAME 16x16 grid, from the same three primitives:

* one triangle (tip on the grid's middle line, back five units high),
* one bar, two units wide and ten tall,
* one diamond, three units across.

So the play arrow, the two jump arrows and the two key arrows are literally the
same triangle, and the start/end bars are the same bar. Two units of stroke is
the only weight in the set. The ink is `wwSkinColor("text")`, with a second
`Disabled` pixmap in `textMuted`, so a disabled transport button greys the way
every other disabled thing in the skin greys instead of staying black.

| Button | The drawing |
| --- | --- |
| To start / To end | bar + triangle pointing at it |
| Previous key / Next key | triangle + the keyframe diamond, on the side it travels to |
| Play / Play backwards | the triangle, and the PAUSE bars while it is playing |
| Stop | the square |
| Loop | two arcs of one circle, each with the same arrowhead -- the two-arrow cycle the ruling asks for |
| Auto-key | the record dot |

The play button turns into a pause button while it plays (Blender does the
same), so the face says what pressing it will do; the drawing is only rebuilt
when the state actually changes, because that code runs on every refresh.

**The divergence, named.** The ruling says "our own SVG". These are the same
vector shapes, drawn with `QPainterPath` instead of parsed out of an `.svg`
file. NifSkope links no Qt Svg module, and neither `Qt6Svg.dll` nor the `qsvg`
image plugin is deployed beside the exe -- real .svg files mean a deploy change
this lane cannot test, and a missing plugin shows up as icons that silently
fail to load. The shapes, the one weight, the one size and the palette are what
the ruling is about; the file format is not. Bungo's call to overturn, and it
is in section 13.

### The tooltips, and the shortcuts they name

The ruling asks for tooltips that say "the action AND its shortcut". Five of
these buttons had no shortcut at all, so Blender's own timeline keys are now
installed -- on the dope sheet only, the same reasoning as the N key: the list
needs Up and Down for its rows, and a text field needs its space bar.

| Button | Tooltip | Key |
| --- | --- | --- |
| To start | Jump to the first frame (Shift+Left) | Shift+Left |
| Previous key | Jump to the previous key on the selected row, or on any row when none is selected (Down) | Down |
| Play backwards | Play backwards (Shift+Ctrl+Space) | Shift+Ctrl+Space |
| Play | Play, and pause while it plays (Space) | Space |
| Stop | Stop playing | none |
| Next key | Jump to the next key on the selected row, or on any row when none is selected (Up) | Up |
| To end | Jump to the last frame (Shift+Right) | Shift+Right |

Stop has no key and its tooltip does not claim one -- Blender has no stop
button at all, and inventing a key for it would collide with something real.
Left and Right (step one frame) and Home (frame all) were already on the sheet
and did not move.

### Loop and speed

Loop was the word "loop"; it is the cycle drawing now. Its icon and tooltip
are set on the **action**, not on the button: a QToolButton with a default
action re-reads text, icon, tooltip and checked state from that action every
time the action changes, so anything set on the button alone is thrown away the
first time Loop is toggled. The Animation menu's own Loop entry therefore shows
the same drawing, which is the point of having one set.

Speed read `x1.50`. The ruling asks for "a labelled spin, '1.50x'", so the `x`
moved from prefix to suffix and the tooltip says what 1.00x means.

### The clipped "Load… / root" header

The cause, measured rather than guessed: the "Animations" heading had **all**
the stretch in that row (`addWidget( heading, 1 )`). It grew to whatever the
column was; when the column was narrower than heading + three buttons, the
layout took the difference out of the buttons, and Qt clips a tool button's
text rather than shrinking it politely.

Two changes, both small:

* the stretch is an empty spacer between the heading and the buttons now, so
  the three buttons keep the width they ask for and the spacer is what
  collapses;
* the left column is given a **minimum width measured from the widgets
  themselves** -- the three buttons' own `sizeHint()`s plus the heading's plus
  the row's margins -- so the splitter cannot squeeze it under that either.

### The gate: (q)

1. every transport button carries a non-null icon, none is a text glyph any
   more (`toolButtonStyle == IconOnly`), and every icon size is printed and
   must be the same one number;
2. **every tooltip is compared to the exact sentence it should carry** -- which
   is the ruling's own gate, "every button's tooltip text asserted by the
   harness";
3. the six shortcuts' key bindings are asserted, and two of them (Shift+Right,
   Shift+Left) are fired with the playhead read back at both ends -- fired, not
   typed, because a synthesized key event never reaches a QShortcut;
4. the speed field must read `1.50x`;
5. no button in the "Load… / root" row may be narrower than the width it asks
   for;
6. the bar is saved as `transport_1x.png` and `transport_2x.png` -- the 1:1 and
   2:1 picture the ruling asks for.

**The floors.** A sentence no button carries ("Frobnicate the sprocket
(Ctrl+Q)") is run through the same tooltip comparison and must not match. And
the clipping predicate is proved able to fail **on the same run**: the left
column is squeezed to 40 px, the same predicate must report the header clipped
there, and only then is the restored width allowed to pass. A green "nothing is
clipped" that had never been shown able to go red would not be worth reading.

### Files touched

| File | CR | LF | bytes |
| --- | --- | --- | --- |
| `src/animworkspace.cpp` | 0 | 3,034 | 114,439 |
| `src/animworkspacetest.cpp` | 0 | 2,333 | 125,744 |
| `tests/spells/animws.sh` | 0 | 169 | 9,994 |

`g++ -fsyntax-only` prints nothing for `animworkspace.cpp`,
`animworkspacetest.cpp` and `hkxanimuitest.cpp`, and on the same run a
deliberate `return notAThing;` appended to each of the two edited .cpp files
was rejected with the line number, so the checker was awake for this item too.
Gate (q) has never executed: no build has run this lane.

## 9. All nine rulings, and where each one is

| Ruling | Section | Gate | State |
| --- | --- | --- | --- |
| 1 -- the bottom bar goes | `## 1` | `ui_align.sh` (s1)(s2) | code in, never run |
| 2 -- selected keys orange | `## 2` | `animws.sh` (k1)-(k8) | code in, never run |
| 7b -- annotation orange / orange-red | `## 2` | same | code in, never run |
| 8 -- blue playhead, darkened outside | `## 2` | same | code in, never run |
| 9 -- draggable play range | `## 3` | `animws.sh` (l) | code in, never run |
| 7 -- right-click an annotation anywhere | `## 4` | `animws.sh` (m) | code in, never run |
| 7a -- the zoom that reset itself | `## 4` | `animws.sh` (m2), measured RED first | code in, never run |
| 3 -- Remove transform axes | `## 5` | `animws.sh` (n) | code in, never run |
| 5 -- the Animations list | `## 6` | `animws.sh` (o) | code in, never run |
| 6, 6a -- the button row, the right panel | `## 7` | `animws.sh` (p) | code in, never run |
| 4 -- the transport icons, the clipped header | `## 8` | `animws.sh` (q) | code in, never run |

"Never run" is the whole point of section 10: every gate in this lane exists as
code and none of them has produced a number, because no build has happened.

## 10. Build and chain -- BUILD PENDING, and why

**`Fallout4.exe` is running.** `Get-Process` at 04:01:56 reads pid 22908,
started 03:48:37, and it was already up at the first check. The standing rule
is no builds while the game runs, so this lane stops at the link and hands the
build on rather than taking the risk.

What was done, because none of it is a build:

1. **`release/NifSkope.before_uinotes1.exe`** -- the baseline rung aside at
   04:00 with its timestamp preserved: 2026-09-11 23:26:29, 21,484,032 bytes,
   sha256 `a77ede97ff487ec766a53ea79c8b848b1552d5ab0413d3221ee627a8f137496d`.
   Every "before" picture must come from this exe.
2. **`qmake NifSkope.pro`** -- run first, as the brief requires, and it mattered
   more than expected: the Makefiles the robocopy brought across still pointed
   at the OTHER tree. `Makefile.Release` line 43 read
   `RES_FILE = E:/Projects/NifskopeWildWastelandEdition/GeneratedFiles/.obj/icon_res.o`,
   so a `make` in this tree would have written an object file **into lane
   TILING4's tree**, which the charter forbids outright. After qmake the same
   line reads `E:/Projects/NifskopeWWE_ui/...`. qmake exits 0; its only
   messages are the usual "lupdate / lrelease could not be found".
3. **`g++ -fsyntax-only`** with the release flags (`scratchpad/flags.rsp`, which
   now lives in the tree) on every file this lane touched that a compiler can
   reach: `animworkspace.cpp`, `animworkspacetest.cpp`, `hkxanimuitest.cpp`,
   `nifskope.cpp`, `nifskope_ui.cpp`. Clean. And on the same runs a deliberate
   `return notAThing;` appended to the file under test was rejected with its
   line number, so the checker was awake -- this lane has already been bitten
   once by a syntax checker that was silently a no-op (section 14).

Two warnings came out of `nifskope_ui.cpp` and are left alone on purpose:
`_USE_MATH_DEFINES` redefined (the flags file defines it and the file's first
line defines it again), and a `/*` inside a comment at line 27056, which is the
`Settings/Render/Lighting/*` in an existing comment written by an earlier lane.
Neither is this lane's, both are harmless, and editing a comment inside a 1.5 MB
file would add a merge hunk for nothing. They are reported here so the merger
knows the checker read the whole file rather than stopping early.

**No NifSkope was running at any point of this lane** (`Get-CimInstance
Win32_Process` for `NifSkope.exe` returns nothing at 04:01), so no GUI harness
was started, no window was renamed or killed, and bungo's own window was never
touched. No `--port` was needed and none was taken.

### The exact resume, in order

```
# 1. the game must be down
tasklist | grep -i fallout4          # must be empty
# 2. this tree only; qmake has ALREADY been run, do not skip it if the tree moves again
cd /e/Projects/NifskopeWWE_ui
mingw32-make -f Makefile.Release -j8 2>&1 | tee scratchpad/uinotes1_20260912/build.log
# 3. count the relinks and read the new exe's time and size
stat -c '%y %s' release/NifSkope.exe
# 4. the chain, second monitor, own unused port, after checking tasklist for a
#    NifSkope started from the MAIN tree with --port (lane TILING4's)
tests/spells/animws.sh            # (a)-(q), the nine rulings' own gates
tests/spells/hkxanim_ui.sh        # the dock's older contract, incl. (g) AnimWsHeader
tests/spells/ui_align.sh          # (s1)(s2) the retired status bar
tests/spells/water_ui.sh          # SKIP unless the build touched water -- it did not
tests/spells/files_tab.sh         # SKIP: this lane touched no file browser code
tests/spells/top_bar.sh           # RUN: the main window's chrome moved in item 1
tests/spells/skeleton_overlay.sh  # RUN: the dock drives the skeleton overlay
```

Which of those are worth running and why is in the list itself; the three
marked SKIP are named with their reason rather than quietly dropped, as the
"pick relevant harnesses" rule asks.

**Expect the first `make` to be large.** `animworkspace.h` changed, and five
translation units include it; `nifskope_ui.cpp` alone is 1.5 MB. Counted relinks
and the exe's time and size belong in this section when it runs -- they are
missing here because they do not exist yet, not because they were forgotten.

## 11. Pictures -- OWED, none taken

`scratchpad/uinotes1_20260912/images/` is empty. Both halves need a running
NifSkope:

* **before** -- the 23:26:29 exe, now `release/NifSkope.before_uinotes1.exe`:
  the dock as it was, the transport bar at 1:1 and 2:1, the "Load… / root"
  header clipped, the fifteen-button bar and the Keys block;
* **after** -- the built exe: the same four, plus the right-side panel open at
  each selection kind, plus the 1:1 dock overview the brief asks for.

Gate (q) saves `transport_1x.png` and `transport_2x.png` itself when
`WW_ANIMWS_OUT` is set, and gate (p) prints the chrome height, so the
before/after comparison his ruling asks for can be read off the logs as numbers
even before the pictures exist.

No GUI was launched this lane at all -- see section 10.

## 12. MERGE LIST

The merge list lives in `scratchpad/uinotes1_20260912/CHANGED_FILES.txt` and is
kept current after every work step; it carries `A`/`M`, one relative path per
line, and CR and LF counts before and after with the byte size. Its own "what
is red in this list" block names the two honest gaps: the `?` before-counts that
this tree cannot recover (HEAD `720762a` does not carry the animation-workspace
files at all, and is not the robocopy baseline for the files it does), and
`src/nifskope.cpp`'s CR/LF arithmetic, which does not close and must be
re-derived by whoever merges against the main tree's copy.

Every file this lane touched is LF-only and measures CR 0 **except**
`src/nifskope.cpp`, which is a mixed file and was only ever edited by binary
splice, never normalised.

## 13. Owed / red / bungo's calls

**Owed, named rather than dropped:**

1. **A key clipboard** (ruling 6's second bullet): Ctrl+C / Ctrl+V / Shift+D on
   KEYS. Delete, G-move and box select are already on the sheet; copying keys
   with their frames and pasting them at the playhead is a new document
   operation, and the question of which track a pasted key lands on has no
   obvious answer. Not started.
2. **Every gate number.** No build has run: every gate in this lane, (k1)
   through (q), is code that has never executed. Nothing here is proven beyond
   `g++ -fsyntax-only`.
3. **Every picture** (section 11).

**Bungo's calls:**

4. **SVG vs drawn paths** (item 4). The ruling says "our own SVG"; the icons are
   the same vector shapes drawn with QPainterPath, because NifSkope links no Qt
   Svg module and neither `Qt6Svg.dll` nor the `qsvg` plugin is deployed. If he
   wants real .svg files, that is a deploy change and a separate lane.
5. **Rename and the clip's internal name** (item 5). Renaming a row renames the
   entry in the list; whether saving should also rewrite the clip's own internal
   name inside the HKX is his call. Today it does not.
6. **Move up / down and NIF block order** (item 5). Reordering moves the loaded
   clips in the list and in the scene's animation list. It does NOT reorder the
   NIF's own sequence blocks; doing that is a file edit with consequences
   outside this dock.
7. **Item 6b, the Material Manager**, was ambiguous in the rulings file and is
   not touched by this lane.
8. **Frame 0 as the reference** (item 3). "Remove transform axes" sets the
   chosen components to the track's frame-0 value on every key. If he wants the
   mean, or the value at the playhead, that is one line.

**Red:**

9. The N key and the six transport keys work while the SHEET has the keyboard,
   not anywhere in the dock -- a named divergence from Blender, made so the
   letter n and the space bar can still be typed into the name fields.
10. Gate (j)'s button-count floor was lowered from 20 to 12 deliberately when
    the fifteen push buttons left the dock. The check prints the count it saw
    and the reason is in the check's own text, but it IS a lowered floor and a
    reviewer should see it as one.

## 14. Mistakes

Four entries, all in `scratchpad/uinotes1_20260912/MISTAKES_ENTRIES.md`, ready
for the root `MISTAKES.md`:

1. **A mixed-line-ending file cannot be edited with string search.**
   `src/nifskope.cpp` is CRLF/LF mixed; it is spliced in binary, and its line
   endings are measured with Python byte counts, never with grep.
2. **I stripped guards mechanically and left `if ( true )` behind.** Removing
   the condition is not the same edit as removing the guard; five sites had to
   be looked at one at a time.
3. **I finished a whole work step before writing anything to the report.** The
   charter says write after every step. Corrected at 02:14, and the rule held
   for the rest of the lane: the section goes in before the next step's first
   edit.
4. **I typed a timestamp from a feeling instead of reading the clock.**
   `PENDING.md` said step 7's code went in at 03:52 when the clock read 03:45
   and the file mtimes were 03:37-03:38. Corrected everywhere, and every time
   in this report now comes from `date` or from `stat` on the file being
   described.

A fifth, carried in from earlier in the lane and already in the ledger: the
syntax checker was a silent no-op until its PATH was fixed, which is why every
`-fsyntax-only` claim in this report is paired with a refuter run that failed on
the same command line.

## 15. Finished-work skill review

**What earned its keep:**

* **One atomic Python script per multi-file item.** Both of the last two items
  (the QAction conversion across four files, and the icons across two) were one
  script that asserts every anchor is unique and writes nothing until every
  substitution has succeeded. Twice this lane a script aborted on a bad anchor
  and left the tree untouched, which is the whole point. Worth writing up as a
  skill of its own: `ww-atomic-multifile-edit`.
* **Repointing the harness in the same change as the rename.** Four checks
  would have gone on passing against widgets that no longer exist. The rule
  that fell out of it: when an object's TYPE changes, grep the harnesses for its
  name before touching anything else, and repoint them in the same commit.
* **A floor for every new number.** (p) proves "found in a menu" can fail with
  "Frobnicate"; (q) proves "nothing is clipped" can fail by squeezing the column
  to 40 px first. Neither floor costs more than five lines.

**What I would change in the brief for the next lane of this shape:**

* The brief's own gate for item 6 ("dock chrome height before/after") needs the
  BEFORE number from a running old exe, which a lane that cannot build cannot
  get. (p) works around it by building a real QPushButton and measuring that
  instead -- worth making the standard trick when a "before" is a retired
  widget's size.
* A lane that may be blocked from building should ring the baseline exe aside
  and run `qmake` FIRST, at the start, not at the build step. Here qmake turned
  out to be protecting the other lane's tree from a stray object file, and it
  would have been better to know that at 02:00 than at 04:00.

## Interruption note -- written 03:31 for a PC restart that did not come

Written at 03:31 before the machine was to go down, so nothing would depend
on my memory. The restart had not happened by 03:45 and the lane simply kept
going; the two files below are kept current after every item from here on, as
the coordinator asked. Left in place as the dated record of what existed at
03:31:

* `scratchpad/uinotes1_20260912/PENDING.md` -- the charter, the per-item state,
  the settled step-7 design, the syntax-check recipe, steps 8-12 and the owed
  list.
* `scratchpad/uinotes1_20260912/CHANGED_FILES.txt` -- the merge list as it
  stands, A/M prefixed, with CR and LF counts and an honest note on which
  "before" counts this tree cannot recover (HEAD 720762a does not carry the
  animation-workspace files at all, and is not the robocopy baseline for the
  files it does carry).
* Every source file on disk is whole; six of the nine items are in (1, 2, 7b, 8,
  9, 7, 7a, 3, 5 by the brief's numbering -- steps 1 to 6 of the work order).
* No build has been run this lane. Nothing here is proven to compile beyond
  `g++ -fsyntax-only`; no gate has produced a number yet.

## Build (UINOTES1b)

Written 2026-09-12 05:5x (times from `date +%H:%M` and from `stat` on the files
described). This section is appended by the resume lane; nothing above it has
been altered.

**The two owed chains were run by lane ROADS4 on 2026-09-12, 06:10:16 to 06:22:33 (times from `date`), on this exact exe (05:48:33, 21,817,856 B, md5 `980e64c1aa4e5478b5833d83ebea9655`), with `Fallout4.exe` down: the UI chain matches all six expected rows, the lodgen chain matches five of eight, and the three that differ -- `lodgen_terrain_vt` 41/2, `lodgen_ground_cover` 29/6, `lodl_open` 23/2 -- are named with their log lines in `scratchpad/uinotes1_20260912/RESUME_BY_ROADS4.md`; `lodl_open`'s two are a SEGMENTATION FAULT in this exe's headless render path and are handed back to this lane, not fixed.**

### The exe

| | file | time on disk | bytes |
|---|---|---|---|
| the rung ("before") | `release/NifSkope.before_uinotes1.exe` | 2026-09-12 04:10:38 | 21,489,152 |
| the merged exe ("after") | `release/NifSkope.exe` | 2026-09-12 05:48:33 | 21,817,856 |

`sha256` of the after exe:
`b62c49989abf002ebacb32769e7e7057d72043b17942436f493c89eaa9e47c47`.
`mingw32-make -f Makefile.Release` at **rc=0**, 0 errors, one warning that is not
this lane's: `-Wunused-function` on `len3`. Build log
`scratchpad/uinotes1_20260912/build_main7.log`.

The recipe that works is the one the repo's own skill already carries
(`.claude/skills/nifskope-ww-build-verify/SKILL.md:17`): `MSYSTEM=UCRT64` in
front of `/c/msys64/usr/bin/bash -lc`, and git exported onto `PATH` inside it.
Without `MSYSTEM=UCRT64` the MSYS g++ is picked up and cannot execute `cc1plus`;
without git on `PATH` the link dies at Error 127. My first two build commands
deviated from that line and failed for exactly those two reasons; the skill
needed no amendment -- the mistake was mine and is in `MISTAKES_ENTRIES.md`.

**Between the 05:06:05 exe and the 05:48:33 one, the only file that changed is
`src/animworkspacetest.cpp`** -- the gate, not the product. No product source has
been touched since 05:06:05. The gate file is 140,453 bytes, 2,591 lines, CR 0
(pure LF, measured with Python byte counts).

### The gate table

`before` = the rung exe 04:10:38. `after` = the 05:48:33 exe for `animws` and
**the 05:06:05 exe for the other six** -- the chain was not re-run on the newer
exe because the game came up. The only difference between those two exes is the
gate file, which none of the other six runs, but they are still numbers from the
older exe and are written as such.

| harness | before (rung 04:10:38) | after | log |
|---|---|---|---|
| `animws` | 72 checks, 0 fail, 1 skip | **210 checks, 1 fail, 1 skip** (05:48:33 exe) | `logs/after/animws6.log` |
| `hkxanim_ui` | 48 checks, 1 fail | 48 checks, 1 fail (05:06:05 exe) | `logs/after/hkxanim_ui.log` |
| `ui_align` | 11 checks, 0 fail | 15 checks, 0 fail (05:06:05 exe) | `logs/after/ui_align.log` |
| `water_ui` | 84 checks, 0 fail | 84 checks, 0 fail (05:06:05 exe) | `logs/after/water_ui.log` |
| `files_tab` | 29 checks, 1 fail | 29 checks, 1 fail (05:06:05 exe) | `logs/after/files_tab.log` |
| `top_bar` | 43 checks, 5 fail | 43 checks, 5 fail (05:06:05 exe) | `logs/after/top_bar.log` |
| `skeleton_overlay` | 5 checks, 1 fail | 5 checks, 1 fail (05:06:05 exe) | `logs/after/skeleton_overlay.log` |

Every red in that table except `animws`'s is **the same count on both exes**, so
none of them is new. `skeleton_overlay`'s one failure is "the four renders are
not the same size" on both -- on the rung `[941x1524, 941x1524, 941x1024,
941x1024]`, on the merged exe `[941x1524, 941x1524, 965x1024, 965x1024]`. The
mismatch that fails it is the 1524-vs-1024 width, identical on both; the 941 ->
965 is item 1's retired status bar showing up as 24 px more render.

The "before" `animws` is 72 checks because the rung exe carries the OLD gate --
the harness is compiled into the exe, so a rung can only run the checks it was
built with. The 210 are the new (k1)-(q) plus everything that was there before.

`animws` went from **210 checks / 15 failures at 05:07** to **210 / 1 / 1 at
05:48**, over five refusing patch scripts (`gatefix2.py` .. `gatefix6.py`, each
refusing unless every anchor matched exactly once and the file stayed pure LF).
**Thirteen of those fourteen repairs were the gate's own defects, not the
product's.** The fourteenth is the product's and is still red; see below.

One cause sits behind most of the thirteen: `AnimWorkspace::refreshLater()` is a
**50 ms single-shot debounce** (`src/animworkspace.cpp:261-264, 1192-1241`), and
everything that goes through the hub -- delete, cut, rename, and merely SELECTING
a row, which activates it -- ends in `clipsChanged()` -> `refreshLater()`. The
gate was pumping the event loop twice and reading the list before the rebuild
had happened. `listPaste()` and `listDuplicate()` call `refresh()` directly,
which is exactly why only those two passed at 05:07. The repair is a 120 ms
`settle()` on a nested `QEventLoop`, used after every action.

Two more gate defects worth writing down because they would bite the next reader:

* `selectRowNamed` originally re-selected the row after settling, which left a
  rebuild pending that landed inside F2's inline editor and destroyed it. It now
  selects once, settles once, and reads back what the rebuild left.
* The (q) floor tried to squeeze the left column to 40 px. The column **cannot**
  go under its own layout's minimum: handed 40 px by the splitter it settled at
  **219 px** (`animws6.log:192`). A floor that cannot fire proves nothing, so the
  floor now squeezes one button (`AnimWsLoadAnim`) to 8 px, sees
  `1 clipped: LoadAnim(8<58)`, and puts it straight back.

### The one red, and why it is not papered over

```
FAIL (o) Ctrl+A selects every row: 1 of 4 (one pump later 1, once the list rebuilds 1)
     (o) Ctrl+A with the list's signals blocked: 4 of 4 selected, current row 1, selection mode 3
```
(`logs/after/animws6.log:201-202`)

The control on the line below the failure is the finding: with the list's own
signals blocked, the same `selectAll()` takes **4 of 4**. So the rows ARE
selected, and something the selection drives takes them away again,
synchronously. The chain, read out of the source:

`itemSelectionChanged` -> `AnimWorkspace::listRowChosen` ->
`selectEntry( ..., drive = true )` -> `WwHkxAnimHub::activate` ->
`GLView::setSceneSequence` -> `GLView::sequenceChanged`
(`src/nifskope_ui.cpp:24752`) -> `AnimWorkspace::setSequenceByName` ->
`list->setCurrentItem( it )`, which is Qt's `ClearAndSelect` and drops every
other selected row.

**This is a behaviour failure, so by the brief's rule it is measured, named and
stopped on -- not fixed.** What it costs today: multi-row Delete / Copy / Cut
cannot be reached with Ctrl+A. Candidate fixes, none applied, bungo's call which:
a re-entry guard around `setSequenceByName` while the selection is changing;
`setCurrentItem( it, QItemSelectionModel::Current )` so it does not clear;
or not driving the viewport from a selection change while more than one row is
selected.

The skip is the fixture's, not a defect: `10mmPistol.nif` has no
`NiControllerSequence`, so check (i) has nothing to test with.

### The drop, in two halves

Measured, in the same run:

* Step 2 of the drop prints **"the gate's own filter saw 0 Drop event(s), and
  the event was ignored"** (`animws6.log:220`). The gate installed an event
  filter of its own on the very same viewport, so this is measurement and not
  inference: a `QDropEvent` handed to `QApplication::sendEvent` is **not
  delivered** inside the application, and `AnimWorkspace::eventFilter`
  (`src/animworkspace.cpp:1577-1584`) therefore never sees the drop it queues
  the commit from.
* The dock's own half IS proved. Move a row the way the view's internal move
  leaves it, then call the slot that filter queues (`commitListOrder`) by name,
  and the clips, the playback order and `Scene::animGroups` all follow --
  `animws6.log:224-225`, both green. In the 05:46 run, where the dock's note was
  read before the rebuild overwrote it, that line read **"The animations are in a
  new order."** (`animws5.log:225`); in the 05:48 run the check settles first, so
  the note has gone back to `93 frames @ 60 fps` and the ORDER itself is what
  proves it.

**Owed to bungo: one drag of a row with the mouse**, to show Qt delivers the Drop
in a hand. A gate does not get to drive the last inch of a drag, and the log says
so in those words rather than claiming a pass.

### bungo's freeze, measured -- not reproduced

He reported at 05:06 that the merged exe freezes when he opens any nif. On every
route I can drive, it does not:

* Opening the same files one after another into the same window through the
  exe's own `--port` IPC (`load_series.ps1`, which is `NifSkope::openFile`, the
  File > Open path): **merged 2.02-2.68 s per open, rung 2.93-3.86 s per open**
  on the same files in the same order. The merged exe is the same or faster on
  every file.
* There IS a per-open slowdown as files pile up in one window -- roughly
  **+0.6 s per open** -- and it is **identical on both exes**, so it is not this
  lane's.
* The one genuinely slow thing I saw was mine: the `animws` run took about three
  minutes because check (p) opened a modal `QInputDialog` and sat in it. Driven
  by a queued `QTimer::singleShot( 0, qApp, ... )` it now takes **7 seconds**.
* A separate, older defect found while measuring and NOT introduced here: the
  `--port` IPC command is **space-separated** (`src/main.cpp`), so a path with a
  space in it never arrives at all. Equal on both exes.

I cannot say the freeze does not exist -- he saw something. I can say I could not
make it happen, and that everything I could measure says the merged exe opens
files at least as fast as the rung does. What would settle it: which file he
opened, and whether his window already had files in it.

### The pictures, and what I see in each

All under `scratchpad/uinotes1_20260912/images/`. I opened every one of these
myself; a clipped or empty panel is a finding, not a picture.

| picture | size | what it shows |
|---|---|---|
| `before/animws/dock_frame46.png` | 1280x320 | the dock as it was: the fifteen-button bar across the bottom, no header menu |
| `after2/animws/dock_frame46.png` | 1280x320 | the same dock after: the bottom bar gone, the Clip / Key / Channel / Marker / View header menu bar, Start and End fields in the transport row, the playhead a blue box, the side-panel arrow at the right edge |
| `after2/animws/sheet_colours.png` | 976x246 | rulings 2 / 7b / 8 close up: orange selected keys, the out-of-range frames darkened, both range grips on the ruler, the playhead box reading 75 |
| `after2/animws/transport_1x.png` | 1280x29 | the transport row at 1:1 |
| `after2/animws/transport_2x.png` | 2560x58 | the same row at 2:1 -- every glyph on one 16x16 grid at one weight, the disabled ones properly greyed |
| `after/animws/animws_list_menu.png` | 183x236 | ruling 5's right-click menu on the Animations list, each item with its key beside it |
| `before/animws/viewport_gizmo.png` | 857x359 | ruling 1, before |
| `after2/animws/viewport_gizmo.png` | 857x383 | ruling 1, after -- **24 px taller in the same window**, exactly the row the retired status bar was taking. Same instrument, same window, so those 24 px are the evidence for item 1 |

**Findings from looking, which is the point of looking:**

1. `before/seam.png` and `after/seam.png` are **byte-identical** (5,478 bytes
   each), and so are the two `topbar.png` (12,955 each). That is correct and not
   a bug: `ui_align`'s shot is of the TOP seam, and item 1 retired the BOTTOM
   bar, so that picture cannot show item 1. Recorded here so nobody cites it as
   one.
2. The annotation labels overlap each other ("FootLeftIncLeft" reading as one
   word) in the dock shot -- **and they do in the BEFORE shot too**, so it is
   pre-existing and not this lane's. Worth a future lane; not owed by this one.
3. The Animations row label is clipped at the 219 px column minimum -- the same
   measurement as the (q) floor above.

**Picture coverage is NOT complete.** The brief asked for one before/after pair
per ruling 1-9. What exists is a pair for ruling 1 (`viewport_gizmo`), a pair for
the dock as a whole (`dock_frame46`), and after-only shots for rulings 2/7b/8
(`sheet_colours`), 4 (`transport_1x`, `transport_2x`) and 5
(`animws_list_menu`). Rulings 3, 6, 6a, 7, 7a and 9 have **no picture at all** --
the rung cannot take a "before" of a panel that does not exist in it, and the
game came up before I could take the remaining "after" shots. That is owed.

### Owed out of this section

1. **The Ctrl+A selection collapse** -- a real behaviour defect, measured and
   named above, deliberately not fixed. Bungo picks the fix.
2. **The drop's last inch** -- one mouse drag of a row, by hand.
3. **The lodgen chain has not been run at all.** `lodgen_chain.sh` is written and
   ready. The numbers it must match row for row (ROADS3's): lodgen_roads 11/0,
   lodgen_terrain 26/0, lodgen_terrain_vt 41/1 (V9c the known red),
   lodgen_ground_cover 29/5, lodgen_terrain_pbrm 14/0, lodgen_native all-green,
   lodl_open 23/0, lod_generation 116/0.
4. **The other six UI harnesses have not been re-run on the 05:48:33 exe.**
   Their numbers above are from 05:06:05 and are labelled as such everywhere.
5. **The pictures for rulings 3, 6, 6a, 7, 7a and 9.**
6. **The freeze**, until bungo says which file and what state his window was in.
7. The space-in-path `--port` IPC defect, older than this lane and equal on both
   exes.

Nothing is committed. Nothing was stashed. bungo's game folder was not touched.
The rung exe is untouched.
