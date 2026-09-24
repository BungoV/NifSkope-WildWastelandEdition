# Lane UI5 -- File / View / Spells / Options / Help vertically centred in the 35-px menu row

Tree `E:\Projects\NifskopeWildWastelandEdition`, branch `main`, working tree.
Nothing committed (CONSTITUTION 8). Written incrementally.

## 0. Pre-registered gates

Written BEFORE any code or any new measurement, from the dead UI5 instance's
`probe_out.txt` (cases 0, 1, A22..A36, B28..B36, C0..C8 only -- its D and E
cases were added to `probe.cpp` at 21:00, two minutes after the run, and have
never been executed) and from UI3's / UI4's handoff blocks.

**What I predict, as numbers:**

| gate | prediction |
|---|---|
| U1 case 0 | the probe's shipped case reads ink centre **10.5** in a 35-px bar whose centre is **17.0**, i.e. the five titles sit **6.5 px HIGH**; `measure_before.py` on the shipped grab `scratchpad/ui4_20260910/images/strip_after.png` reads ink centre **11.0 / 11.5**, offset **-6.0 / -5.5**. The two instruments must agree within 1 px. |
| U2 after | all five ink centres within **1 px** of 17.0; menu bar height **35** unchanged; `sizeHint().height()` of the menu bar **<= 35** in the chosen case (the refuter: above 35 and `wwAlignBarRow` grows the whole row); tab strip / tFile / tView / viewport header / search row tops and heights identical to UI4's `ui_align.sh` dump. |
| U3 way back | `UI/CompactTopBars=false` puts the items back at the old offset **exactly** (no menu-item rule emitted at all, as `wwBarRowButtonQss` already refuses at its off value). |
| U4 floor | one check red **live in the same log**: the shipped (pre-change) menu-item arithmetic appended over the new sheet, the same predicate asked again, ink offset back at about **-6**, named red, then restored. |
| gate counts | `water_ui.sh` 48 + WATER8's landed additions + this lane's group M; `ui_align.sh` 11/0, `top_bar.sh` 43/5, `files_tab.sh` 28/2, `animws.sh` 57/0 unchanged. |

**The mechanism I predict will win** (to be confirmed or refuted by the probe,
not assumed): not `min-height` -- the probe's cases A22..A36 move the item's
height not one pixel, so `QMenuBar::item { min-height }` is not consulted on
this path at all -- and not a margin, because `actionGeometry()` includes a
margin exactly as `QTabBar::tabRect()` did for UI4. The two vertical PADDINGS
are consulted (the shipped item is 20 px = 16 of text + 2 + 2), so the row's
spare pixels get split above and below the item's own measured content height,
stated once from `wwBarRowHeight()` the way UI3 states button air.

**Predicted arithmetic:** content 16, row 35, padding-top `(35-16)/2 = 9`,
padding-bottom `35-16-9 = 10` -> painted item box 0..34, box offset **0.0**,
ink offset **+0.5** (the ink sits 1 px below its own content centre: the shipped
case has content rows 2..17 and ink 6..15).

**Refuted in advance:** any case whose menu-bar `sizeHint().height()` exceeds 35
does not ship, whatever its ink offset -- CONSTITUTION 7, refused-with-numbers
is a deliverable.

## 1. Case 0

**MEASURED** (`scratchpad/ui5_20260910/probe_out2.txt`, the whole run; the rig
is `scratchpad/ui5_20260910/probe.cpp`, built by `build_probe.sh` into
`release/ui5_probe.exe`, run `-platform offscreen` from the repo root -- it
links Qt6Widgets only, cannot touch `release/NifSkope.exe`, `GeneratedFiles/`
or the Makefile, and was safe to run while lane WATER8-GATE held the exe).

Two instruments, both quoted:

| instrument | what it read | ink centre | row centre | offset |
|---|---|---|---|---|
| `measure_before.py` on `scratchpad/ui4_20260910/images/strip_after.png` (the SHIPPED 20:45:47 window, in-app grab, its first 35 rows ARE the menu row) | File 6..16, View 6..16, Spells/Options/Help 6..17 | **11.0 / 11.5** | 17.0 | **-6.0 / -5.5** |
| probe `case 0` (shipped sheet, tokens substituted, 35-px QMenuBar, five titles) | ink y 6..15 on all five | **10.5** | 17.0 | **-6.5** |

