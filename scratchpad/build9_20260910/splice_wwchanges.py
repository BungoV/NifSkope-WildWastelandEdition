#!/usr/bin/env python3
"""Lane BUILD9, 2026-09-10 -- splice this round's changelog entry.

WW_CHANGES.md is MIXED (CR 19,020 as of 2026-09-09) and stays so. Never sed -i
it. This inserts ONE new top-level entry immediately after the title, carrying
lane BUILD9's own text and, verbatim, the three pending lanes' delivered entry
files with their heading level normalised to ### and a measured status block
appended to each. The lanes' own words are not rewritten (CONSTITUTION 8).

Default is --check (writes nothing). --apply writes.
"""
import sys

PATH = "WW_CHANGES.md"
TITLE = "# NifSkope — Wild Wasteland Edition: Change Log\n"

LANE_FILES = [
    ("scratchpad/filestab_20260910/WW_CHANGES_ENTRY.md", """
**BUILT AND GATED (lane BUILD9, 2026-09-10, exe 15:52:46).** `hookup.py --check`
71 edits, every anchor matched as declared; `--apply` moved the five files by
exactly the predicted bytes (`src/nifskope.cpp` CR 9,379 -> 9,520, the +141 CRLF
lines it inserts; the other four LF-only at CR 0). qmake RC=0, make RC=0,
`src/filestab.h` named by `filestab.o`, `filestabtest.o`, `nifskope.o` and
`nifskope_ui.o`, exe newer than every changed path, sheet in step.

`tests/spells/files_tab.sh`: **29 checks, 2 failures.** (1) 0 "NIF" strings in
the page, seeded offender found then gone. (2) `.nif 1 | .bto 1 | .btr 1 |
.hkx 14939 | .gltf 0 | .lodl 0 | .lodt 0`, first clip
`meshes/actors/_testcharacter/behaviors/_testcharacter.hkx`. (3) 78 play / 17
named with no node / 4 by case, clip count +1, became the playing sequence.
(6) refuses in words with 0 named nodes and loads nothing. The dock grab is
`scratchpad/filestab_20260910/dock_after.png`, beside `dock_before.png` taken
from the old exe.

**The two red gates, with their causes measured, NOT amended to green.**
(5) counts 6 tool buttons on the page of which 2 have no tooltip, and the two
are `QLineEditIconButton` -- the clear buttons Qt creates inside a `QLineEdit`
with `setClearButtonEnabled(true)`, one per search field, not controls the lane
put there. Narrowing a gate's population after seeing its numbers is what
CONSTITUTION 1 forbids, so it stays red and the decision is bungo's.
(4) reports one node of 139 not restored after unload, and the node is
`PipboyBone`: `NifSkope -no-gui list` on the fixture shows `[72] NiNode
'PipboyBone'` followed by `[73] NiTransformController`, so the fixture's own
animation drives it. The gate snapshots the bind pose at t=0, scrubs to t=mid,
unloads and compares WITHOUT stepping back, and the scene is still at the
clip's time. Corroborated twice: `hkxanim_play.sh`'s restore gate is 27/0 on
`skeleton.nif`, which has no such controller, and lane HKX3's own gate (d),
which DOES step after unloading, reads 0 of 139 on this same fixture. **The
product question for bungo: unloading a clip leaves the scene at the clip's
time, so a model with its own animation stays where the transport left it.**

Two instrument defects were repaired rather than worked around: the driver
built its loose fixture tree with `mktemp -d`, whose `/tmp/...` path `winpath()`
cannot convert, so the Windows binary was handed a string it could not open and
the first run's census read `.nif 0 | .bto 0 | .btr 0`; and the panel-style
floor blanked `tools.first()`, which was already untipped, so it could not
fire. Six expectations in `WW_LOADEDNIFS_TEST` still quoted the renamed labels
and were updated (literals only, no assertion touched): that harness went 9
failures -> 3, which is the same three it had before the renames.
"""),
    ("scratchpad/hkx3_20260910/WW_CHANGES_ENTRY.md", """
**BUILT AND GATED (lane BUILD9, 2026-09-10, exe 15:52:46).** Nine anchors, each
still matching once after FILESTAB's 71 edits to the same two files; `--apply`
moved `NifSkope.pro` 19,044 -> 19,350 and `src/nifskope_ui.cpp` 1,476,506 ->
1,478,429, CR 0 -> 0 on both. `hkxanimui.h` is named by four objects,
`moc_hkxanimui.cpp` exists, `WW_HKXANIM_UI` is in `Makefile.Release`.

`tests/spells/hkxanim_ui.sh`: **48 checks, 1 failure, 0 skips.** (a) one new
row, 93 frames, 60 fps, 1.53333 s. (b) 78 bound nodes at frame 46, worst
translation 0, worst rotation 1.72665e-05 deg; the frame-0 floor goes red at
299.83 / 104.964 deg. (c) `frame 46 / 92 - 0.767 s`, speed 1 -> 2 -> 1, the
Loop row flips, the clip registers as a looping cycle. (d) 78 of 139 nodes
posed, 0 of 139 differ after unload. (e) drag-enter and drop accepted, same
93/60, and the junk floor adds no row. (f) `skeleton.hkx` lands marked refused
with the reason in words. (g) 0 unstamped number fields of 3, 0 group boxes,
0 unstyled selectors of 1, 6 of 6 controls with tooltips, the summary line and
the list outside the splitter. Picture:
`scratchpad/hkx3_20260910/dock_clip_midclip.png`, 1549x284, the clip loaded and
the playhead at 0.767 s. `hkxanim_play.sh` still 27 checks / 0 failures, so
HKX2's playback underneath is untouched.

**The one red is an instrument that cannot fire here, and it was measured
rather than guessed.** Gate (g)'s floor asks the wheel to step the Speed field
once it has focus. The harness now prints the focus state beside it:
`hasFocus no, focusWidget <none>, window active no`. A WW harness window is
never activated by design (`WW_WINDOW_AT`, no `raise()`), and
`QWidget::hasFocus()` is false in an inactive window however many times
`setFocus()` is called -- so `wwGuardWheel`, which blocks the wheel exactly
while `!hasFocus()`, is behaving as specified and the floor has no way to
exercise the other half. The guard's unfocused half passes.

**A build trap this lane's hook-up walked into, now in MISTAKES.md.** Edit 3
adds `DEFINES += WW_HKXANIM_UI`, and `make` compares mtimes, not flags:
`timeline.o` was newer than its source and was kept, compiled WITHOUT the
define, while the fresh `nifskope_ui.o` called a method that had compiled out.
The link failed on `TimelineWidget::setGLView(GLView*)` and deleted the exe.
Six objects had to be deleted by hand before the rebuild.
"""),
    ("scratchpad/skeloverlay_20260910/WW_CHANGES_ENTRY.md", """
**BUILT AND GATED (lane BUILD9, 2026-09-10, exe 15:52:46).** Five anchors each
matching once, `NifSkope.pro` +27 bytes and `src/nifskope_ui.cpp` +1,901 as
predicted, four `lane SKELOVERLAY` markers, CR 0 -> 0 on both.

`tests/spells/skeleton_overlay.sh`: **17 checks, 0 failures, PASS.** On
`fixtures/human_male_vanilla.nif` the dock reports All 130, Bones 93, Deforming
93, Unused 0, and the overlay's census matches it exactly: 130 joints,
129 segments, 78 stubs, 0 missing nodes, muted 37 = 130 - 93. The OFF floor
reads all zeros. The overlay changes 14,390 pixels (2.412% of the frame) and
**0 of them lie outside the mask rasterised from the segments it reports having
drawn**, with the mask covering 12.23% of the frame -- so the mask cannot have
passed by covering everything. Toggling off restores the off-render byte for
byte (0 pixels differ). With the clip at frame 46, every drawn joint is the
ANIMATED node's world position (worst 0 units), and the bind-pose floor
disagrees on 122 joints, largest 367.6 units.

Pictures, through the render hook on one pinned orthographic camera:
`scratchpad/skeloverlay_20260910/off.png`, `on.png`, `on_frame46.png`, three
distinct images; the gate's own evidence, including `gates/gate_mask.png` with
the mask in grey and every changed pixel in orange, is under `gates/`.

**The pose-armature factoring is not proven behaviour-identical by a run, and
the two neighbouring harnesses do not settle it.** `WW_SKELETON_TEST` PASSES
(129 bones drawn on the vanilla skeleton). `WW_POSEDRAW_TEST` FAILS at
"clicking a bone did not make it the active object", on BOTH fixtures available
here -- `human_male_vanilla.nif` (130 bones) and the vanilla `skeleton.nif`
(129) -- with `poseBoneAt` resolving block 0 at a bone's own drawn position in
each. The factoring is arithmetically identical by diff (`characteristicBoneSize`
is `refreshPoseBoneSize`'s body with the list parameterised and -1 standing in
for the early return; `boneTailIn` is `poseBoneTail`'s with the list and the cap
parameterised), and this harness was last recorded green on a FACIAL rig of 70
bones, not on either of these. Reported, not cured.
"""),
]

