# Lane UINOTES2 — the two transport toggles as icons, Ctrl+A, the owed pictures

Tree: `E:/Projects/NifskopeWWE_ui` (the copy). Nothing committed, nothing stashed,
nothing under `E:/Projects/NifskopeWildWastelandEdition` opened, edited, built or run.
Every time below comes from `date +%H:%M` in the turn that wrote it, or from `stat`.

bungo's words this lane is built on:

* 06:0x — "okay, new icons look good, what is the "pose" button and the round dot
  that's not centered button?"
* 06:1x, offered both toggles drawn as icons in the same set with a lit state when
  on — "Both icons"

## 0. The baseline, measured before anything was touched (06:18)

`animws` on the rung exe (`release/NifSkope.exe` 2026-09-12 05:48:33, 21,817,856 B,
copied aside as `release/NifSkope.before_uinotes2.exe` at 06:17):

```
210 checks, 1 failures, 2 skips
FAIL (o) Ctrl+A selects every row: 1 of 3 (one pump later 1, once the list rebuilds 1)
```

That is the brief's pre-registered baseline (210/1) reproduced on my own run, so the
number I am building against is mine and not a quoted one. Log
`scratchpad/uinotes2_20260912/animws_before.log`; the "before" pictures the same run
saved are in `scratchpad/uinotes2_20260912/images/before/`.

The second skip is the fixture's, not a defect, and was there before: the gate (i)
NIF `10mmPistol.nif` has no `NiControllerSequence`, and no skeleton.hkx was passed.

### "not centered", measured rather than guessed

His second question named the auto-key dot as "not centered". It is not the dot.
`scratchpad/uinotes2_20260912/measure_dot.py` finds every lump of ink in the 2:1
picture of the transport row and prints its bounding box:

| ink | x range (2:1) | y centre | what it is |
|---|---|---|---|
| 0–6 | 28..495 | **27.5** | the seven transport glyphs |
| 9 | 728..743 | **27.5** | the auto-key dot |
| 8 | 622..671 | **36.5** | the word "pose" |

Row centre line is y 28.5. The dot sits on exactly the same line as every other
glyph; the WORD beside it sits nine 2:1 pixels (4.5 logical px) lower, because a
text button puts its text on the font's baseline while an icon button centres its
icon. A word among drawings, half a row lower than they are, is what read as "not
centered" — and drawing both as icons is what removes it.

## 1. The two toggles as icons (code 06:2x)

`src/animworkspace.cpp`. Three things changed in the drawing code and two at the
call site; the patch is the refusing script
`scratchpad/uinotes2_20260912/fix1_icons.py` (exact-once anchors, pure-LF assert,
all-or-nothing).

**Auto-key = Blender's record dot**, which it already was — but at radius 4 on the
16×16 grid, where every other glyph in the set is ten units tall (the triangle's
back, the bar). Radius 5 now, so it carries the same mass as the rest instead of
reading as a speck.

**Pose-with-gizmo is a new glyph and it is OURS, not Blender's** — said plainly
because the house rule is to follow Blender and name every divergence. Blender has
no single icon for this: its armature glyph is a bone, and the gizmo is a ring drawn
in the viewport, never on a button. So the glyph is the two put together: Blender's
octahedral bone (head at the top, two shoulders a third of the way down, a long
tail) inside the two side arcs of the rotate gizmo's ring, the ring open at the top
and the bottom so the bone's tips read as tips instead of merging into it. Same
16×16 grid, same 2 units of stroke, same palette ink as the other ten glyphs.

**The lit state** is a second pixmap in the icon, inked in the palette's `accent`
(`#f0a54a`, whose own name in `skinVars[]` is "selection accent, active toggles"),
with `accentDisabled` for a toggle that is on but cannot be clicked. Qt picks it
because a checked `QToolButton` carries `State_On`. `wwTransportToggleIcon()` is the
one place that states the rule.