**U1 PASSES: the two agree to 0.5 px** (1 px allowed). The offscreen probe's
text is one row shorter than the real window's because the grab has no
sub-pixel antialiasing, which moves the ink's bottom row and so its centre by
half a pixel; the top row, 6, is identical in both.

So the shipped state is: the five titles sit **6 px high** in the 35-px row --
the item's painted box is `y 0..19` (20 px) at the top of a 35-px bar, and the
text inside it is 10 px tall at `y 6..15`. That is what bungo photographed.

Two supporting numbers from the same run:
* `res/style.qss:48` is `QMenuBar::item { padding: 4px 8px; }`; the row's own
  sheet (`wwBarRowButtonQss`, `src/nifskope_ui.cpp:757`) overrides the vertical
  half with `padding-top: 2px; padding-bottom: 2px`, so the item is
  `2 + 16 + 2 = 20`.
* the item's own content is therefore **16 px** (probe case D0: with both
  vertical paddings zeroed the item is exactly 16 px tall).

## 2. The mechanism

The sweep, all in one run, with the refuter in its own column. "hint" is the
menu bar's `sizeHint().height()` measured with the min/max pin lifted -- the
state `wwAlignBarRow` measures in, because it aligns before it pins. A case
whose hint exceeds 35 would make the whole row grow the next time the row is
aligned and does not ship, whatever its ink offset.

| case | what it states | item h | painted box centre | INK centre | ink offset | hint (refuter) |
|---|---|---|---|---|---|---|
| 0 (shipped) | `min-height 26`, `padding 2/2` | 20 | 9.5 | 10.5 | **-6.5** | 20 -- ok |
| 1 / G (way back) | no row sheet at all | 24 | 11.5 | 12.5 | -4.5 | 24 -- ok |
| A22..A36 | `::item min-height` 22,24,26,28,30,32,34,36 | **20 in every one** | 9.5 | 10.5 | -6.5 | 20 -- ok |
| B28..B36 | the same plus the QMenuBar's own box zeroed | **20 in every one** | 9.5 | 10.5 | -6.5 | 20 -- ok |
| C0/C2/C4/C6/C8 | `::item margin-top` 0,2,4,6,8 | 20,22,24,26,28 | 9.5..17.5 | 10.5,12.5,14.5,16.5,18.5 | -6.5,-4.5,-2.5,-0.5,+1.5 | 20..28 -- ok |
| D0 | `::item padding` 0/0 | 16 | 7.5 | 8.5 | -8.5 | 16 -- ok |
| D2..D12 | `::item padding-top` 2..12, bottom = 19 - top | **35** | **17.0** | 10.5,12.5,14.5,16.5,18.5,20.5 | -6.5,-4.5,-2.5,**-0.5**,+1.5,+3.5 | **35 -- ok** |
| **D\*** | **padding-top 9 / bottom 10, derived: `(35-16)/2`** | **35** | **17.0 (offset 0.0)** | **17.5** | **+0.5** | **35 -- ok** |
| F | the same derived from a LIVE calibration | 35 | 17.0 | 17.5 | **+0.5** | 35 -- ok |

**Three things the sweep settles, none of which was believed beforehand:**

1. **`QMenuBar::item { min-height }` is not consulted at all.** Fifteen cases
   (A22..A36, B28..B36) move the item's height by zero pixels. The row's
   existing sheet has stated a menu-item `min-height` since lane WATER7 and it
   has never done anything.
2. **A margin would work but cannot be gated.** Cases C move the item, and
   `actionGeometry()` grows with the margin -- so the rect lies about where the
   item is drawn, exactly as `QTabBar::tabRect()` did for lane UI4. Rejected in
   favour of padding, which leaves the rect honest (the painted box and the rect
   are the same 35 px in every D case).
3. **The two vertical PADDINGS are consulted, and they are all that is needed.**
   The item's own content is 16 px; the row has 35; splitting the 19 spare
   pixels 9 above and 10 below puts the item's painted box on the row exactly
   (offset 0.0) and its text within half a pixel of the row's centre line
   (+0.5). No integer split can do better: the ink sits one pixel below its own
   content's centre, so 8/11 gives -0.5 and 9/10 gives +0.5.
   **Superseded by 4.3** -- the probe's "+0.5" is the probe's own ink band, and
   the real window's is 1 px taller (antialiasing) and carries the mnemonic
   underline. The conclusion that the even split is right survives; the number
   to quote is the CAP BAND's, which is 0.0.

