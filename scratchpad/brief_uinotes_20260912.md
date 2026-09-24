# bungo's UI notes, 2026-09-12 01:3x (collected BEFORE any animation-workspace work; every item is a ruling)

Order of business: these land before any further animation workspace / timeline lane.

## 1. Remove the bottom status bar (01:38)
His words: "pic rel needs to be removed, that entire bottom bar that shows you the loaded nif, waste of space."
- The bar is the QStatusBar `statusbar` in src/ui/nifskope.ui:335; the loaded-nif path is what it shows at rest.
- It also carries transient messages: src/nifskope.cpp 4318, 4585 (Added N files), 4637 (Saved), 5671, 9759/9764
  (background file loading), src/nifskope_ui.cpp 26656, 28041-28085, 31187. Those messages need a home
  (a lane decision: drop them, or a toast/overlay in the viewport) -- NOT a new bar.
- Two harnesses read statusBar()->currentMessage() (src/nifskope_ui.cpp 14642, 14789): repair their expected
  source, not the checks.
- Gate: window height reclaimed by the bar's height, measured by the ui_align.sh geometry dump, before/after picture.

## Timeline editor guesses put to him at 01:37 (unconfirmed, do not charter until he says which)
1 frame/time/fps disagree (frame 6 @ 60 fps shows 0.112 s); 2 annotation labels collide (FootRight/SyncRight);
3 ruler runs past the clip end, no end marker; 4 COM row highlighted while the Keys panel edits an annotation,
Rename greyed; 5 "93 keys" is per-track; 6 "Load..." / "root" clipped, ruler labels half a frame right of keys.

## 2. Selected keyframes vanish; they must turn orange like Blender (01:40)
His words: "When clicking on the keyframes, they go invisible, instead of appearing like in Blender, so as orange."
- Seen on the COM row: clicking keys around frames 9-12 leaves a blank gap in the diamond row.
- Blender's rule: unselected key = white/grey diamond, selected = orange (#FF9E00-ish on the timeline), with the
  selection outline; the dope sheet keeps the diamond visible in both states.
- Suspect: the selected-state brush in the timeline key painter (src/ui/widgets/timeline*.cpp) draws with the
  row highlight colour or a fully transparent brush. Gate: a picture of one clicked key, and a harness check that
  a selected key's pixels differ from the row background.

## 3. Track context menu: "Remove transform axes..." under Remove track (01:44)
His words: "Add an option here, under remove track, to remove all transforms in specific directions, so x, y, z or a
combination of them. Same goes for rotation direction. That way, I can make it so that the slide stays in the center,
but the COM still moves downward when the player crouches."
- The menu today (on a track row, e.g. COM): Insert key at frame N / Select all keys on this track / Rename track... /
  Remove track / Isolate in the viewport. The new entry goes directly under Remove track.
- Behaviour: a small dialog with six checkboxes -- translation X, Y, Z and rotation X, Y, Z (the rotation axes are the
  Euler components about the bone's local axes; say which convention and show it in the dialog) -- applied to every key
  of that track: the chosen translation components are set to the track's frame-0 value (or 0 -- ask him which; default
  frame-0 so a bone does not jump), the chosen rotation components likewise, all other components untouched. Undoable.
- His use case, the gate's fixture: Running_To_Slide_And_Back_To_Running on human_male_vanilla.nif, COM track: strip
  the horizontal travel (X and Y) and keep Z, so the slide plays in place but the COM still dips on the crouch. Gate:
  after the operation the COM's X/Y are constant over 93 keys and Z is byte-identical to before; the HKX round-trips
  through the writer and the reader reads the same keys back.

## 4. The transport-bar icons are bad and unclear (01:45)
His words: "Most of these icons are very bad looking, and unclear to what they do. They need to be better."
- The row: |<  <>  <  |>  []  <>|  >|  (loop)  x1.50  60 fps  [frame]  -- the first frame / previous key / previous
  frame / play / stop / next key / next frame / loop glyphs. Today they are thin one-colour glyphs at mixed sizes
  (the key-jump glyphs look like the play glyph; play is a filled highlight button while the others are flat).
- Rule: follow Blender's timeline transport (feedback_blender_reference): jump-to-start, jump-to-keyframe (the
  diamond with a bar), step-frame, play/pause (reverse play too), all one weight and one size, drawn as our own SVG in
  the wwskin palette, with tooltips that say the action AND its shortcut. Loop and speed become recognisable controls
  (loop icon = the two-arrow cycle; speed a labelled spin, "1.50x"). Gate: a picture of the bar at 1:1 and 2:1, and every
  button's tooltip text asserted by the harness.