**Loop does NOT light, and that sentence replaces one I had already written.**
I wired it to light, wrote here that it did, and then measured the built exe: in
the gate's own 2:1 grab `transport_2x_on.png` the checked loop button's ink is
`#4d5761`, plain ink over the checked blue, while pose and auto-key are `#61584b`
and `#665847`, the accent. The cause is ownership, not the icon: the dock's loop
button takes its icon from the SHARED `aAnimLoop` action, and the render toolbar
re-skins that action on every refresh of its popup
(`src/nifskope_ui.cpp`, `ui->aAnimLoop->setIcon( tlMakeIcon( "loop", ... ) )`)
with a single-state icon, so nothing the dock puts there survives. Making Loop
light means changing the RENDER TOOLBAR's own glyph, which is not what bungo
asked for, so the lit icon came back off (`fix4_loop_claim.py`) and the harness
now PRINTS loop's two inks beside the two it gates — measured, deliberately not
asserted, because "Loop must not light" is the wrong thing to own the day
someone decides it should. The gate's line reads
`(q) Loop, whose icon the render toolbar owns: off=#e6e8eb on=#e6e8eb moved=0 checked=1`.
The mistake is written up in `MISTAKES_ENTRIES.md`. Play and Play backwards do
NOT light either, and that one IS a choice: they already say they are running by
swapping the glyph to the pause bars, and two signals for one state is one more
than the button needs.

Tooltips are unchanged in wording (both already name what the switch does; neither
has a shortcut, and none was invented). They are now asserted by the gate word for
word, which they were not before.

`g++ -fsyntax-only` with the release flags prints nothing for `animworkspace.cpp`
at 06:25, and on the same command line a deliberate `return notAThing;` was rejected
with its line number, so the checker was awake.

## 2. Ctrl+A keeps every row (code 06:2x)

Patch `scratchpad/uinotes2_20260912/fix2_ctrla.py`.

UINOTES1b measured the chain and stopped, correctly, at naming it:

```
itemSelectionChanged -> listRowChosen -> selectEntry(drive=true)
 -> WwHkxAnimHub::activate -> GLView::setSceneSequence
 -> GLView::sequenceChanged -> AnimWorkspace::setSequenceByName
 -> list->setCurrentItem( it )          <- Qt's ClearAndSelect
```

Of the three candidates that lane named I took the guard on the driven re-entry,
which is the smallest: one call, one condition, and the drive itself is untouched,
so a single click still drives the viewport exactly as it did.

`setSequenceByName` now calls
`setCurrentItem( it, it->isSelected() ? NoUpdate : ClearAndSelect )`. If the row the
viewport is echoing is already part of the selection, the current row moves and the
selection is left alone; if it is not, it is selected as before, because then the
viewport is showing something the list is not and the row has to become visible.

**A second place threw the selection away and the first fix alone would not have
held.** `rebuildList()` clears the list and rebuilds it, restoring only the CURRENT
row — so the 50 ms debounced refresh would have eaten the selection a moment after
the key. It now remembers which rows were selected, by kind and name, and puts them
back, using the same NoUpdate rule for the current row so Qt cannot undo what the
function just did. That also fixes a defect nobody had named: **every** refresh
reduced a multi-selection to one row, so Delete / Copy / Cut acted on one clip
however many the user had picked.

**What would refute the fix:** a user with a multi-selection who expects the list to
collapse to the one clip the viewport switched to. That trade is deliberate and is
written beside the code — a selection the user made outranks the viewport's echo of
it. If bungo wants the other behaviour it is the condition on one line.

## 2b. The `lodl_open` crash lane ROADS4 handed back (07:0x)

The director made this item 2b, before the pictures: `lodl_open.sh` went 23/0 on
the 04:10:38 exe and 23/2 with six segmentation faults on the 05:48:33 exe, and
since the only code between those two exes is UINOTES1's UI work, it was handed
to me. The retired status bar was the named suspect.

**It is not UINOTES1's code, and here is how that was measured rather than
argued.**

### Reproduced first, on my own rung, in my own tree

`release/NifSkope.before_uinotes2.exe`, md5 `980e64c1aa4e5478b5833d83ebea9655` —
byte for byte the exe ROADS4 tested.

| exe | `lodl_open.sh` | segfault lines | planes rendered |
|---|---|---|---|
| rung 05:48:33 | **23 / 2** | 10 | **0 of 9** |
| mine 06:55:52 | **23 / 0 PASS** | 0 | **9 of 9, 8 distinct** |