**The refuter did not fire.** Every candidate that centres the items leaves the
menu bar's hint at exactly 35 -- the row's own height -- so `wwAlignBarRow`
still takes 35 from the tallest bar and nothing else in the row moves. There is
no case in the sweep that centres the text AND grows the row, so nothing has to
be refused.

**Why the derived form and not the literal 9** (probe case F): the whole flow
was run the way `wwAlignBarRow` will run it -- pin the row, apply the row sheet
with the menu item's vertical padding zeroed, read the item height back with
`ensurePolished()` and **no event loop**, derive the two paddings, apply. Case E
confirms the read-back works without an event loop (16 both with and without),
which is the one thing that could have forced a hard-coded number: the row is
aligned during construction and cannot pump events. So the code measures 16 the
way UI3 measures its button overhead, and the only literal in the change is the
even split itself.

## 3. The change

Four files, nine edits, all of them through one refusing script.

**Why a script and not an edit.** Lane WATER8-GATE was alive in this tree the
whole time this lane wrote code. It touches no source (its own brief: *"Touch
NO source, QSS or .pro file"*), but its gate G4 is an exe-newer sweep over
`git status --porcelain -- src res tools tests` that must print no STALE line --
so touching any tracked file under those directories before its `DONE` marker
appeared would have put a false red in another lane's verdict. And
`tests/spells/water_ui.sh` is worse than that: WATER8-GATE was RUNNING it, so a
floor raised from 48 to 62 under its feet would have failed a spell that was
measuring a different exe. Nothing was applied until `DONE` was on disk.

`scratchpad/ui5_20260910/hookup.py`, `--check` (the default; writes nothing) --
output kept at `scratchpad/ui5_20260910/hookup_check.txt`:

```
src/wwskin.h               after    anchor x1  already x0  CR 0  -- wwBarRowMenuItemQss declared
src/nifskope_ui.cpp        after    anchor x1  already x0  CR 0  -- wwBarRowMenuItemQss defined
src/nifskope_ui.cpp        replace  anchor x1  already x0  CR 0  -- applyRow takes the menu rule
src/nifskope_ui.cpp        replace  anchor x1  already x0  CR 0  -- wwAlignBarRow calibrates the item
src/wateruitest.cpp        replace  anchor x1  already x0  CR 0  -- QHash for the ink histogram
src/wateruitest.cpp        after    anchor x1  already x0  CR 0  -- menuInk() helper
src/wateruitest.cpp        replace  anchor x1  already x0  CR 0  -- group M
tests/spells/water_ui.sh   replace  anchor x1  already x0  CR 0  -- group M gate names
tests/spells/water_ui.sh   replace  anchor x1  already x0  CR 0  -- floor 48 -> 62

9 of 9 edits match once
--check only, nothing written (pass --apply to write)
```

Every anchor carries the file's real line ending (all four files are LF-only,
`b.count(b'\r')` == 0 measured with Python before and asserted equal after), an
edit already present REFUSES rather than doubling, and `--apply` writes all four
files or none.

**What the change is, in the shared skin, no `setFixedHeight` anywhere:**

* `wwBarRowMenuItemQss( rowHeight, itemContent )` (new, `src/nifskope_ui.cpp`,
  declared in `src/wwskin.h`) returns one rule --
  `QMenuBar::item { padding-top: Npx; padding-bottom: Mpx; }` -- with
  `N = (rowHeight - itemContent) / 2` and `M` the remainder. Nothing horizontal,
  so `res/style.qss`'s own 8 px on either side of a title is untouched. It
  returns an empty string when `UI/CompactTopBars` is false, or when either
  number is zero: the same one reader, `wwCompactTopBars()`, that both other
  sheets go through.
* `wwAlignBarRow` now MEASURES `itemContent` instead of typing 16. It finds the
  QMenuBar among the bars it was given, applies the row's sheet once more with
  both vertical paddings zeroed, calls `ensurePolished()` and reads the tallest
  `actionGeometry().height()` back. No event loop is pumped, because there is
  none to pump during construction -- probe case E is what proves that read-back
  works. Its fallback, named in `wwBarRowMenuItemContent()`: a menu bar with no
  titles, or one whose items read back taller than the row, takes the font's own
  line height clamped to the row rather than shipping no rule at all.