- Note "Load..." and "root" in the Animations header row: same lane (item 6 of the guesses).

## 5. The Animations list: shortcuts, drag-and-drop reorder, context menu (01:47)
His words: "For these animations, I should be able to use a shortcut to delete, copy, paste, etc. them, same goes with
reordering with a drag and drop, same goes with right clicking and selecting each such option."
- The list is the "Animations" panel at the left of the Animation dock (rows like
  "Running_To_Slide_And_Back_To_Running -- 93 frames @ 60 fps"; header buttons Load..., x, root).
- Required, all three ways for each action: keyboard shortcut, right-click menu entry, and where it applies the
  mouse: Delete (Del / X as Blender), Copy (Ctrl+C), Paste (Ctrl+V, pastes a copy after the selected row), Cut
  (Ctrl+X), Duplicate (Shift+D), Rename (F2 / double-click), Select all (Ctrl+A); reorder by drag-and-drop with a drop
  indicator line, and Move up / Move down in the menu with shortcuts. Multi-select with Shift/Ctrl.
- The order in the list is the order written to the file where the file has one (say in the report where it does).
- Gate: harness drives every shortcut and every menu entry on a fixture with three clips and asserts the row order,
  count and names after each; a drag-and-drop reorder measured by the model's row order, plus a picture of the menu.

## 6. The Keys / properties panel and the button row are crowded (01:49)
His words: "Also, for this... it's getting pretty crowded in here, isn't it?" (over the Keys panel: Pose with the
gizmo, Auto-key gizmo transforms, Reduce tolerance, Reduce rotation, Annotation Name, Range, the status line, and the
button row Insert key / Delete / Reduce / Add annotation / Rename / Trim / Retime / Bake root / Unbake / Remove track /
Rename track / Add float track / Set float key / Save / Save as).
- Yes. The shape to follow is Blender's dope sheet: a header menu bar (Key, Channel, Marker, View) holds the actions;
  the sidebar (N panel) holds only what is being edited, in collapsible sections that show ONLY when their subject is
  selected: Clip (trim, retime, root motion bake/unbake, save), Key (insert, delete, reduce tolerance/rotation),
  Annotation (name, range, add/rename), Track (rename, remove, float track, set float key). Gizmo options (Pose with
  the gizmo, Auto-key) become two toggle buttons in the transport row, like Blender's auto-key record button.
- The bottom button row goes away; every action stays reachable by menu, context menu and shortcut (item 5's rule for
  clips applies to keys and tracks too: Delete, Ctrl+C/V, Shift+D on keys, G to move, box select).
- Save / Save as move to the dock's header menu (Clip > Save) and File; not a button at the bottom.
- Gate: the dock's fixed chrome (transport + header + status) height measured before/after; the sidebar shows one
  section for each selection kind on the fixture and none for the others; every former button's action found in a
  menu by the harness by text.

### 6a. Amendment (01:51): the properties live in a RIGHT-side panel of the Animation dock
His words: "Why not add another panel in the animation manager, that opens from the right side, that contains more
stuff." -> Ruling: the selection-driven sections of item 6 go into a panel that slides open from the RIGHT edge of
the Animation dock (toggle button in the dock header + the N key while the dock has focus, exactly Blender's
sidebar), collapsible, remembering its width. The left column keeps only the Animations list and the track tree;
the Keys block under the list goes away. The right panel can hold more than the old Keys block did (clip info,
root-motion settings, annotation list, float tracks) -- each as its own collapsible section.

### 6b. (01:53) He sent the Material Manager dock (right-side dock: Filter rows / Replace..., Browse... / Texture
Preview... / Refresh / More..., "Materials in file - 9 material(s), 29 texture(s)" table, a Texture Preview pane with
"Select a texture row for a preview", footer "Live, undoable edits - Double-click a material to unfold - Right-click
for tools") with NO words. Read as: the reference for how the right-side animation panel should sit (a dock on the
right edge). Seen in it, for the record: the footer help line breaks the no-descriptions rule; the empty preview pane
takes half the height when nothing is selected. Asked him which he meant.

## 7. Right-click anywhere on the sheet -> "Add annotation at frame N" (01:56)
His words: "Now, why can't I right click and insert an annotation anywhere?"
- Why, measured: `AnimWorkspace::sheetRowContextMenu` (src/animworkspace.cpp ~1861) builds a menu only for Bone /
  Unbound, Float and NifTrack rows; the Annotations row and the ruler get nothing. Annotations are added only by the
  `Add annotation` button (`addAnnotationAtPlayhead`, ~1700) at the PLAYHEAD with the Name field's text. Every menu
  entry uses `currentFrame()`, never the clicked x. (The older TimelineWidget has "Add text key here..." on its ruler
  at the clicked time -- `TimelineLanesView::contextMenuEvent`, src/ui/widgets/timelineviews.cpp:918 -- but the
  workspace sheet does not use it.)
