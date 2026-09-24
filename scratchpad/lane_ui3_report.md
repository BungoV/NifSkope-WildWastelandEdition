# Lane UI3 -- the row's buttons ARE the row, and R3 at 1 px

Repo `E:\Projects\NifskopeWildWastelandEdition`, main, nothing committed.
Written incrementally (CONSTITUTION 1).

bungo's ruling, verbatim: *"compact these vertically like this, the top bar and
the buttons"*. BUILD12 measured the aligned row at **35 px** and the buttons
inside it at **39**, and gate R3 of `tests/spells/water_ui.sh` passed that on an
**8 px** tolerance while its own header promised 1.

---

## 1. Why 39 won -- measured, not read

The answer was got OUTSIDE the application, so the one build of the session
could be spent on a fix that was already known to work. Two standalone probes
link `Qt6Widgets` only and never touch `release/NifSkope.exe`:

* `scratchpad/ui3_20260910/probe.cpp` -- a `QToolBar` named `tView` at icon size
  16x16 with min/max height pinned to 35, four `QToolButton`s in
  `ToolButtonTextBesideIcon` with `InstantPopup` menus and their own stylesheet
  `wwBoxedButtonQss( "3px 6px" )`, `release/style.qss` as the application sheet,
  and the bar-row sheet appended exactly as `wwAlignBarRow` appends it.
* `scratchpad/ui3_20260910/probe2.cpp` -- the same rig, sweeping the sheet's
  content height 18..40 with and without the bar's own box.

**It reproduced 39 exactly** (`scratchpad/ui3_20260910/probe_offscreen.txt`,
case 1: `height=39 sizeHint=42`), so the mechanism is the real one and not a
story. Three things made it, and none of them was the row height:

| # | cause | file:line |
|---|---|---|
| 1 | **A widget's OWN stylesheet outranks every ancestor's**, whatever the selectors say. All four buttons carry one -- `wwBoxedButtonQss`, whose `padding: 3px 6px` therefore beat the row's padding before the row ever saw the button. A sheet on the BAR can only reach a property the button's own sheet does not mention, which is why WATER7's `min-height` arrived and its `padding` did not. | `src/nifskope_ui.cpp:520` (the sheet), `:27540` (Workspaces), `:26929` (LOD / Animation / Collision) |
| 2 | **QSS `min-height` on a QToolButton is a minimum on the CONTENTS**, not on the widget: Qt expands the contents to it, adds 3 px of its own for `CT_ToolButton`, then the padding and the border. `31 + 3 + (3+3) + (1+1) = 42` asked for, 39 laid out. **`max-height` is not consulted on this path at all** -- probe case 3 set it and nothing moved. | the sheet that produced it: `wwBarRowButtonQss`, `src/nifskope_ui.cpp:631` as it stood at BUILD12 (`min-height: rowHeight - 4`, `padding: (rowHeight - 18) / 2`) |
| 3 | **The bar's own layout margin.** `PM_ToolBarItemMargin 2 + PM_ToolBarFrameWidth 2 = 4`, measured by probe2, so a QToolBar starts its items 4 px below its own top -- even a button of exactly the row's height would hang 4 px past the bottom of a bar of exactly the row's height and be clipped there (`y 4` in every row of the shipped sweep). | `res/style.qss:45` `QToolBar { border: 1px solid transparent; }` is where the frame width comes from |

So BUILD12's number was not one error but three, and only the second of them
had anything to do with the arithmetic that was changed.

## 2. The fix, through the skin

`wwBarRowButtonQss` no longer takes the ROW; it takes what is left of the row
once the style's own additions come out, and `wwAlignBarRow` **measures** that
instead of typing Qt's 3:

* `wwBarRowBoxQss()` (new) emits `QToolBar { border: 0px; padding: 0px;
  margin: 0px; }` for the bars. That puts their items at **y 0** with the whole
  row to sit in, and -- the part that makes the whole thing forgiving -- once
  the margin is gone the layout **clamps** a button to the bar instead of
  letting it overflow. probe2 variant 1: content **26..29 all measure 35 px at
  y 0**. A basin, not a knife edge.
* `wwBarRowButtonQss( contentHeight )` states the glyph line and the air above
  and below it (`min-height`, `padding-top`, `padding-bottom`) and nothing
  horizontal, so the row has one vertical box and every button keeps its width.
* `wwAlignBarRow` appends both to the bars **and to every tool button in them**
  -- cause (1) means there is no other way to reach a button that carries its
  own sheet -- and calibrates: it asks for a content height certainly taller
  than any glyph line in the row (the row itself), reads back what the style
  added, and takes that much out of the row. `wwBarRowButtonContent()` and
  `wwBarRowButtonOverhead()` read the two numbers back for the gate and the log.
