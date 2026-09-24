### 2026-09-11 -- lane UI6: the segmented strips rejoined, the Animation Manager retired, the dropdown arrows given their own column

**bungo, over a zoomed screenshot of the Header | Blocks | Files strip lane UI4
had just gapped:** *"Why are they separated?"* -- a segmented control is ONE
element, so the four pixels of air belong OUTSIDE its box and nowhere inside it.
Both strips -- Header | Blocks | Files on the left and LOD | Water in the LOD
Generation panel on the right -- are one joined box again: segments touching,
one shared seam, square inner corners, the two outer corners rounded, and the
same 4 px from the row's top and bottom, from the window's left edge and from
the toolbar beside it. Measured on the painted pixels, not on `tabRect()`, which
includes the margin and reads the same either way:

```
                              before 05:58:21      after        want
  strip top -> row top               4                4          4
  strip bottom -> row bottom         4                4          4
  strip left -> window edge          4                4          4
  strip right -> toolbar             4                4          4
  BETWEEN each pair of segments      4                0          0
  ---------------------------------------------------------------
  the ROW                           35               35         35   unchanged
  each SEGMENT                      27               27              unchanged
  the LOD panel's two segments       4 apart          0 apart    0   (never measured before)
```

`UI/SegmentedStripAir = 0` still emits the 18:25:20 sheet byte for byte, so the
way back is exact at its off value.

**bungo, over the UI3 comparison of the viewport header:** *"That's fine, as
long as the dropdown arrows do not intersect with the text / icons like on the
screenshots you showed me."* Every button in the top row that HAS a menu now
states one horizontal rule, `padding-right: 8px`, and the arrow is at least 2 px
clear of the glyph on all thirteen of them (worst 2, on Select; it was 0 and
-2 on the narrow icon buttons before). The row's height, the buttons' height and
every vertical number are untouched. The menu buttons are 4 px wider, and in a
narrow window the left dock gives up those pixels (measured: 174 -> 164 px in the
harness's window).

**bungo, over three screenshots of the old Animation Manager:** *"Do you see
it?"* (two spin boxes with their steppers clipped to a sliver), *"we have a new
standard for those sliders, don't you remember?"*, *"look at all this text
clutter"*, and earlier *"Animation manager was one of the first features for
nifskope, and it's pretty old and outdated btw"*.

* **The Animation Manager dock is gone.** No `TimelineDock`, no `TimelineWidget`,
  no menu entry, and the interim Havok clip strip inside it (its Load / Unload /
  Root motion buttons, its seven bare spin boxes and its binding paragraph) is
  deleted from `src/ui/widgets/timeline.cpp`. The Animation dock takes its seat
  in the Workspaces menu -- which it never had, being one past the end of the
  list -- so **Workspaces > Animation** now opens it.
* **Every number in the Animation dock is a scrub field, and none of them draws
  a native stepper.** Eleven fields; the two that still carried Qt's up/down
  arrows (Speed and Frame) lost them.
* **The bone-binding report is a few words.** The label reads **"78 of 95
  bones"** (14 characters) and its tooltip carries the whole sentence, 456
  characters, naming every bone that did not bind. The clip line and the
  sequence line were folded the same way. Nothing was deleted; only where it is
  shown moved.

Gates on the built exe: `water_ui.sh` 86/0 PASS (floor 72; it was 76/0 at
floor 62), `ui_align.sh` 11/0, `top_bar.sh` 43/5 (the same five),
`files_tab.sh` 28/2 (the same two), `animws.sh` 72/0 PASS (1 skip) -- gate (j) had never
run before and now does -- `hkxanim_ui.sh` 48/1 re-aimed at the
surviving dock, `loaded_nifs.sh` 166/3 against a control run of the
kept rung that reads 166/1. Pictures in
`scratchpad/ui6_20260910/images/`.