BUILD9 = """## 2026-09-10 -- three pending lanes built and gated, and the bars given one row (lane BUILD9)

Lanes FILESTAB, HKX3 and SKELOVERLAY each ended BUILD PENDING with anchored,
unapplied hook-ups: 71, 9 and 5 edits into `NifSkope.pro`, `src/nifskope.cpp`,
`src/nifskope.h`, `src/nifskope_ui.cpp` and `src/ui/nifskope.ui`, files lane
BUILD8 owned while they ran. All three are applied, built and measured here,
strictly in that order, one build per lane; each lane's own entry below carries
its numbers. Then the alignment bungo raised on a screenshot the same hour.

**THE ALIGNMENT.** His words, on a screenshot: *"See the issue with alignment
here?"* -- the left dock's tab strip (Header / Blocks / Files) and the
viewport's toolbar (Object Mode / Select / Add / Object / Global) did not share
a row. Measured, in main-window coordinates, before anything was changed
(`tests/spells/ui_align.sh`, `WW_UIALIGN_TEST`, `src/uialigntest.cpp`):

| bar | top | height |
|---|---|---|
| main toolbars `tFile` / `tView` | 0 | 35 |
| viewport toolbar `ViewportHeader` | 35 | **33** |
| dock tab strip `LeftColumnModeSelector` | 35 | **26** |
| dock search row | **65** | 23 |
| viewport content begins | **68** | -- |

So the tops of row 1 already agreed and the HEIGHTS did not: a 7 px step at the
seam, and in consequence the dock's second row started 3 px above the line the
viewport's content starts on.

Three bars in three different parents -- the QMainWindow's toolbar area, the
central column, the dock -- can never be made to agree by a layout, because no
layout contains more than one of them. Something has to state the rule, and it
is now stated once, in the shared skin helpers rather than as three
`setFixedHeight` calls at three call sites (which is how they came to disagree):
`wwAlignBarRow()` takes the TALLEST natural height among the bars and gives it
to all of them, and `wwStartContentBelowBar()` zeroes the top margin of the dock
page whose first row has to begin on the viewport's content line. The row is
never a typed constant and never shrinks a bar: growing one is safe, and
squeezing a `QToolBar` does not clip it, it hides controls behind an extension
chevron. The tab strip is then restyled through the same shared segmented sheet
(`wwSegmentedTabBarQss( rowHeight )`, one piece of arithmetic that knows the
sheet's own padding) so its tabs FILL the row instead of sitting in the top
26 px of it.

After: tab strip top 35 height 35, viewport toolbar top 35 height 35, search row
top 70, viewport content top 70. **11 checks, 0 failures**, including the floor
that shows the same comparison going red when one bar is grown by 4 px. Pictures
of the seam, same framing: `scratchpad/build9_20260910/seam_before.png` and
`seam_after.png`.

Neighbouring harnesses after the change: `loaded_nifs.sh` 166 checks / 3
failures -- the same three it had before this session's first edit;
`files_tab.sh` 29 / 2 with the same two measured causes; `hkxanim_ui.sh` 48 / 1;
`skeleton_overlay.sh` 17 / 0; `top_bar.sh` 43 / 5, all five pre-existing (it
expects a View menu listing six docks -- Block List, Block Details, Header, NIF
Browser, Inspect, KFM -- that were merged into one "Left Editor" entry before
this session, and the menu it reads back contains "Left Editor").

Nothing is committed.

"""