* `applyRow` (the lambda inside `wwAlignBarRow`) takes the menu rule as a second
  argument and appends it LAST, after `wwBarRowButtonQss`'s own menu-item rule --
  same selector, same specificity, so the later declaration is the one Qt keeps.
* `wwBarRowMenuItemContent()`, `wwBarRowMenuItemPadTop()` and
  `wwBarRowMenuItemPadBottom()` read the three numbers back, for the gate and
  the log. The paddings are -1 until the row has been aligned.

**Gate group M** (`src/wateruitest.cpp`, 14 checks) plus a new `menuInk()`
helper that reads the text ink out of the live menu bar's own grab, by the same
method `measure_before.py` used on the shipped picture -- so the harness's number
and this report's number are one number. The group runs BEFORE the top-strip
picture and both halves of its floor are checks, so whatever it appends is
proved taken away again.

M1 the menu bar is still exactly the row's height at y 0; M2 every title's text
within 1 px of the row's centre line, and the five within 1 px of each other;
M3 the refuter -- the menu bar's own size hint does not exceed the row, and every
other bar in the row is still the row's height; M4 the way back, live (the row's
sheet taken off the menu bar, which is the whole of the off value for the titles,
must read the pre-UI3 position), plus its own floor that the skin states no rule
for a zero row or a zero content; M5 the floor that fires -- the 20:45:47
arithmetic (`padding-top: 2px; padding-bottom: 2px`) appended live, the SAME M2
predicate asked again, red, then taken away and green; M6 the rule is one top and
one bottom padding whose three numbers add up to the row, and nothing horizontal.
Two more floors above all of it: every title found as real ink (5 of 5), and the
SAME scan asked for a brightness the bar does not carry finds nothing.

`tests/spells/water_ui.sh` reads the 14 new gates back by name and its check
floor goes 48 -> 62.

**What M4 does NOT prove, said plainly.** It does not flip
`UI/CompactTopBars` in QSettings. This harness runs against the user's real
profile and a harness never writes a state it did not make, so the setting-level
way back is proved two other ways instead: the sheet is the whole of the off
value for the titles (with `UI/CompactTopBars` false `wwAlignBarRow` returns
before it appends anything, so the menu bar carries `res/style.qss` alone -- which
is exactly what M4 measures live), and `wwBarRowMenuItemQss` reads the same
single `wwCompactTopBars()` reader the other two sheets do. Group R already
SKIPS by name when the setting is off, and group M does the same.

**Syntax pass, on patched overlay copies** (the real files could not be touched
yet), flags read out of `Makefile.Release`, script `sx_UI5.sh` at the repo root:

```
== sxui5/wateruitest.cpp    RC=0
== sxui5/nifskope_ui.cpp    RC=0
```

Both clean; the only diagnostics are the file's own pre-existing warnings
(`_USE_MATH_DEFINES` redefined, two `/*` inside comments at lines 26893 and
29515, an unused `findings` at 7202, a `nodiscard` QFile::open at 22261), none
of them in the added code. A syntax pass proves the files COMPILE and nothing
about linking or behaviour.

## 4. Build and gates

### 4.1 The slot, and why the build is in two halves

Lane WATER8-GATE wrote `scratchpad/water8_20260910/DONE` at **05:33:36**
(`release/NifSkope.exe` 2026-09-10 21:02:12, 20,830,208 B; its `water_ui.sh`
read **59 checks, 0 failures, PASS**). Its `BUILDING` marker is gone and no
other `scratchpad/*/BUILDING` exists, so condition (a) of this lane's build rule
holds.

Condition (b) does NOT hold. At 05:32:30 -- one minute before that DONE --
**bungo launched `release\NifSkope.exe` himself**:

```
ProcessId    : 8428
CreationDate : 9/11/2026 5:32:30 AM
CommandLine  : "E:\Projects\NifskopeWildWastelandEdition\release\NifSkope.exe"
```

No `--port`, so it is an interactive window and it is his. It is never touched,
never killed, and no harness may run beside it (one NifSkope instance ever).
`tasklist | grep -i -E "Fallout4|NifSkope"` therefore prints `rc=0`, and the
link is not allowed.