- Ruling: right-click on ANY row or the ruler offers "Add annotation at frame N" where N is the frame under the
  cursor (snapped), opening an inline name editor on the new marker; on an existing annotation marker: Rename,
  Move (drag), Delete. "Insert key at frame N" likewise takes the clicked frame, not the playhead. Shortcut: M on the
  sheet adds an annotation at the playhead (Blender's marker key), Ctrl+M renames.
- Gate: harness right-clicks at three x positions and asserts the frame in the menu text and the annotation's frame
  after the action; the existing 4 annotations of the fixture untouched.

### 7a. (01:58) Add AND remove by right-click; dragging an annotation resets the zoom
His words: "we need to add and remove annotations with right click, also, when I zoom into the timeline and try to
drag an already existing annotation, the zoom resets to default zoom for some reason"
- Remove: right-click on an annotation marker -> "Remove annotation '<name>'" (confirms item 7's marker menu).
- BUG: with the timeline zoomed in, starting a drag on an existing annotation marker snaps the view back to the
  default zoom. Suspect the drag path (or the edit it commits) rebuilds the sheet / resets the view range
  (frameAll or setRange on document change). The gate: harness zooms to a known range, drags a marker by 5 frames,
  asserts the visible range is unchanged and the marker moved exactly 5 frames. Fix goes with item 7.

### 7b. (01:59) Annotation selection colours
His words: "Selected annotation should also be orange, and orange redish for secondary annotation selection"
- Active (last clicked) annotation marker + label = orange (Blender's active-marker orange, same token as item 2's
  selected key); the other selected markers in a multi-selection = orange-red (Blender's non-active selected shade,
  a darker/redder orange); unselected = the plain marker colour. Same two-tone rule for keys in item 2: active key
  orange, other selected keys orange-red. Both colours are wwskin palette tokens, named in the report.
- Gate: harness selects two markers, samples the pixel of each and of an unselected one, asserts three distinct
  colours matching the tokens.

## 8. Playhead blue; outside the clip range darkened (02:01, over a Blender 4.5 timeline screenshot)
His words: "Timeline marker where you're at in the played animation should also be blue." / "Areas inside of an
animation should be as they are, outside like in Blender, darkened"
- Playhead: Blender's blue (the frame number in a blue rounded box on the ruler, a thin blue line down the sheet),
  not the orange block we draw now. Orange is for selection only (items 2, 7b).
- Range: the ruler and every row keep their normal colour between the clip's first and last frame; before frame 0
  and after the last frame the whole column is darkened (Blender's out-of-range shade, a translucent dark overlay),
  so the clip end is visible -- this also answers guess 3 (the ruler ran past the clip with no end marker).
- Reference picture: his Blender 4.5.3 shot (frame 17 in blue on the ruler, blue line, Start 1 / End 46, the area
  before frame 1 and after 46 darker). Tokens from the wwskin palette; name them in the report.
- Gate: pixel samples on the ruler at the playhead (blue token), inside the range (row colour) and outside (darkened),
  asserted by the harness on the 93-frame fixture at two zoom levels.

## 9. Draggable start and end frame handles on the ruler (02:03)
His words: "Allow me to drag the starting and ending frame, add markers for them of some kind I can drag to make the
total animation space available longer or shorter"
- Two handles on the ruler at the clip's first and last frame (the edges of item 8's darkened zones), each a
  visible grip that drags with snapping to whole frames; the numbers also editable in the transport row (Start /
  End boxes, as Blender's) and in the right panel's Clip section. Dragging the end past the last key lengthens the
  clip (new frames, no keys); dragging it inward trims (same as Trim, undoable, keys beyond it are dropped only on
  commit, with the count in the undo text); dragging the start moves frame 0 (retime the keys, or trim -- the lane
  states which and why; Blender's Start just changes the range, but our clip has no frames before 0, so start-drag =
  trim from the front). Shortcut: Ctrl+Home/End set start/end to the playhead (Blender). The Trim button and its
  boxes in the old panel go away with item 6.
- Gate: harness drags the end handle +10 and -10 frames and asserts the clip's frame count, the darkened zone edge
  and the HKX duration on save; the same through the Start/End boxes.