def main():
    apply = "--apply" in sys.argv
    raw = open(PATH, "rb").read()
    cr0, lf0, n0 = raw.count(b"\r"), raw.count(b"\n"), len(raw)
    text = raw.decode("utf-8")

    if text.count(TITLE) != 1:
        print("REFUSED: the title line was not found exactly once.")
        return 1
    if "lane BUILD9" in text:
        print("REFUSED: an entry for lane BUILD9 is already in the file.")
        return 1

    block = BUILD9
    for path, status in LANE_FILES:
        body = open(path, encoding="utf-8").read()
        # drop a leading HTML comment addressed to the splicer
        if body.lstrip().startswith("<!--"):
            body = body[body.index("-->") + 3:]
        body = body.lstrip("\n")
        # every lane entry becomes a ### section of this one entry
        lines = body.split("\n")
        if lines[0].startswith("## ") and not lines[0].startswith("### "):
            lines[0] = "#" + lines[0]
        body = "\n".join(lines).rstrip("\n")
        block += body + "\n" + status.rstrip("\n") + "\n\n"

    at = text.index(TITLE) + len(TITLE)
    # keep the blank line that follows the title
    while text[at] == "\n":
        at += 1
    out = (text[:at] + block + text[at:]).encode("utf-8")

    print("%s bytes %d -> %d   CR %d -> %d   LF %d -> %d"
          % (PATH, n0, len(out), cr0, out.count(b"\r"), lf0, out.count(b"\n")))
    if out.count(b"\r") != cr0:
        print("REFUSED: the CR count moved. Nothing written.")
        return 1
    if not apply:
        print("--check: the splice is clean and the CR count is unchanged. Nothing written.")
        return 0
    open(PATH, "wb").write(out)
    print("--apply: written.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