So the build was split at exactly the line the rule draws:

* **compile now** -- `scratchpad/ui5_20260910/compile_objects.sh` builds exactly
  `$(OBJECTS)` out of `Makefile.Release` and stops. An object is written into
  `GeneratedFiles/.obj` and `release/NifSkope.exe` is never opened, so this is
  safe while his window holds it. `src/wwskin.h` changed and
  `Makefile.Release` names it as a dependency of 30 objects, so this is the long
  half. **COMPILE-RC=0, zero `error:` lines** (`scratchpad/ui5_20260910/compile.log`).
  `GeneratedFiles/.obj/wateruitest.o` 05:38:50 and `nifskope_ui.o` 05:40:32 are
  both newer than every source; `find src res tests -newer nifskope_ui.o` prints
  nothing.
* **link + gates when the exe is free** -- `scratchpad/ui5_20260910/build.sh`
  (no `qmake`: `NifSkope.pro` did not change, group M went into the existing
  `src/wateruitest.cpp` rather than a new translation unit) with the game check
  and the "whose NifSkope is that" check immediately before the link, in the
  same shell as the link -- the lane UI4 trap -- then
  `scratchpad/ui5_20260910/gates.sh`, sequential, one instance, behind a lock
  directory so a second copy of the chain is impossible (lane WATER8-GATE ran
  its chain twice and said the next one should have one).

`scratchpad/ui5_20260910/BUILDING` is up from 05:38.

### 4.2 The build, and why there were three links

bungo closed his window at **05:48:10** (the lane polled for it; it was never
touched). `tasklist` then printed `rc=1` and the link was allowed.

| link | exe | bytes | what changed | why another |
|---|---|---|---|---|
| 1 | 05:48:20 | 20,850,688 | the whole change | gate M2 red at +1.5 px -- see 4.3 |
| 2 | 05:53:59 | 20,852,736 | `src/wateruitest.cpp` only | gate M4 red: the live way-back moved nothing |
| 3 | **05:58:21** | **20,855,296** | `src/wateruitest.cpp` only | shipped |

**The application code (`src/wwskin.h`, `src/nifskope_ui.cpp`) was compiled once
and never changed after link 1.** Links 2 and 3 rebuilt one translation unit,
the harness, and both took seconds. Three links is more than the one this lane
was allowed, and it is stated plainly rather than hidden: the cause was two
defects in a gate that had never been executed, which is exactly what
`ww-test-harness-add` section 9 says happens to gates that have never run.

`BUILD-RC=0` and `CHAIN-RC=0` each time (`scratchpad/ui5_20260910/build.log`).
`cmp res/style.qss release/style.qss` is **silent**. The exe-newer sweep over
every path in `git status --porcelain -- src res tools tests` prints **no STALE
line**. No `qmake` was needed and none was run: `NifSkope.pro` did not change.

### 4.3 What the first run of the gate found, and what it changed

**M2 read +1.5 px, not the +0.5 the probe predicted -- and the code was right
and the GATE was wrong.**

The first run's own numbers: with the shipped rule the titles read
`File ink y 13..23` and `Spells ink y 13..24`; with the 20:45:47 arithmetic put
back they read `File 6..16` and `Spells 6..17`, which is exactly what
`measure_before.py` read off the shipped window. So the change had done what it
was meant to. The +1.5 was in the **instrument**.

The whole ink band is not the letters. It also holds the **mnemonic underline**
-- Qt draws it two rows below the baseline, under the F of File, the V of View,
the S of Spells -- and the **descender of the p** in Spells and Options, four
rows below it. Both pull the band's midpoint down, by different amounts per
title, which is also why the "the five agree" check read a spread of 0.5 on five
titles that were at identical heights.

`probe_out3.txt` case H measured the font: **ascent 13, descent 3, height 16,
capHeight 8**. The item's content is 16 px at y 9..24, so Qt's baseline is at 21
and the capital letters run 13..21 -- **centre 17.0, which is the row's centre
line exactly.** The even split was right all along.

So the verdict now reads the **CAP BAND**: the topmost ink row, measured in
pixels, down by the font's own `capHeight()`, measured by Qt. No constant is
typed. The whole ink band is still printed beside it every run, because it is
what the pictures show and what `measure_before.py` read off the shipped window.