* No `setFixedHeight` anywhere, and the way back is unchanged and still exact:
  `UI/CompactTopBars = false` appends nothing and touches no widget's own sheet.

Files: `src/wwskin.h` (mine), `src/nifskope_ui.cpp` through the refusing
anchored script `scratchpad/ui3_20260910/hookup.py` (one `replace` edit over one
contiguous region, `--check` first: *1 of 1 anchors matches once, CR 0*).
`res/style.qss` was NOT touched -- the fix did not need it, and its
`QToolBar { border: 1px }` still serves every toolbar that is not in the row.

## 3. The gate

`release/NifSkope.exe` **18:25:20, 20,773,888 bytes** (BUILD12's was 17:45:29,
20,751,360). Two builds, both `BUILD-RC=0`; the second is the menu-arrow round
in section 4. Exe proven newer than all four changed files; `res/style.qss`
and `release/style.qss` compare equal; no stale object -- the only `.o` older
than `src/wwskin.h` is `colorwheel.o`, and `colorwheel.cpp` only MENTIONS
`wwskin.h` in a comment, it does not include it and `Makefile.Release` does not
list it. No `.pro` change, so no qmake.

**THE BUTTON NUMBER: 39 px at y 4 -> 35 px at y 0, on all four, the row is 35.**

```
R3 button tFile/ViewWorkspacesButton: 35 px, y 0 (row 35)
R3 button tView/ViewLodButton:        35 px, y 0 (row 35)
R3 button tView/ViewAnimationButton:  35 px, y 0 (row 35)
R3 button tView/ViewCollisionButton:  35 px, y 0 (row 35)
R3: 4 bar buttons, 4 at the row height, heights 35..35, row 35, worst offset 0
R3: the skin calibrated content 26 with a measured style overhead of 9
```

**The floor fires, in the same run, on real widgets.** BUILD12's own arithmetic
(`min-height: 31`, `padding: 8`) is appended live over the shipped sheet and the
SAME predicate is asked again:

```
R3 floor: with BUILD12's own arithmetic put back (min-height 31, padding 8)
          the buttons read 49..49 against a row of 35, worst offset 14
  ok  (R3 floor) the SAME test goes red ... (0 of 4 buttons at the row height)
R3 floor: restored, the buttons read 35..35 again
  ok  (R3 floor) ...and taking it away puts every button back on the row (4 of 4)
```

49 rather than 39 because the bar's box is gone by then, so nothing clips the
overshoot any more -- which is itself the third cause, seen from the other side.
The restore half is what makes the picture below the shipped state.

| gate | UI3, 18:25:20 exe | BUILD12 baseline |
|---|---|---|
| `water_ui.sh` | **37 checks, 0 failures, 0 skips, PASS** (floor 30) | 30 / 0 (floor 24) |
| `ui_align.sh` | **11 / 0, PASS** | 11 / 0 |
| `top_bar.sh` | **43 / 5** -- the same five (`Panels lists the ... dock`) | 43 / 5 |
| `files_tab.sh` | **28 / 2** -- the same two (Qt's own `QLineEditIconButton` clear buttons carry no tooltip) | 28 / 2 |
| `animws.sh` | **57 / 0, 1 skip, PASS** | 57 / 0 |

Nothing moved except `water_ui.sh`, which gained seven checks and lost its 8 px
tolerance. Suites the change does not reach were not run.

## 4. Pictures -- and the second build they cost

Both through the application's own grab (`water_ui.sh`'s `SHOT=`), same spell,
same crop, two exes. Never a desktop capture.

* `scratchpad/ui3_20260910/images/buttons_before.png` (1512x107) -- the 17:45:29
  exe. The top row reads File / View / Spells / Options / Help, then the boxed
  **Workspaces** button, then LOD 0 / Animation / Collision. The Workspaces box
  is **cut off along the bottom of the row**: its rounded border simply stops,
  because the button is 39 px starting 4 px down inside a 35 px bar and the
  bottom 8 px are clipped away.
* `scratchpad/ui3_20260910/images/buttons_after.png` (1512x107) -- the 18:25:20
  exe, same framing. Every box in the row is whole and ends on the row's own
  bottom edge, the four labels sit on one line with the menu titles, and the
  strip below (Header | Blocks | Files) meets it without a step.
* `scratchpad/ui3_20260910/images/cmp_toprow.png` -- the two stacked, 1.8x, red
  rule between them: the same seven labels and four buttons, the lower half a
  complete box where the upper half is a clipped one.
