#!/usr/bin/env python3
"""Lane UI6 -- write the three TEXT deliverables the director splices
(CONSTITUTION 8: lanes deliver changelog and handoff text, the director
splices it; this lane does NOT edit WW_CHANGES.md, HANDOFF.md or MISTAKES.md).

Numbers are filled in from the arguments so nothing here is typed twice.
"""
import io
import os
import sys

D = 'scratchpad/ui6_20260910/'

MISTAKES = r'''## 2026-09-11 -- lane UI6: the probe grabbed a transparent button and measured Qt's white fill

**What was done.** Part 2 of the lane's probe measured the gap between a
viewport-header button's glyph and its dropdown arrow by `b->grab()`, the method
`ww-qss-geometry-probe` section 3a and 3b both name. Every icon-only button came
back **UNREADABLE**: the ink scan found nothing at all, on four buttons out of
four, while the one button with a text label read fine.

**What was true instead.** `QWidget::grab()` renders into a QPixmap it allocated
and **filled with white**. An `autoRaise` QToolButton paints no background of its
own, so the grab's background is `#efefef` (luminance 239) and this theme's glyph
colour is `#e6e8eb` (luminance 232) -- **seven levels apart**, under a threshold
of 40. The buttons were drawn perfectly; the instrument could not see them.

**How it was found.** Because the probe printed `(UNREADABLE)` per button
instead of a number, and because case 0 of a probe has to reproduce the SHIPPED
state before anything after it is believed. A scan that finds nothing is not a
measurement of "nothing is there".

**The rule that prevents it.** Render over a fill you chose --
`QPixmap pm( w, h ); pm.fill( <the bar's colour> ); w->render( &pm, QPoint(),
QRegion(), QWidget::DrawChildren );` -- whenever the widget under test paints no
background of its own. `ww-qss-geometry-probe` is amended in both trees with a
new section 3c.

**And the same class in the probe's own fixture.** Its icons were built as
`QPixmap pm; QPainter p( &pm ); ...; return QIcon( pm );` -- the pixmap is copied
while the painter is still attached, so the icon came out empty. `p.end()` before
the copy. A rig whose FIXTURE is invisible reports the widget as correct and
empty, which is indistinguishable from the defect under test.

## 2026-09-11 -- lane UI6: a stylesheet put back is not a BOX put back, and it cost a link

**What was done.** Group A's floor appends `padding-right: 4px` over the shipped
button sheet (the 05:58:21 state), measures, then restores every victim with
`setStyleSheet( saved )` and three `processEvents()`, and asks the same question
again. The restore half went **RED** -- worst 1 px against a gate of 2 -- on a
window whose FIRST sweep, before anything was appended, had read 13 of 13 at 2
or better.

**What was true instead.** Two things at once, and only the second is the
instrument:

1. the sweep pinned and unpinned each button INSIDE its own measurement
   (`setFixedSize`, then restore), so a toolbar of thirteen buttons re-laid out
   thirteen times per sweep;
2. Qt keeps a widget's computed stylesheet box until the widget is re-polished.
   Assigning the old string back does not by itself make the box be worked out
   again, so the third sweep read every arrow two columns left of where the
   window draws it.

**How it was found.** By printing the restored sweep's table instead of only its
worst number: the same button read `arrow from 21, ink 18` in sweep 1 and
`arrow from 19, ink 17` in sweep 3 at the SAME width, which is not a number that
can come from the window.

**Cost.** Two links (one for the pin, one for the polish), both of one
translation unit, no application code.

**The rule that prevents it.** A sweep over several widgets pins them ALL, reads
them ALL, unpins them ALL -- one layout state per sweep. And a floor that puts a
sheet back re-polishes what it touched (`style()->unpolish( w ); polish( w );
updateGeometry();`) before it re-measures, or it is measuring a widget that is
half in the sabotage.

## 2026-09-11 -- lane UI6: the re-aimed harness waited on a timer with processEvents

**What was done.** `WW_HKXANIM_UI_TEST` was re-aimed from the retired Animation
Manager dock at the Animation dock. It kept the old dock's rhythm: call the
loader, `qApp->processEvents()`, read the list. Six checks went red at once --
the clip was not a row, the row did not say its frames, unloading did not remove
it, the refused file did not land.

**What was true instead.** The old dock rebuilt its list synchronously. The
Animation dock answers `clipsChanged` with `refreshLater()`, which arms a **50 ms
QTimer**, and `processEvents()` does not fire a timer that has not expired. Every
one of those checks was reading the list as it stood BEFORE the load.

**How it was found.** The first run of the re-aimed harness, and the tell was the
shape: `rows 0 -> 0, row -1` on a load whose own sentence said the clip had
bound.

**The rule that prevents it.** When a harness is re-aimed at a different widget,
its WAITS are part of what has to be re-aimed, not just its object names. Call
the surface's own synchronous rebuild (`refresh()`), never rely on
`processEvents()` to serve a `singleShot`.

## 2026-09-11 -- lane UI6: a gate that has never run, found by moving it

**What was done (and not by this lane).** `animws.sh`'s gate **(j)**, the
panel-style group -- the scrub-field count, the group-box count, the tooltip
count, the wheel guard, the pinned note and action bar -- lives inside the
sequence-NIF branch of `src/animworkspacetest.cpp`. The fixture that branch is
given is `10mmPistol.nif`, which has **no NiControllerSequence**, so the branch
takes its SKIP and calls `finish()`.

**What was true instead.** Every `animws.sh` run since lane HKXEDIT2 -- the 57/0
that four handoffs quote -- has finished without asking a single panel-style
question. The one visible sign was the SKIP line, which names the sequence NIF
and says nothing about (j).

**How it was found.** This lane added three checks to (j) and its count moved by
three and not by six. Moving (j) into a lambda that BOTH branches call took
`animws.sh` from 57 to 72 checks, and its very first execution found a real
defect (the next entry).

**The rule that prevents it.** A gate group that depends on an optional fixture
is written so it runs WITHOUT it, and a SKIP names every check it takes with it,
not just the one it is about. When a count moves by less than the number of
checks added, the difference is a group that did not run.

## 2026-09-11 -- lane UI6: two number fields were stamped as scrub fields and kept Qt's arrows

**What the first execution of gate (j) found.** Two of the Animation dock's
eleven number fields -- **Speed** and **Frame**, both on the transport row --
carry `wwScrubbed` and Qt's native up/down buttons at the same time.

**Why the old check could not see it.** It asked one question: does every
`QAbstractSpinBox` carry the `wwScrubbed` property. Both do.
`wwMakeScrubField` removes the native buttons only inside
`if ( spec.chrome && host->width() >= 4 * WW_ARROW_W )`
(`src/ui/widgets/wwnumberfield.cpp:959`), and the transport row asks for
`chrome = false` to save width -- so the stamp was applied and the buttons
stayed. bungo's ruling was *"we have a new standard for those sliders"*, and a
field with Qt's arrows on it is not that standard whether or not it is stamped.

**Fixed** by stating `setButtonSymbols( NoButtons )` on both, which is what the
chrome branch would have done. Drag or type; there is nothing left to squeeze.

**The rule that prevents it.** A stamp is a claim, not a measurement. Gate the
PROPERTY the user sees -- here, "no field in this panel draws a native stepper"
-- beside the stamp, and give it a floor that reproduces the defect.

## 2026-09-11 -- lane UI6: giving the arrows their column narrowed the left dock, and it was not predicted

**What was done.** Every menu button in the top row gained `padding-right: 8px`
so its dropdown arrow stops touching its glyph. Thirteen buttons, 4 px each.

**What was true as well.** The viewport header's minimum width grew with them,
and in the harness's window QMainWindow took the difference out of the LEFT
DOCK: measured, `dock tab strip w 174 -> 164`, `viewport header w 833 -> 857`.
Two `loaded_nifs.sh` checks then went red, because they probe the Loaded Files
name column at a FIXED `nameArea.left() + 60` and that point now lands on a
glyph.

**How it was found.** A control run of the kept rollback rung
(`release/NifSkope.before_ui6.exe`) read 166 / 1 twice while the new exe read
166 / 3 twice -- reproducible on both sides, so not flakiness -- and the two
strips' widths in the two `water_ui.sh` logs named the mechanism.

**The rule that prevents it.** A change that makes any widget WIDER in a row
that shares the window's width has a second effect on every dock beside it, and
the pre-registered gates have to carry it: "and no other panel's width moved by
more than N" belongs beside "the arrow has its air". This lane registered only
the arrow.
'''