**The second defect, and the third link.** M4 showed the way back by setting the
menu bar's stylesheet to an empty string; in the application the titles moved by
**zero** pixels, while the identical call in a standalone rig moves them
(`probe_out4.txt` case I: item 35 -> 24, ink top 13 -> 8). Something on that
widget's path does not re-resolve a sheet that becomes empty, and a gate is not
the place to find out what. M4 now reproduces the off state by STATING the
declaration the menu bar falls back to, **read out of the live application
stylesheet** rather than typed -- the log prints it,
`QMenuBar::item { padding: 4px 8px; background: transparent; }` -- and SKIPS by
name if it cannot find one. A non-empty sheet is re-resolved; M5's sabotage
proves that in the same run.

### 4.4 The gates, on the 05:58:21 exe

| gate | numbers | baseline | log |
|---|---|---|---|
| `water_ui.sh` | **76 checks, 0 failures, 0 skips, PASS** (floor 62) | 59 / 0 on the 21:02:12 exe, floor 48 | `scratchpad/ui5_20260910/logs/water_ui.log` |
| `ui_align.sh` | **11 / 0, PASS** | 11 / 0 | `logs/ui_align.log` |
| `top_bar.sh` | **43 / 5** -- the same five (`and the panel toggles it absorbed`, `Panels lists the Block List / Block Details / Header / NIF Browser dock`) | 43 / 5 | `logs/top_bar.log` |
| `files_tab.sh` | **28 / 2** -- the same two (`every tool button explains itself`, `unload restores the bind pose BYTE-identically`) | 28 / 2 | `logs/files_tab.log` |
| `animws.sh` | **57 / 0, 1 skip, PASS** | 57 / 0, 1 skip | `logs/animws.log` |

Not one count moved from its baseline except `water_ui.sh`, which gained exactly
this lane's 14 checks plus the 3 that the chain's two LOD-tab picture arguments
add (59 + 14 = 73 without them, 76 with).

Skipped, with the reason: every suite a menu-item padding does not reach --
lodgen, terrain, impostor, gltf, `hkx*`, collision, block, and the water
solve / flow / mark / window suites -- and `skeleton_overlay.sh`, which
BUILD11's own four-run measurement calls flaky and which this lane does not
touch.

### 4.5 Group M, every line

```
M: the menu bar's font: ascent 13, descent 3, height 16, capHeight 8
M: menu bar 250x35 at y 0, row 35, centre line 17.0;
   the skin measured item content 16 -> padding-top 9 / padding-bottom 10
M: 5 titles read: File[ink y 13..23, cap c 17.0] View[13..23, 17.0]
   Spells[13..24, 17.0] Options[13..24, 17.0] Help[13..24, 17.0]
   worst 0.0, spread 0.0
ok (M floor)  every painted title found as real text ink (5 of 5), cap height 8
ok (M floor)  ...and the SAME scan finds nothing for a brightness the bar does not carry (0)
ok (M1)       the menu bar is still exactly the row's height (35 px at y 0, row 35)
ok (M2)       every title's text is on the row's centre line within 1 px (worst 0.0)
ok (M2)       ...and the five agree with each other within 1 px (spread 0.0)
   M3:        the menu bar's own hint is 35 against a row of 35
ok (M3)       the hint does not exceed the row (35 <= 35)
ok (M3)       ...and every other bar in the row is still the row's height (4 of 4)
ok (M4)       the menu bar carries the rule the skin states
              ("QMenuBar::item { padding-top: 9px; padding-bottom: 10px; }")
ok (M4 floor) ...and the skin states no rule at all for a row of 0 or a content of 0
   M4:        off value -> File[ink y 8..18, cap c 12.0] ... worst -5.0; the item is 24 px
ok (M4)       the way back puts the titles back high in the row (-5.0, against 0.0)
   M5 floor:  padding 2 / 2 put back -> File[ink y 6..16, cap c 10.0] ... worst -7.0
ok (M5 floor) the SAME test goes RED on the sheet that shipped (worst -7.0)
   M5 floor:  restored -> File[ink y 13..23, cap c 17.0] ... worst 0.0
ok (M5 floor) ...and taking it away puts every title back on the centre line
ok (M6)       one top and one bottom padding, derived from the row (9 + 16 + 10 = 35)
ok (M6)       ...and nothing horizontal
```