(Both runs with `PY=/c/Windows/py.exe`. The MSYS2 python in this tree has no
`numpy`, and with the default `python` the harness reports the last check as red
with EMPTY numbers — the known symptom in `nifskope-ww-lodgen`. On my exe that
one check is green the moment a python with numpy runs it: coverage 0.1451
against a floor of 0.05, luminance SD 46.77 against a floor of 2, the harness's
own arithmetic on the harness's own 88,019-byte picture.)

### Four measurements that take the UI out of it

1. **A plain NIF renders headlessly on the RUNG.** `WW_RENDER_VIEW=1` +
   `WW_RENDER_SHOT` on `fixtures/human_male_vanilla.nif`: rung rc 0, my exe rc 0,
   and the two pictures are the same 14,609 bytes. The status bar, the viewport
   overlay line, the transport row and the dock are all built and drawn on that
   path. They do not crash it.
2. **A BIG terrain NIF renders headlessly on the RUNG.** The rung's own
   `-no-gui lodl --region -16 -16 15 15 --lod 2` wrote a 2,651,953-byte NIF; both
   exes rendered it, rc 0, both pictures 24,632 bytes. So it is not scene size,
   shape count, or anything that scales with what is on screen.
3. **Only the `.lodl` DOCUMENT crashes**, and only in the viewer: the rung's
   `-no-gui lodl` CLI writes its NIF happily (and 21 of the harness's checks,
   all the non-render ones, pass on the rung). Six viewer renders, six crashes.
4. **The stack is shallow — 19 frames, no repetition**, so it is not runaway
   recursion from a selection loop, which was my first guess and was wrong. Both
   exes are linked `-Wl,-s`, so the frames are addresses; the shape is what is
   evidential here.

```
Thread 1 received signal SIGSEGV, Segmentation fault.
#0  0x00007ffcc35d2a54 in ?? () from release/Qt6core.dll
#1  0x00007ff60cc13a99 in ?? ()                       <- app
#2  0x00007ffcc3987c9b in ?? () from release/Qt6core.dll
#3  0x00007ff60ce6486a in ?? ()                       <- app
#4  0x00007ff60c922289 in ?? ()                       <- app
#5  0x00007ffcc3684a8c in ?? () from release/Qt6core.dll
#6  0x00007ffcc2406824 in ?? () from release/Qt6widgets.dll
#7  0x00007ffcc362cbf8 in ?? () from release/Qt6core.dll
#8  0x00007ffcc363180c in ?? () from release/Qt6core.dll
#9  0x00007ffcc300a2d2 in ?? () from release/Qt6gui.dll
#10 0x00007ffcc382e25a in ?? () from release/Qt6core.dll
#11 0x00007ffcc300a2a9 in ?? () from release/Qt6gui.dll
#12 0x00007ffcc363a063 in ?? () from release/Qt6core.dll
#13 0x00007ffcc3637454 in ?? () from release/Qt6core.dll   <- event loop
#14..#16 app, #17 BaseThreadInitThunk, #18 RtlUserThreadStart
```

It dies after `meshed and built`, inside a queued call that the event loop
delivers — i.e. after the lodl scene exists, on the way to the shot.

### And the thing that must be said about my own green row

**My exe is not "the rung plus my UI changes".** The first build in this copy
recompiled nine translation units, and five of them are lodgen's:

```
animworkspace.o  animworkspacetest.o  main.o  nifskope_ui.o
lodgen.o  lodgenchunkpass.o  lodgenmanager.o  nativeemit.o  nifcli.o
```

They rebuilt because `src/lodgen.h` and `src/nifcli.cpp` in this copy carry lane
ROADS4's in-flight edits, stamped 06:07 — newer than the 05:48:33 exe, and this
was the copy's first build. So the `.lodl` render path in my exe is BUILT FROM
DIFFERENT SOURCE than the rung's, and my 23/0 does not prove that the rung's
lodgen sources would give 23/0. What it does prove, with measurements 1-3, is
that the crash does not live in the UI code this lane and lane UINOTES1 own.

**Owed, to whoever owns the lodl viewer path:** a crash in the `.lodl` document's
viewer render on the 05:48:33 binary, gone in any exe built from this copy's
current lodgen sources, reproducible in one line —

```
WW_RENDER_SHOT=<png> WW_RENDER_VIEW=1 WW_RENDER_SIZE=360x360 \
  release/NifSkope.before_uinotes2.exe --port <free> Commonwealth.lodl
```

3 of 3 crashes on the rung, 3 of 3 clean renders on 06:55:52. Nothing in
UINOTES1's rulings was changed to make that happen, and nothing was changed here
to hide it.

## 3. The pictures (07:1x)

All in `scratchpad/uinotes2_20260912/images/after/`, every one taken by the
harness on the shipped exe, every one opened and looked at.

| picture | ruling | scale | what it shows |
|---|---|---|---|
| r3_axis_dialog.png | 3 | 1:1 | the Remove-transform dialog, Translation X/Y ticked, the axis note, Remove/Cancel |
| r6_header_bar.png | 6 | 1:1 | the dock's own menu bar: Clip, Key, Channel, Marker, View |
| r6_menu_clip.png | 6 | 1:1 | the Clip menu open: Trim to range, Retime, Bake root, Unbake (greyed), Save, Save as |
| r6a_panel_track.png | 6a | 1:1 | the right-side panel on a TRACK: Clip / Root motion track / Retime to / Track / Bone |
| r6a_panel_annotation.png | 6a | 1:1 | the same panel on an ANNOTATION: Annotation / Name FootRight |
| r7_sheet_menu_frame37.png | 7 | 1:1 | right-click on the ruler at frame 37, all four entries naming the frame |
| r7a_annotation_menu.png | 7a | 1:1 | right-click on the FootLeft marker: rename (Ctrl+M), remove, then the sheet's own four |
| r7a_zoom_after_drag.png | 7a | 1:1 | the sheet still at 20..60 after a 5-frame marker drag -- the zoom did NOT reset |
| r9_range_grips.png | 9 | 1:1 | both white grips, the blue playhead at 75, the out-of-range ends darkened |
| dock_overview.png | all | 1:1 | the finished dock, 1280x320, side panel open |
| transport_2x_off.png / transport_2x_on.png | 4 | 2:1 | the transport row, the two new toggles off and lit |
| icons_before_after.png | 4 | 2:1 | BEFORE (rung) / AFTER off / AFTER on, the same widget, the same gate, captioned with the measured ink line |
| icons_zoom_4x.png | 4 | 4:1 | the three mode buttons only, nearest-neighbour |

The 2:1 and 4:1 crops are nearest-neighbour magnifications of the harness's own
grab (ww-texel-picture: no resample may invent a pixel, and the caption carries
the report's number in the report's units).

**The number the two pictures were taken for.** compose_icons.py measures the
ink in each picture with the same arithmetic as the baseline measurement:

```
transport glyph line:  before y 27.5   after-off y 27.5   after-on y 27.5
the two toggles:       before  "pose" x 622..671 y 36.5  |  dot x 728..743 y 27.5
                       after   pose   x 616..647 y 27.5  |  dot x 696..715 y 27.5
```

The word sat 9 pixels (4.5 logical px) below the line every drawing beside it is
centred on. Both toggles are now on that line. That is the whole of "the round
dot that's not centered", answered in numbers.

**Three things I saw in the pictures that nobody asked about.** Reported, not
touched -- they are bungo's calls, not mine:

1. **The annotation labels overlap each other at 1:1.** In dock_overview.png and
   r9_range_grips.png five markers inside 93 frames draw their names on top of
   one another: "FootLeftSyncLeft", "FootRightFootRight". Zoomed to 20..60
   (r7a_zoom_after_drag.png) they are perfectly legible, so it is crowding, not a
   drawing fault. Blender's dope sheet drops a marker's text when the next marker
   is closer than the text is wide.
2. **"Retime to" keeps its label and an empty box when an ANNOTATION is
   selected** (r6a_panel_annotation.png), where the track view shows 30.00 fps. A
   row that cannot apply reads better absent than blank.
3. **The Animations list row is one long label** -- "Running_To_Slide_And_Back_
   To_Running - 93 frames" -- clipped by the column at 1:1.

## 4. The drop, proved again on this build (07:0x)

The same two halves lane UINOTES1b established, re-run on the shipped exe, from
animws_final.log:

```
(o) the Drop event itself cannot be delivered from inside the application
    (the gate's own filter saw 0)
ok  (o) a row moved and the drop's own commit run: the clips themselves are in
    the new order: ... -> Running_To_Slide(3) | SlideTest | Running_To_Slide
ok  (o) ...and the scene's own list with it
ok  (o) the list is back to what it was
```

Qt will not deliver a synthesised QDropEvent through QApplication::sendEvent, so
the gate drives commitListOrder -- the slot the real drop queues -- and then
checks the three things a real drop has to move: the rows, the clips behind them
(93 frames, so it is the whole clip and not a label), and Scene::animGroups.

**For bungo, one line:** drag a clip in the Animations list onto another row and
let go -- the row lands where you dropped it, and the order you see is the order
it plays in; press Ctrl+Z once and it goes back.

## 5. The gates

Exe under test: release/NifSkope.exe, **2026-09-12 06:55:52, 21,850,624 B**,
BUILD2-RC=0, 0 lines matching "error:" or "Error [0-9]", make -q returns 0
(nothing left to build), and cmp res/style.qss release/style.qss is identical.
Fallout4.exe count 0 at every build and every launch, counted separately from
NifSkope.exe; every window on the second monitor with my own --port.

| harness | brief's baseline | rung 05:48:33 (mine) | shipped 06:55:52 | row |
|---|---|---|---|---|
| animws | 210 / 1 | **210 / 1**, 2 skips | **224 / 0**, 2 skips | +14 checks, the red gone |
| lodl_open | (item 2b) 23 / 2 | **23 / 2**, 10 segfault lines | **23 / 0 PASS** | green |
| ui_align | 15 / 0 | -- | **15 / 0 PASS** | same |
| water_ui | 84 / 0 | **82 / 0 PASS** | **82 / 0 PASS** | see below |
| top_bar | 43 / 5 | -- | **43 / 5** | same, and the same five |
| skeleton_overlay | 5 / 1 | -- | **46 / 0** in-app, +5 mask, dots PASS | see below |
| hkxanim_ui | 48 / 1 | -- | **SKIPPED** | fixture absent |
| files_tab | 29 / 1 | -- | **SKIPPED** | fixture absent |

* **water_ui is 82, not 84, on the RUNG TOO** -- I ran the rung to find out
  rather than call it a regression. Both 0 failures. The brief's 84 is the main
  tree's number; this copy has two fewer checks to run.
* **hkxanim_ui and files_tab were SKIPPED.** Both want
  scratchpad/hkx1_20260910/clips/*.hkx, which does not exist in this copy and
  whose only home is the main tree. I did not go and get it: the brief's first
  rule is that I open nothing there. Neither harness reaches the code I changed.
* **skeleton_overlay**: the in-app half is 46 checks / 0 failures, PASS. Its
  picture-analysis steps need numpy, which the MSYS2 python in this tree does not
  have, so the script exits 1 without running them. I ran them myself under the
  Windows python: gate (j) "every pixel the overlay drew is on the character" 5
  checks / 0 failures PASS, and S6 "1 clusters, 0 unnamed" PASS. Two steps stay
  NOT MEASURED and neither can be run from this tree: the (j') floor wants
  scratchpad/skelfix_20260910/before_on_frame46.png and S2 wants rung renders
  under scratchpad/skel2_20260910/rung; both are absent here.
* **top_bar's five** are the pre-registered five, unchanged and quoted:
  "and the panel toggles it absorbed", "Panels lists the Block List dock",
  "... Block Details dock", "... Header dock", "... NIF Browser dock".

### What is red

Nothing this lane owns. top_bar 43/5 is red and was red before this lane,
identically, and it is the top bar's Panels menu, not the animation dock.

### What was not measured

* hkxanim_ui and files_tab -- fixtures absent from the copy (above).
* skeleton_overlay's (j') floor and its S2 rung comparison -- reference pictures
  absent from the copy (above).
* Whether a build from the RUNG's lodgen sources would pass lodl_open: my build
  recompiled five lodgen translation units against lane ROADS4's in-flight
  src/lodgen.h, and I may not touch those files to find out (section 2b).
* Whether the pose glyph reads as "a bone in a gizmo" to someone who has not been
  told. That is an eye question and it is bungo's.

## 6. Documents, and the skills

All under `scratchpad/uinotes2_20260912/`:

| file | what it is |
|---|---|
| `WW_CHANGES_ENTRY.md` | the entry for `WW_CHANGES.md`, headed `## 2026-09-12 - The two gizmo switches become icons, and Ctrl+A keeps the whole selection` (em dash in the file) |
| `HANDOFF_BLOCK.md` | the HANDOFF top block: the exe, the gate table, the four things the next lane needs, the lodl crash written up as still owed |
| `MISTAKES_ENTRIES.md` | four entries -- the claim that outran its proof, the second selection eater the gate could not see, the half-written exe, and the story I told about the lodl crash before I measured it |
| `CHANGED_FILES.txt` | A/M with byte counts and CR/LF counts per file, measured in Python; plus the not-touched list with their mtimes, and every lane artefact |

### Skills

**Amended: `nifskope-ww-build-verify`.** Two bullets, both paid for this lane.
*A backgrounded build is finished when its exit code says so, and at no other
moment* -- an exe whose timestamp, size and `make -q` answer all look finished
can still begin `00 00` while the linker runs, and two harness runs were spent on
one. *Read the build log for what ELSE was rebuilt before crediting a change* --
a header another lane edited in the same tree pulled five lodgen translation
units into a two-file UI build, so the new exe was not "the old exe plus my
diff", with the one-line `grep` that names them.

**Added: `ww-toggle-lit-gate`.** The whole procedure for a mode toggle that
lights up: the `QIcon` mode/state pairs and which buttons should NOT use them;
**who owns the icon** -- the `grep` that finds out whether something else re-skins
a shared `QAction`, asked BEFORE promising a lit state; the four checks and the
floor that gate it by the icon's mean ink; then the 2:1 picture, because the icon
can be right while the button is wrong -- which is exactly what happened here;
and the ink-centre measurement that answers "this control looks not centered" in
numbers, since it is usually not the control that was pointed at.

**Used as written, no amendment needed:** `nifskope-ww-resume-pending` (the rung
kept aside before the first build, the baseline re-measured rather than quoted),
`ww-spec-gate-audit` (210 / 1 reproduced on my own run before building to a
number), `ww-texel-picture` (nearest-neighbour 2:1 and 4:1, the crop chosen where
the defect is, the caption carrying the report's number in the report's units),
`ww-test-harness-add` (every new check shown able to fail on the same run).


## 7. Ruling 07:3x -- the lit state is white, not the accent

bungo, over `icons_before_after.png`, verbatim: **"Why do these turn yellow when
selected? the buttons, keep them white"**.

**The token, named.** ON is **`textBright`, `#f2f3f5`** -- the brightest ink in
`skinVars[]` (`src/nifskope_ui.cpp:321`), one line below `text` `#e6e8eb`. OFF
stays `text`, as before. The two disabled pixmaps moved with them: Disabled/Off
is still `textMuted` `#aeb3ba`, and **Disabled/On is now `text`** -- brighter
than the muted ink a disabled OFF toggle draws, so a toggle that is on but cannot
be clicked does not read as off, and dimmer than the enabled white, so it still
reads as disabled. `accent` and `accentDisabled` are gone from
`wwTransportToggleIcon()` entirely.

**The step is small on purpose, and that is written beside the code.**
`dist(text, textBright)` is 12 + 11 + 10 = **33 of a possible 765**. The state
signal the eye actually reads is the QSS `:checked` plate under the glyph
(`bgBtnDown` `#355f86`, from `wwBoxedButtonQss()`); the ink only has to stop
contradicting it. That sentence is in the doc comment so the next person does not
"fix" the small gap.

**What the gate had to change, and why the threshold moved twice.** Gate (q)
compared the ON mean to `accent` with `dist <= 40` and required `dist(off, on) >=
60`. Both numbers were sized for a colour that is no longer there. Now:

* `dist(off, on) >= 20` -- it still has to move, floored at a fraction of the
  33 the two tokens are apart, not at a constant inherited from the accent;
* **both** means are pinned to their own token, `text` for off and `textBright`
  for on;
* the Stop floor is untouched: Stop has no On pixmap, and the same arithmetic
  must still report it moved 0. It does.

The pinning tolerance had to be measured rather than assumed. With the tolerance
written as a summed distance of 2 the gate went **red on the first run**: the
pose glyph is a ring and is nearly all antialiased edge, and the mean is taken
over pixels whose alpha is merely over half, where un-premultiplying costs up to
1 per channel. Its ON mean read `#f1f2f4` against `#f2f3f5` -- one short on each
channel, summed distance 3 -- while the auto-key dot, which is solid, read the
token exactly. The fix is not a looser number but the right measurement: the
tolerance is **2 on every channel** (`max(|dr|,|dg|,|db|)`), which is what "the
mean ink is the token" means, and a summed test would have had to be loosened to
6 to say the same thing.

**Measured on the shipped exe** (`animws_done2d.log`, gate (q)):

```
Pose     off=#e6e8eb on=#f1f2f4 moved=30 offWhiteMaxCh=1 offPlainMaxCh=0
AutoKey  off=#e5e8eb on=#f2f3f5 moved=34 offWhiteMaxCh=0 offPlainMaxCh=1
                                        (textBright #f2f3f5, text #e6e8eb)
```

**And the picture, because the icon can be right while the button is wrong.**
That is the mistake this lane already made once at 06:35, so the ink was read
back off `transport_2x_on.png` itself. Reading every pixel that differs from the
`:checked` plate gives a blend (`#5a6571`, `#5e6771`) -- a 16 px glyph is mostly
edge, which is exactly why the accent build's same measurement read `#61584b` and
not `#f0a54a`. Taking the glyph's CORE instead (the pixels furthest from the
plate) gives, for both toggles:

```
transport_2x_on.png    pose #f2f3f5   auto-key #f2f3f5     (textBright)
transport_2x_off.png   pose #e6e8eb   auto-key #e6e8eb     (text)
```

The button really does show the white. `measure_lit_core.py` beside the pictures
does that measurement on its own; `compose_icons.py` now captions the composite
from the same number instead of from a constant, so the picture and this report
cannot disagree.

**The skill was amended where it said accent** (`ww-toggle-lit-gate`): the
frontmatter, the reference implementation, the bullet that justified
`accent`/`accentDisabled`, and checks 3 and 4. Two new bullets say what this
ruling cost: *ask which ink, do not assume the accent* (it was overruled the same
day it shipped), and *a small ink step is fine when the PLATE carries the state,
so size the thresholds to the tokens you chose and never leave a floor sized for
a colour you no longer use*. Check 4 now spells out the per-channel tolerance and
why summing will not do.


## 8. Ruling 08:2x -- keys outside the range are greyed

bungo, verbatim: **"also, grey out diamond keyframes out of animations start and
end range, to indicate they're not being taking into consideration anymore"**.

**The token, named, and the arithmetic.** The dim is
**`animOutOfRange` (`#17191c` dark, `#cfcfcf` light) composited over the key's
own colour at 155/255** -- Blender's own alpha from `ANIM_draw_framerange`, the
same one the darkened band already lays over the background. So a greyed diamond
is exactly the colour it would have been if the out-of-range wash had been
painted OVER it instead of under it. One token, one constant, and it is right in
the light theme for free.

It lives in one public static, `AnimDopeSheet::dimOutOfRange( const QColor & )`
(`src/animdopesheet.h`), so the painter and the harness cannot drift apart: the
gate compares the painted pixels to the painter's own function rather than
re-deriving 155/255 on its own.

**Every state is dimmed the same way.** The diamond lambda now takes the key's
frame and runs the colour it was going to use -- plain, "other selected", or
active -- through `dimOutOfRange()` when the frame is outside
`rangeStart()..rangeEnd()`. A selected key that has fallen out of the range keeps
its selection colour, dimmed, so it still reads as selected while it says
"ignored". Nothing else changes: same size, same shape, still hit-tested, still
draggable. **It follows the grips for free**, because dragging one already calls
`setRange()`, which calls `update()` -- the gate proves that rather than assuming
it.

**The comment that said the opposite is gone.** `paintEvent` carried a paragraph
justifying the band being painted band-by-band partly because "a key outside the
range then stays as bright as one inside, which is what Blender does". That is
still true of Blender and it is now a stated divergence, in the same place, with
bungo's sentence quoted.

**The gate**, `(k8b)`, on the 93-frame fixture (every frame is a key):

| with the range at | frame 5 | frame 30 | frame 80 |
|---|---|---|---|
| 10..50 | `#595a5c` greyed | `#bfbfbf` plain | `#595a5c` greyed |
| 10..90 (End moved, no reload) | `#595a5c` still greyed | -- | `#bfbfbf` plain again |
| 0..92 (**the floor**) | `#bfbfbf` | `#bfbfbf` | `#bfbfbf` |

Every sample is compared to the EXACT colour, not to "darker than", so a wash of
the wrong strength fails too; the floor row is what shows the comparison able to
go the other way on the same run. Selected-and-out-of-range is measured in the
same block: the active key reads `#725a25` (`animKeySel` `#ffbe33` dimmed) and the
other selected one `#724611` (`animKeySelOther` `#ff8c00` dimmed), both exactly
what `dimOutOfRange()` says, and the three greyed states are still three
different colours.

The two zoom passes of gate (k) carry the same expectation now, which is why
pass 2, whose range is 25..40, expects its ACTIVE key at frame 20 to be greyed.
That is not a workaround: it is the first place the new rule bit, and the
expectation is computed from the pass's own range.

**Picture:** `images/after/sheet_range_dim.png`, the sheet at 1:1, 1054x96, range
10..50, with the greyed diamonds either side and two selected-but-out-of-range
keys among them.

**One thing the new gate broke, and how it was caught.** Adding (k8b) turned gate
(n) into a SKIP -- "the COM row is scrolled out of the sheet" -- because (k8b)
grabs the sheet and pumps the event loop several times over. The run still said
`0 failures`; only the skip count moved, from 2 to 3. A gate forces the state it
measures, so (n) now scrolls the COM row in the way (k) already does for the thigh
row and says so in the log (`the COM row was scrolled out of the sheet; scrolled
in to row index 2, centre y now 53`), and its menu check is measured again.


## 9. The DONE2 delivery

**The exe:** `E:/Projects/NifskopeWWE_ui/release/NifSkope.exe`, **2026-09-12
08:47:03, 21,867,008 B**, md5 `1b41b4f9407732f00fd9d02399dac5e3`, `make` exit 0,
`MZ` read back before it was run. `qmake` was not re-run, as instructed. Two
`make` rounds went into it: the first landed both rulings (9 translation units --
the dope-sheet header pulled `nifskope_ui.cpp` in with it), the second rebuilt
only `animworkspacetest.cpp` for the per-channel tolerance.

**animws: 236 checks, 0 failures, 2 skips** -- against 224 / 0 / 2 before these
two rulings. The 12 new checks are (k8b)'s eleven (ten plus its picture) and
gate (q)'s new "the unlit ink is still the plain text ink". Both skips are the
two fixture skips this lane has had from the start: no `skeleton.hkx` in this
copy, and the vanilla 10mmPistol NIF has no `NiControllerSequence`.

**Changed, all pure LF, CR 0 every one:**

| file | bytes | LF |
|---|---|---|
| `src/animworkspace.cpp` | 122,142 | 3,166 |
| `src/animworkspacetest.cpp` | 162,827 | 2,940 |
| `src/animdopesheet.cpp` | 40,890 | 1,271 |
| `src/animdopesheet.h` | 12,302 | 269 |
| `tests/spells/animws.sh` | 13,928 | 221 |
| `.claude/skills/ww-toggle-lit-gate/SKILL.md` | 6,646 | 125 |

`src/animdopesheet.cpp` and `src/animdopesheet.h` are the two outside the first
delivery's five, listed in `CHANGED_FILES.txt` with their counts and the reason.

**Pictures re-taken:** `transport_2x_off.png`, `transport_2x_on.png`,
`icons_before_after.png` (2736x298) and `icons_zoom_4x.png`, plus the new
`sheet_range_dim.png`.

**One thing worth knowing before the next lane builds.** From 08:47 this
session's bash shell could not EXECUTE `release/NifSkope.exe` -- `execve`
returned EACCES, and so did `cmd /c` from the same shell -- while the identical
bytes under another name in the same folder ran, an older exe ran, and PowerShell
launched `release/NifSkope.exe` itself. It is the path in this shell, not the
binary: the harness was run against a byte-identical copy (`EXE=`), md5 verified
equal both ways, and the copy was deleted afterwards. Nobody should rebuild
chasing a corrupt exe over it.