WW_CHANGES = r'''### 2026-09-11 -- lane UI6: the segmented strips rejoined, the Animation Manager retired, the dropdown arrows given their own column

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

Gates on the built exe: `water_ui.sh` __WATERUI__ (floor 72; it was 76/0 at
floor 62), `ui_align.sh` 11/0, `top_bar.sh` 43/5 (the same five),
`files_tab.sh` 28/2 (the same two), `animws.sh` __ANIMWS__ -- gate (j) had never
run before and now does -- `hkxanim_ui.sh` __HKXANIMUI__ re-aimed at the
surviving dock, `loaded_nifs.sh` __LOADEDNIFS__ against a control run of the
kept rung that reads 166/1. Pictures in
`scratchpad/ui6_20260910/images/`.
'''


def main():
    vals = dict(a.split('=', 1) for a in sys.argv[1:])
    ww = WW_CHANGES
    for k, v in vals.items():
        ww = ww.replace('__%s__' % k, v)
    with io.open(D + 'MISTAKES_ENTRIES.md', 'w', encoding='utf-8', newline='\n') as f:
        f.write(MISTAKES)
    with io.open(D + 'WW_CHANGES_ENTRY.md', 'w', encoding='utf-8', newline='\n') as f:
        f.write(ww)
    print('wrote MISTAKES_ENTRIES.md (%d B) and WW_CHANGES_ENTRY.md (%d B)'
          % (os.path.getsize(D + 'MISTAKES_ENTRIES.md'),
             os.path.getsize(D + 'WW_CHANGES_ENTRY.md')))


if __name__ == '__main__':
    main()