* `scratchpad/ui3_20260910/images/cmp_header.png` and `cmp_zoom.png` -- the
  SECOND ROW (the viewport header), which is what cost a second build.

**The second build, and one thing bungo should look at.** The first build was
green at 1 px and the picture showed what the count could not: the viewport
header's **Global**, snap and grid buttons had grown a dropdown arrow in their
bottom-right corner. A QToolButton's default `menu-indicator` lives there;
`wwBoxedButtonQss` has always centred it, which is why the four buttons the
ruling is about never showed it. The row's sheet now states the same rule, so
the arrow stays beside the glyph whatever sheet a button carries, and R4 pins
it.

Those three arrows are **newly VISIBLE**, and that is not a new arrow: those
buttons always had a menu, and their indicator was in the 8 px that the old
39-px-in-a-35-px-bar clipped off. `cmp_zoom.png` (4x) shows it -- before, no
arrow; after, a small arrow at the right of each. On **Global** it sits cleanly
beside the label; on the two narrow ICON-ONLY buttons it touches the icon,
because those buttons carry `padding: 1px 1px` (`res/style.qss:205`) and have
no room to the right. It is honest (they are dropdowns) and uniform with every
other menu button in the row, but it is a look bungo did not ask for, so it is
his call: leave it, give the row's indicator a little right margin (one line in
`wwBarRowButtonQss`, one build), or turn the whole row off with
`UI/CompactTopBars = false`.

## 5. Mistakes

Three, written in full in `scratchpad/ui3_20260910/MISTAKES_ENTRIES.md` and
appended to `MISTAKES.md` by this lane:

1. **A Bash heredoc ate the backslashes the loaded skill had already warned
   about.** The spell's line continuations counted 0 through
   `python - <<'PYEOF'`; `ww-anchored-hookup` 3a names that exact trap, and it
   had been read twelve minutes earlier. Fixed by writing
   `scratchpad/ui3_20260910/patch_spell.py` with the Write tool.
2. **`hookup.py`'s own marker assertion was arithmetic about an uncounted
   number** (`n + 1` where the replacement names the marker three times). It
   refused twice before it was derived from the two texts instead. No damage --
   the script refused before writing, which is what it is for.
3. **The first build moved the buttons and forgot what hangs off them** -- the
   menu indicator, section 4. Caught by the picture, not the gate; cost a
   second build.

## 6. Finished-work skill review

**Loaded and used:** `nifskope-ww-panel-style` (its 2026-09-10 "bars that meet
along one line" section is the law this lane is amending, and its rule "the rule
lives ONCE in the shared skin helpers, never as setFixedHeight at three call
sites" is what kept the fix inside `wwAlignBarRow`); `nifskope-ww-build-verify`
(the gated chain, the exe-newer sweep over EVERY changed file, the
`cmp res/style.qss release/style.qss` step, the `sx_$LANE.sh` naming rule, the
`-DWW_HKXANIM_UI` defines that a bare syntax pass is missing);
`ww-anchored-hookup` (the one refusing edit to `src/nifskope_ui.cpp`).

**Named and declined, with the reason:** `ww-test-harness-add` -- the harness
and its spell already exist and this lane tightened checks inside them rather
than adding a `WW_*_TEST`; its rules on floors that cannot fire were followed
anyway, which is what the live sabotage floor is. `nifskope-ww-render-shot` --
the deliverable is a LAYOUT picture, and CONSTITUTION 5 gives that to the
in-application dock grab (`SHOT=`), which is also what makes the before and the
after the same crop from the same spell; the render hook photographs geometry.
`nifskope-ww-resume-pending` -- the game was down and no marker was up, so the
lane built and there is no PENDING to resume.

**Written, because it should have existed and would have saved BUILD12's four
pixels:** `ww-qss-geometry-probe` --
`E:\Projects\Claude\.claude\skills\ww-qss-geometry-probe\SKILL.md`. Answering
"what will this stylesheet actually do to that widget's height" by BUILDING A
40-LINE Qt PROGRAM AGAINST `release/style.qss` instead of reasoning about Qt's
box model, in about two minutes and without the build slot. It is the whole
reason this lane knew the fix worked before the one build: the probe reproduced
39 exactly, then found that `max-height` is ignored on that path, that the
number wanted was 26 and not 29, and that the bar's own 4 px margin was a third
cause nobody had named. **The director must mirror it to the repo tree**
(`.claude/skills/`), per CONSTITUTION 1a's two-tree rule.

**Declined as one-offs:** the `--revert` flag on `hookup.py` (a lane that has to
apply its own edit twice is a lane that got the edit wrong the first time; the
flag is three lines and the fix is not to need it).
