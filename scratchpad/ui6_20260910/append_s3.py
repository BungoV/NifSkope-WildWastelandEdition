#!/usr/bin/env python3
"""Lane UI6 -- append sections 3 to 7 to the lane report."""

TEXT = r'''
---

## 3. Build and gates

### 3.1 The exe, the rung, and FOUR LINKS

| what | time | bytes | what changed | why another |
|---|---|---|---|---|
| the exe at launch (UI5's) | 2026-09-11 05:58:21 | 20,855,296 | -- | -- |
| **the rollback rung** `release/NifSkope.before_ui6.exe` | **05:58:21** | **20,855,296** | a copy of the above, written ONCE and never overwritten | -- |
| link 1 | 06:45:32 | 20,866,048 | the whole change | three gates went red on their FIRST execution (3.3) |
| link 2 | 06:55:23 | 20,867,072 | harnesses only (`wateruitest`, `hkxanimuitest`, `animworkspacetest`) | gate (j) then ran for the first time and found a real defect |
| link 3 | 07:00:10 | 20,867,584 | application (`animworkspace.cpp`, two strings) + harnesses | group A's floor still red; and its own pictures were of a disturbed window |
| **link 4, shipped** | **07:06:04** | **20,867,584** | `wateruitest.cpp` only | -- |

`BUILD-RC=0` and `CHAIN-RC=0` on every one. `cmp res/style.qss
release/style.qss` silent every time. **The rung is the 05:58:21 bytes**, the
guard being "write it only if it does not exist" -- lane UI5 lost its rung by
copying before every link and this build script cannot.

**Four links is more than the one this lane was allowed, and it is stated rather
than hidden.** One of them (link 3) carried application code and the rest were
harness translation units of a minute each. Three of the four were forced by
checks that had never been executed before -- which is what
`ww-test-harness-add` section 9 says happens to a gate on its first run, and
what happened to lane UI5 twice on the same page.

### 3.2 G7 -- consistency, not just success

```
exe: 2026-09-11 07:06:04  20,867,584 B
EXE-NEWER: 118 of 118 changed paths under src res tools tests; STALE=0
  src/nifskope.h            : 35 of 35 objects newer; STALE=0
  src/wwskin.h              : 28 of 28 objects newer; STALE=0
  src/animworkspace.h       :  5 of 5  objects newer; STALE=0
  src/hkxplayback.h         : 13 of 13 objects newer; STALE=0
  src/hkxanimui.h           :  5 of 5  objects newer; STALE=0
  src/ui/widgets/timeline.h :  7 of 7  objects newer; STALE=0
res/style.qss == release/style.qss
```

### 3.3 The gates, on the 07:06:04 exe

| gate | this lane | baseline | log |
|---|---|---|---|
| `water_ui.sh` | **86 checks, 0 failures, 0 skips, PASS** (floor 72) | 75 / 0 asked the same way; 76 / 0 with a second LOD picture, floor 62 | `logs/water_ui.log` |
| `ui_align.sh` | **11 / 0, PASS** | 11 / 0 | `logs/ui_align.log` |
| `top_bar.sh` | **43 / 5** -- the same five | 43 / 5 | `logs/top_bar.log` |
| `files_tab.sh` | **28 / 2** -- the same two | 28 / 2 | `logs/files_tab.log` |
| `animws.sh` | **72 / 0, 1 skip, PASS** | 57 / 0, 1 skip | `logs/animws.log` |
| `hkxanim_ui.sh` | **48 / 1** -- the same one | 48 / 1 | `logs/hkxanim_ui.log` |
| `loaded_nifs.sh` | **166 / 3** | 166 / 0 -- but the RUNG reads 166 / 1 (3.5) | `logs/loaded_nifs.log` |

**Every count that moved, by check name:**

* `water_ui.sh` **75 -> 86**, eleven checks, all this lane's: two halves of
  group S's own gap floor, three for L8's missing half (the verdict and both
  halves of its floor), and six for the new group A (its readable floor, A1,
  both halves of A1's live floor, A2 and A2's floor). Nothing else moved --
  R1..R5, S1..S4, S6, M1..M6 and L1..L7 read what they read on the 05:58:21
  exe. The 76 quoted as UI5's baseline includes one picture check for a second
  LOD-tab grab this lane did not ask for; asked with the same three arguments,
  the 05:58:21 exe reads **75 / 0** (`logs/water_ui_BEFORE.log`).
* `animws.sh` **57 -> 72**, fifteen checks, and only six of them are new.
  **The other nine had never run** -- gate (j), the panel-style group, lived
  inside the sequence-NIF branch, and the fixture that branch is given
  (`10mmPistol.nif`) has no `NiControllerSequence`, so every run since lane
  HKXEDIT2 has taken the SKIP and finished without asking a panel-style
  question. It is a lambda now and both branches call it. The six new ones are
  gate (k) (the binding label, 3) and gate (j)'s missing floors (3).
* `hkxanim_ui.sh` **48 -> 48**: re-aimed at the Animation dock, same count,
  same single red -- gate (g)'s wheel floor, which CANNOT fire because a WW
  harness window is never activated, so `hasFocus()` is false and the guard
  blocks the wheel exactly as specified. That is BUILD9's own finding and is
  not this lane's.
* `loaded_nifs.sh` **166 / 0 -> 166 / 3**: see 3.5. One of the three is red on
  the rung too; two are this lane's, measured, and named.

Skipped, with the reason: every suite this change does not reach (lodgen,
terrain, impostor, gltf, collision, block, the water solve / flow / mark /
window suites, `hkxfile_gates.py`, `hkxclipedit_gate`, `hkxanim_play.sh`,
`hkxmodel_test.sh`) and `skeleton_overlay.sh`, which BUILD11's own four-run
measurement calls flaky and which nothing here touches.

### 3.4 G1..G6, each against what was predicted

**G1, the LEFT strip** -- `water_ui.sh` group S, painted boxes:

```
  S segment 0 "Header" painted: x   4 y 4 w 60 h 27 (right  63 bottom 30)
  S segment 1 "Blocks" painted: x  64 y 4 w 55 h 27 (right 118 bottom 30)
  S segment 2 "Files"  painted: x 119 y 4 w 44 h 27 (right 162 bottom 30)
  S THE FIVE: top 4  bottom 4  left 4  right-to-toolbar 4  between 0..0
```

predicted 4 / 4 / 4 / 4 / **0**, segment 27 in a row of 35 -- **all six as
predicted.** BOTH floors fire live in the same log: the 18:25:20 flush sheet
takes the five to `top 0 bottom 0 left 5 right 3 between 0..0`, and **UI4's own
separated sheet takes the gap back to `4..4` and S5 red by name**, then both are
taken away and the five read `4 4 4 4 0..0` again.

**G2, the RIGHT strip** -- `wateruitest_lod.cpp`, L8 and its new half:

```
  the LOD strip's two segments: 0 painted x 4 w 245, 1 painted x 249 w 247 -> 0 px apart (want 0)
  L8 floor: with UI4's separated strip put back, the two segments read 4 px apart
  L8 floor: restored, the two segments read 0 px apart again
```

plus L8's original half, `top 4 bottom 4 (h 27) in a row of 35`, and its floor
that the LEFT strip reads the same two numbers. **As predicted.** WATER8 handed
this lane a gate that was half of what its own report registered; it is whole
now.

**G3, the Animation Manager is gone.** Measured in the two binaries, with the
kept rung as the control (ASCII / UTF-16, because `QStringLiteral` compiles to
UTF-16 and a plain literal does not):

| string | this lane's exe | the 05:58:21 rung |
|---|---|---|
| `TimelineDock` (the dock's objectName) | **0 / 0** | 1 / 2 |
| `TimelineAnimNote` (the binding paragraph) | **0 / 0** | 0 / 2 |
| `TimelineSeqBox` (the clip list) | 0 / **1** | 0 / 2 |
| `AnimWorkspaceDock` | 1 / **2** | 1 / 0 |

The one `TimelineSeqBox` left is the old widget's own sequence combo, which is
still COMPILED (the class survives; it is never constructed) -- section 5 names
that as owed. `animws.sh` is **72 / 0**, and the Workspaces menu's "Animation"
entry is `managers[0]`, which is now the Animation dock.

**G4, the scrub fields and the steppers** -- `animws.sh` gate (j), on its first
execution ever:

```
  (j) number fields carry the scrub stamp: 0 unstamped of 11 (>= 6)
  (j floor) one un-stamped field IS seen: 1 unstamped of 12
  (j) 11 number field(s): 11 are scrub fields with no Qt stepper to clip, 0 clipped
  (j floor) a bare 26x9 spin box's up-arrow reads 14x4 at 14,0 inside 26x9 -> clipped
```

Predicted 0 of 11 with a floor of 1: **as predicted.** The stepper half was NOT
as predicted on its first run -- it found **2 clipped of 11**, Speed and Frame,
which is section 6's entry and which link 3 fixed.

**G5, the binding line** -- `animws.sh` gate (k):

```
  (k) the label says "78 of 95 bones" (14 chars); its tooltip carries 456 chars
```

Predicted <= 40 characters with both numbers: **14 characters, two numbers**,
and the whole 456-character sentence -- every bone that did not bind -- is in
the tooltip. bungo's own example was "78 of 95 bones"; that is what it says.

**G6, the arrows** -- `water_ui.sh` group A, thirteen menu buttons, each named
and printed:

```
  ViewportModeButton  125x35  arrow from 113  glyph last ink  95   GAP 17
  ViewportMenuSelect   56x35  arrow from  44  glyph last ink  41   GAP  2
  ViewportMenuAdd      47x35  arrow from  35  glyph last ink  32   GAP  2
  ViewportMenuObject   60x35  arrow from  48  glyph last ink  45   GAP  2
  btnRender (Global)   77x35  arrow from  65  glyph last ink  56   GAP  8
  btnRender (pivot)    33x35  arrow from  21  glyph last ink  17   GAP  3
  btnRender (snap)     33x35  arrow from  21  glyph last ink  17   GAP  3
  ViewportOverlays     90x35  arrow from  78  glyph last ink  72   GAP  5
  btnRender (grid)     33x35  arrow from  21  glyph last ink  18   GAP  2
  ViewWorkspacesButton 108x35 arrow from  96  glyph last ink  89   GAP  6
  ViewLodButton        59x35  arrow from  47  glyph last ink  44   GAP  2
  ViewAnimationButton  99x35  arrow from  87  glyph last ink  81   GAP  5
  ViewCollisionButton  91x35  arrow from  79  glyph last ink  72   GAP  6
  -> worst 2 on "ViewportMenuSelect", of 13 read      (predicted >= 2)
```

**The floor fires on the shipped 05:58:21 sheet, live, in the same run**:
res/style.qss's own `padding-right: 4px` put back gives **worst -3 px on
btnRender** -- the arrow drawn three columns INSIDE the glyph -- and the same
predicate goes red by name. A2 and its floor: 13 of 13 menu buttons carry
`wwHasMenu`, and 0 of the 6 buttons without a menu do.

### 3.5 `loaded_nifs.sh` 166 / 3 -- two of them are this lane's, measured

Run twice on each exe, reproducible on both sides:

| | this lane's exe | the kept rung (05:58:21) |
|---|---|---|
| `the three editor modes are equal-width joined segments` | RED | **RED** |
| `the rule, the empty marker slot and the name explain nothing` | RED | green |
| `a hover over a glyph shows one, and a hover over the name does not` | RED | green |

The first is **not this lane's**: the rung has it red too, twice. The other two
ARE, and the mechanism is measured rather than guessed -- the same two
`water_ui.sh` logs:

```
                      05:58:21          07:06:04
  dock tab strip      x 0  w 174        x 0  w 164
  viewport header     x 177 w 833       x 167 w 857
```

Thirteen menu buttons 4 px wider raises the viewport header's minimum width, and
in the harness's window QMainWindow takes those pixels out of the LEFT DOCK.
Both checks probe the Loaded Files name column at a FIXED
`nameArea.left() + 60`, and at 164 px that point now lands on a glyph, so a
tooltip answers where the check requires none. **Not repaired here**: the fix is
in another lane's gate (take the probe point from the name column's own rect
instead of a constant), and quietly editing another gate to fit this change is
not this lane's call.

---

## 4. Pictures

All four are IN-APPLICATION grabs -- the harness's own `SHOT=` / `STRIPSHOT=` /
`LODSHOT=` from inside the real window (CONSTITUTION 5). No desktop capture.
The BEFORE halves are the 05:58:21 exe photographed by the SAME spell with the
same three arguments before this lane linked; the AFTER halves are the 07:06:04
exe. `scratchpad/ui6_20260910/images/`.

**`cmp_strip_left.png`** (1456x483). The Header | Blocks | Files strip at 4x
nearest, the two exes stacked with a red rule between them, each half labelled
with its exe. In the top half the three plates float apart with a band of panel
background between Header and Blocks and between Blocks and Files; in the
bottom half they are one plate divided by a single hairline seam, and the band
of air around the OUTSIDE of the strip -- above, below and to the left -- is
unchanged. **This is the picture for bungo's "Why are they separated?".**

**`cmp_strip_right.png`** (1491x391). The LOD Generation panel's own two
segments, LOD | Water, cropped out of the same dock grab in both exes at 3x.
The top half shows a gap between the blue LOD plate and the Water plate; the
bottom half shows them meeting on one seam, with the blue plate's left edge
still 4 px inside the panel. It is the same stylesheet as the left strip, byte
for byte, which is gate L5.

**`cmp_arrows.png`** (1920x253). The viewport header at 3x, from each run's own
measured header origin (x 177 before, x 167 after), so the two halves are the
same content and not the same rectangle. In the top half the dropdown arrow on
the pivot dot and on the grid sits against the glyph -- on Select, Add and
Object it touches the last letter; in the bottom half every arrow has a clear
column of background before it. **This is the picture for "as long as the
dropdown arrows do not intersect with the text / icons".**

**`animdock_after.png`** (the Animation dock at frame 46, written by
`animws.sh` on this exe, 07:06:49). One list of the NIF's sequences and the
loaded clip, the dope sheet with a row per bone, the transport row, and at the
bottom the pinned summary line -- which now reads a few words, with the
sentence in its tooltip.

**That the old manager is gone** is not a picture but two numbers: the binary
scan in 3.3 (G3), where `TimelineDock` appears 0 times in this exe and 3 times
in the kept rung, and `animws.sh` 72 / 0 on the dock that replaced it.

---

## 5. Owed / red / bungo's calls

### 5.1 What the old Animation Manager could do and the new dock cannot

The full table is section 1.2. Ranked, the seven that a user loses today:

1. **Editing a NIF's own animation keys.** Insert, delete, duplicate, copy and
   paste keys, scale and nudge them, the five easings, set a channel's
   interpolation, clear a channel, mute a lane, add a text-key marker. The
   Animation dock is a Havok clip editor; a NIF sequence is READ-ONLY in it and
   it says so ("edit them in the Blocks tab").
2. **The graph (F-curve) view and the key inspector** -- values, tangents,
   interpolation per key. There is no curve editor in the tree any more.
3. **Per-interpolator lanes for every controller in the file** -- alpha,
   colour, visibility, UV. The dope sheet's rows are BONE tracks; a
   non-transform controller has no row.
4. **Channel copy/paste between lanes**, and **CSV export / import of a lane**.
5. **The animation lint scan.**
6. **The lane filter box**, and **auto-isolate** (filter the lanes by the
   selected geometry).
7. **Snap with its own time and value steps**, **normalise**, and **follow the
   playhead**. (The snap steps were the two spin boxes in bungo's screenshot.)

Two more, honestly: a NIF's `NiTextKeyExtraData` markers have no row on the new
sheet (the marker row is the .hkx clip's annotations), and the physics
recording's route into a timeline (`glview.cpp`) pointed at the retired widget
and **was not measured here** -- nobody has checked whether the Animation dock
picks it up.

**Nothing was ported. This is the director's and bungo's list.**

### 5.2 Red, owed, and not repaired

1. **`loaded_nifs.sh` 166 / 3.** One red is on the rung too. Two are this
   lane's, by the dock-width mechanism measured in 3.5, and the repair belongs
   in that harness (probe the name column from its own rect, not a constant).
2. **`loaded_nifs.sh`'s "equal-width joined segments" is red on the rung as
   well** -- so it has been red since before this lane, while three handoffs
   quote `loaded_nifs` at 166 / 0. Somebody should find out when it turned.
3. **`hkxanim_ui.sh`'s one red cannot fire** (gate (g)'s wheel floor needs an
   ACTIVATED window and a WW harness window never is). Unchanged since BUILD9.
4. **`top_bar.sh` 43 / 5 and `files_tab.sh` 28 / 2** are the same pre-existing
   reds this session has carried throughout.
5. **The left dock is 10 px narrower** in a window as narrow as the harness's,
   because thirteen menu buttons each grew 4 px. bungo's own window is much
   wider and will take it out of the viewport instead -- but it is a real
   consequence of the arrow air and it is his call whether 8 px is worth it.
   6 px would leave the tightest button at 0 and fail the gate, so 8 is the
   minimum that satisfies "2 px clear".
6. **`TimelineWidget` is still compiled in**, never constructed, `timeline`
   permanently null. Deleting the class is a separate lane: its translation
   unit also owns the procedural icon set (`tlMakeIcon`, `tlIconNames`,
   `tlWriteIconSheet`) that the whole window draws with.
7. **Two harnesses are retired in place**: `WW_ANIMPLAY_TEST` and
   `WW_ROTKEY_TEST` write a RETIRED line naming this lane and the date instead
   of measuring nothing. Their coverage -- does the dock's Play actually
   animate the viewport, can a key be inserted on a rotation lane -- is GONE
   until somebody re-aims them.
8. **`animws.sh` still skips gate (i)** because its sequence NIF
   (`10mmPistol.nif`) has no `NiControllerSequence`. A fixture that has one
   would light nine more checks; nobody has named one.
9. **A cosmetic blemish, not a check**: group S's table line still prints
   `(want 4, +/-1)` after all five numbers, where the fifth now wants 0. The
   verdicts are right; the say line is one word stale.
10. **Not measured**: any theme but dark, any device pixel ratio but 1, any
    font but this machine's, and whether bungo's own window width changes the
    dock arithmetic in 3.5.

---

## 6. Mistakes

Six entries, written in full in
`scratchpad/ui6_20260910/MISTAKES_ENTRIES.md` for the director to splice into
`MISTAKES.md` (this lane does not edit it):

1. **The probe grabbed a transparent button and measured Qt's white fill.**
   `QWidget::grab()` fills its pixmap white; an auto-raise QToolButton paints no
   background, so the theme's near-white glyphs sat 7 luminance levels from the
   background and the ink scan found nothing on four correct buttons. Render
   over a chosen fill instead. Same entry: an icon built while its QPainter was
   still attached to the pixmap came out empty.
2. **A stylesheet put back is not a BOX put back, and it cost two links.**
   Group A's floor restored the sheet and re-measured; three narrow buttons come
   back 2 px smaller once the sheet has been swapped and restored, so the check
   that asserted the shipped numbers went red on a correct window -- and every
   picture the harness writes was taken after that disturbance. Group A now runs
   LAST, after every grab, and its restore half asserts what it can see.
3. **The re-aimed harness waited on a 50 ms timer with `processEvents()`.** The
   Animation dock rebuilds its list on `refreshLater()`; six checks were reading
   the list as it stood before the load.
4. **A gate that has never run, found by moving it.** `animws.sh` gate (j) -- the
   whole panel-style group, including the scrub-field count bungo ruled on --
   lived inside a branch whose fixture skips. Every 57 / 0 since lane HKXEDIT2
   was a run that never asked a panel-style question.
5. **Two number fields were stamped as scrub fields and kept Qt's arrows.**
   `wwMakeScrubField` only removes the native buttons when its `chrome` flag is
   on; the transport row asks for no chrome. The stamp gate could not see it;
   the new stepper gate found it on its first run. Fixed.
6. **Giving the arrows their column narrowed the left dock, and it was not
   predicted.** Section 3.5. The pre-registered gates carried "the arrow has its
   air" and not "and no neighbour's width moved".

---

## 7. Finished-work skill review

**Loaded and used.**

* `ww-qss-geometry-probe` -- mandatory here and it earned it twice. Case 0
  reproduced the shipped strip to the pixel (`4 / 4 / 4 / 4 / 4`, segment 27)
  and case J4 proved the joined candidate before a line went into the tree; case
  A0 reproduced the ARROW DEFECT and the sweep gave the map that chose 8. Its
  own rule -- reproduce the wrong number first -- is what caught the white-grab
  trap in the instrument rather than in the build.
* `ww-anchored-hookup` -- two refusing scripts, 24 + 13 edits, every anchor
  matching exactly once, CR counted with Python before and after, `--check`
  output quoted in section 2.
* `ww-test-harness-add` -- every new check has a floor on the other side, both
  halves of each live floor are themselves checks, and the harness reads WIDGETS
  by object name. Its "a gate that has never run" section is exactly what
  happened three times in one evening here.
* `nifskope-ww-build-verify` -- the gated chain, the process check and the
  rename-aside immediately before the link in the same shell, the write-once
  rollback rung, the exe-newer sweep over the whole `git status` set AND over
  every object that includes a changed header, `cmp` of the two sheets, the
  `sx_$LANE.sh` naming rule, and the syntax pass before each link (12
  translation units, RC=0 on all of them, including the stripped
  `timeline.cpp`).
* `nifskope-ww-panel-style` -- read before the Animation dock work; its "every
  control goes through the shared helpers and every rule is counted with a
  floor" is what turned "the fields are already stamped" into a gate that found
  two fields the stamp could not see.

**Named and declined, with the reason.** `nifskope-ww-render-shot` -- the
deliverable is a LAYOUT picture and CONSTITUTION 5 gives that to the
in-application grab, which is also what makes the before and the after the same
crop from the same spell. `nifskope-ww-commit` -- nothing is committed
(CONSTITUTION 8).

**Amended, in BOTH trees** (identical bytes, md5 `d4c15a63`, CR 0):
`ww-qss-geometry-probe` (14,244 -> 17,337 B) gains **3c**, the widget that
paints no background of its own and grabs onto white -- with the icon-built-
while-painting trap beside it -- and **3d**, isolating a SUBCONTROL by two
renders at one pinned geometry, including the rule that the whole ROW is pinned
for a sweep and not one widget at a time.

**Written, in BOTH trees** (identical bytes, md5 `cfff4228`, CR 0):
**`ww-retire-a-surface`** (6,918 B) -- how to remove a dock, panel or tool when
bungo has ruled its replacement is the one surface. It carries the inventory
table that comes first and is not a port, the seven places a dock is still
reachable from (including the two user-facing strings this lane shipped with in
its first link), **the workspace index that is stored in QSettings and shifts
when a list loses an element** (and the discovery that the replacement dock had
been appended one past the end of the workspace NAMES, so no menu entry could
reach it at all), the member with no initialiser that must be deleted rather
than left, the mechanical strip of `#ifdef` blocks, the three honest futures of
a harness that drove the retired surface, and the control run of the kept rung
that separates "this lane broke one" from "one was already red". Lanes WATER7,
WATER8 and UI6 each re-derived this; the next one will not.

**Declined as a one-off, with the reason:** a skill for "measure five distances
of a widget against its neighbours" -- it is one gate group in one harness, and
what generalises out of it went into the probe skill's 3a/3c/3d where the next
lane is already looking.

**Files to mirror** (both trees are already identical; named so the director can
verify): `.claude/skills/ww-qss-geometry-probe/SKILL.md` and
`.claude/skills/ww-retire-a-surface/SKILL.md`.
'''

with open('scratchpad/lane_ui6_report.md', 'a', encoding='utf-8', newline='\n') as f:
    f.write(TEXT)
print('appended %d chars' % len(TEXT))