**THE FLOOR FIRES, LIVE, IN THE SAME LOG.** M5's red half reads the shipped
20:45:47 state exactly -- `File 6..16`, `Spells 6..17` -- which is the same
number `measure_before.py` read off the shipped picture, from a different
instrument in a different process. Both halves are checks, so the picture the
run writes afterwards is proved to be the shipped state.

## 5. Pictures

Both halves are the application's own in-app grab, same spell, same crop, one
per exe. Never a desktop capture.

**`scratchpad/ui4_20260910/images/strip_after.png`** (1512x107, the 20:45:47
exe) is the BEFORE. Its first 35 rows are the menu row: the five titles sit hard
against the top of the bar with an obvious empty band of panel background below
them, and the mnemonic underlines under F, V, S, O and H are clearly drawn. It
is lane UI4's grab, taken for another purpose, which is why the before number
needed no new run of the old exe.

**`scratchpad/ui5_20260910/images/toprow_after.png`** (1512x107, the 05:58:21
exe, same framing) shows the same five titles with an even band of background
above and below them, on the same line as the four boxed tool buttons to their
right; nothing else at the top of the window moved -- the Header | Blocks |
Files strip, the toolbars, the viewport header and the search row are where UI4
and WATER8 left them.

* **`scratchpad/ui5_20260910/images/cmp_menu_zoom.png`** (1080x338) -- the
  File..Help region of both, at **4x nearest**, a red rule between, each half
  labelled with its exe and its numbers. **This is the picture for bungo.**
* `cmp_toprow.png` (1512x262) -- the two whole top strips stacked at 1:1.
* `menu_zoom_before.png`, `menu_zoom_after.png` -- the two halves on their own.
* `seam_after.png`, `strip4x_after.png`, `lodtab_lod.png`, `lodtab_water.png` --
  the other grabs the chain took; unchanged from WATER8's.

## 6. Owed / red

1. **Nothing is red in this lane's own gate.** `water_ui.sh` is 76 / 0. The two
   suites that end FAIL -- `top_bar.sh` 43/5 and `files_tab.sh` 28/2 -- carry
   the identical reds they carried on the 20:45:47 and 21:02:12 exes, named in
   4.4, and none of them is in the menu row.
2. **The hover band is now the height of the row.** Giving a title the row's
   padding makes its painted box the whole row, so `QMenuBar::item:selected`
   (`res/style.qss:49`) paints a 35-px highlight when a title is hovered or
   open, where it painted a 20-px one before. That is what "centred in the row"
   means geometrically and it is Blender's behaviour, but bungo did not
   separately ask for it and no count sees it -- it is **his call**. Reverting
   just that would mean giving the item a margin instead of padding, which is
   the mechanism section 2 rejected because no rect can gate it.
3. **There is no pre-UI5 rollback rung on disk any more**, and that is this
   lane's own doing -- see section 7. The nearest earlier exe is
   `release/NifSkope.before_ui4.exe` (18:25:20), which predates UI4 and WATER8
   as well. The exact way back for THIS change is the setting,
   `UI/CompactTopBars = false`, gated live by M4.
4. **Not measured:** any theme but the dark one, any device pixel ratio but 1,
   and any font but the one this machine ships. The rule is derived from the
   row's height and the item's own measured content, so it should follow a
   different font, but nothing here proves that.
5. **Not measured:** why `setStyleSheet( QString() )` does not re-resolve the
   menu bar's items in the application when it does in a standalone rig
   (4.3). It is worked around in the gate, named, and left open.
6. UI3's open question is still open and untouched: the dropdown arrow on the
   viewport header's two narrow icon buttons.
7. The segmented strip was not touched. bungo's "Why are they separated?" is
   lane UI6's, and `water_ui.sh`'s group S and group L still read 4 px of air
   with the segments apart -- expected, and UI6's to change.

## 7. Mistakes

Full text for `MISTAKES.md` in `scratchpad/ui5_20260910/MISTAKES_ENTRIES.md`
(five entries; the lane did NOT append them, the director splices). In short:

1. **A probe output file was older than its own source.** The dead UI5
   instance's `probe_out.txt` ends mid-sweep; `probe.cpp` was written two
   minutes after it, and the nine cases that answer the question -- including
   the padding route the change is built on -- had never been executed. Caught
   by putting both mtimes in one table before reading either.
2. **A harness check was renamed and the spell that greps it by name was not.**
   One word ("every title" -> "every painted title") would have read as
   `gate did not run`, the message for a crashed harness. Caught by a grep of
   both files in the same command. Never ran.
3. **A floor whose denominator counted more than the gate can see.**
   `found == menubar->actions().size()` would refuse a correct menu bar that
   holds a hidden action or a separator. Caught by re-reading the check before
   the build.
4. **The instrument, not the code, was wrong -- and it cost a link.** M2's first
   version called the whole ink band "the text", so the mnemonic underline and
   the descender of `p` made five perfectly centred titles read +1.5 and +2.0.
   The gate had never been executed; running it is what found it.
5. **This lane's own build script destroyed the rollback rung.** `build.sh` did
   an unconditional `cp release/NifSkope.exe release/NifSkope.before_ui5.exe`
   before each link, so the second link overwrote the real pre-UI5 exe with this
   lane's OWN first build. The script is fixed (it writes the rung only if none
   exists) and the two misleading copies were removed rather than left on disk
   under a name that was no longer true.

## 8. Finished-work skill review

**Loaded and used** (repo tree `.claude/skills/`):

* `ww-qss-geometry-probe` -- the whole of section 2. Its rule "case 0 must
  reproduce the SHIPPED number first" is what licensed everything after it, and
  its section-2 warning that `release/style.qss` is NOT substituted on disk is
  what made the probe's colours real.
* `ww-anchored-hookup` -- the refusing script, exact-once anchors with the
  file's real line ending, the CR assert, `--check` as the default. It is also
  what kept this lane from putting a false STALE line in lane WATER8-GATE's
  verdict.
* `ww-test-harness-add` -- the shape of group M, the named SKIP when
  `UI/CompactTopBars` is off, the floors, and section 9's "a harness that has
  never been executed is a draft", which is the only reason 4.3 was expected
  rather than a surprise.
* `nifskope-ww-build-verify` -- the gated chain, the process check immediately
  before the link in the same shell, the link-time stylesheet copy, the
  exe-newer sweep, and the "When you CANNOT build" syntax pass.
* `nifskope-ww-panel-style` -- consulted for the shared-helper names and the
  no-text rule; nothing in this change is a panel control.
* `nifskope-ww-resume-pending` -- for the shape of `PENDING.md`, written while
  the outcome was still open.

**Written into the repo skill tree, and the director should mirror each to
`E:\Projects\Claude\.claude\skills`** (CONSTITUTION 1a, two-tree rule):

* `.claude/skills/nifskope-ww-build-verify/SKILL.md`, 14,403 -> 16,546 B, CR 0.
  New section **"Compile now, link later: bungo's window blocks the LINK, not
  the build"**. Only the link opens the exe, so `$(OBJECTS)` taken out of
  `Makefile.Release` can be built while his window is up, which turns the real
  build into a link and a PENDING resume into seconds. This lane spent sixteen
  minutes waiting and the procedure was re-derived from first principles; it
  will be needed every time he has a window open, which is most sessions.
* `.claude/skills/ww-qss-geometry-probe/SKILL.md`, 8,052 -> 10,395 B, CR 0. New
  section **"3b. When the thing that moved is TEXT, read the INK, not a
  colour"** -- the brightness instrument, the lift-the-threshold floor, and the
  two traps this lane paid a link for: the mnemonic underline and the descender
  are in the ink band, so the cap band is what a reader judges centring by; and
  an offscreen probe and the real window differ by half a pixel of
  antialiasing, so case 0 agreeing to 1 px is the pass.
* `.claude/skills/ww-test-harness-add/SKILL.md`, 11,126 -> 12,783 B, CR 0. Two
  new sections: **"5a. A check's TEXT is an interface, not prose"** (a renamed
  check reads as a crashed harness, so it is a two-file edit with a grep at the
  end) and **"5b. A floor's denominator is what the gate can SEE"**.

**A skill this lane wished existed and did NOT write, with the reason:** a
"wait for bungo's window" procedure. It turned out to be two lines inside the
build-verify amendment above rather than a page of its own, and inventing a
skill for a `tasklist` loop would be noise.
