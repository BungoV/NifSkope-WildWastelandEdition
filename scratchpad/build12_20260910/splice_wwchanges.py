"""Rewrite lane WATER7's WW_CHANGES.md entry in place with BUILD12's measured
numbers.  WW_CHANGES.md is a MIXED-line-ending file; the 2026-09 entries at the
top are LF-only.  Every replacement below is LF-only text replacing LF-only
text, so the CR count must not move: 19,020, asserted before and after."""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
P = os.path.join(ROOT, "WW_CHANGES.md")
CR_EXPECTED = 19020

SUBS = []

# 1. the status block at the head of the entry
SUBS.append((
b"""**NOT BUILT.** `scratchpad/build11_20260910/DONE` did not exist and that lane
was still writing (its `gates_summary.txt` at 17:19:40 against a 17:23 check),
so this lane ended BUILD PENDING with
`scratchpad/water7_20260910/PENDING.md`. `hookup.py` is **not applied**. Every
gate figure here is a PRE-REGISTRATION, not a result.
""",
b"""**BUILT AND GATED (lane BUILD12, 2026-09-10).** `release/NifSkope.exe`
**17:45:29**, 20,751,360 bytes (BUILD11's was 17:08:39, 20,693,504).
`hookup.py --check` matched **7 of 7 anchors exactly once**, `--apply` wrote
`src/nifskope.h` 48,915 -> 49,371, `src/nifskope_ui.cpp` 1,488,803 ->
1,492,544 and `NifSkope.pro` 20,185 -> 20,208, CR 0 before and after on all
three, and all six markers read back at the counts the script DERIVES from its
own edit table. qmake ran BEFORE make (`SOURCES` gained
`src/wateruitest.cpp`; `grep -c wateruitest Makefile.Release` = 7); no
`DEFINES` or `CXXFLAGS` line changed, so no object was deleted for a changed
flag. `make` exit code **0**; exe newer than **all 115** changed paths under
`src res tools tests NifSkope.pro`, 0 stale; every one of the **27**
translation units that include the changed `src/wwskin.h` and the **34** that
include `src/nifskope.h` has an object newer than its header, 0 stale;
`res/style.qss` byte-equal to `release/style.qss`. Report:
`scratchpad/lane_build12_report.md`.
"""))

# 2. the row-height prediction -> the measurement
SUBS.append((
b"""**PREDICTION, not a measurement: the row stays 35 and the menu bar grows to it
from ~23-25.** `wwAlignBarRow` takes the TALLEST natural height, so if the menu
bar turns out to be the tallest the row GROWS instead -- the opposite of what
bungo asked for. The resume registers that refuter and says not to ship it in
that case.
""",
b"""**MEASURED, on the 17:45:29 exe: the row is 35 px and it did not grow.**
`wwBarRowHeight() = 35` with `UI/CompactTopBars = true`; five visible bars, all
five at the row height; the menu bar 35 against the dock tab strip's 35, and
the floor that a 4 px difference goes red fires. 35 is the number BUILD9
measured, and the rows below the menu bar sit where the OLD exe had them --
tab strip top 35, viewport toolbar top 35, search row top 70 -- so the menu bar
joined the row at the row's own height and nothing above it moved by a pixel.
What DID move is the two bars BUILD9 left behind: `tMode` and `tRender` were
33 px at top 36 and are now 35 px at top 35. The registered refuter (if the
row reads above 35, set `UI/CompactTopBars` false and do not ship) therefore
did not fire, and the shipped default stays **true**.

**One measured thing the gate lets through.** R3 reads the row's buttons at
**39 px inside the 35 px row** -- 4 px taller than the bar holding them --
and passes, because the check that runs allows 8 px where the spell's own
header promises 1 (the 1 px is enforced only BETWEEN the buttons, `39..39`).
The arithmetic is `wwBarRowButtonQss`: `min-height: rowHeight - 4` plus
`(rowHeight - 18) / 2` of padding. "The buttons" is half of what bungo asked
for, so this is named as red and left for his call rather than changed
(`MISTAKES.md`, lane BUILD12).
"""))

# 3. the pictures
SUBS.append((
b"""**Pictures: OWED, and refused rather than faked.** At 17:23 BUILD11 was still
writing its gate logs, so launching a NifSkope instance for a before/after grab
would have broken "one instance ever" in the middle of another lane's runs.
`water_ui.sh` takes both (`SHOT=` the whole top strip, `TABSHOT=` the left dock
with the Water tab open) and the resume says which exe is still a valid BEFORE.
""",
b"""**Pictures: DELIVERED, before and after, by BUILD12.** All four are under
`scratchpad/build12_20260910/images/`. `water_ui.sh` cannot photograph the OLD
state -- its harness does not exist in the 17:08:39 exe -- so the BEFORE grab
was taken with BUILD9's `ui_align.sh`, the spell that photographs the same
seam, run against that exe before the hook-up was applied: `seam_before.png`
(707x82, three tabs Header/Blocks/Files and the viewport toolbar beside them).
After: `seam_after.png` (the same crop and spell on the new exe, the aligned
Workspaces/LOD/Animation/Collision row now visible above the tabs),
`topbar_after.png` (1512x107, the whole top of the window: File/View/Spells/
Options/Help on the SAME line as Workspaces, LOD 0, Animation and Collision,
then the tab strip beside Object Mode/Select/Add/Object/Global/Overlays, then
the search row) and `watertab.png` (278x741, the left dock with FOUR tabs and
Water selected -- Landscape file, Marking, Selected body, a folding Bake
section, the refusal sentence and the pinned Water window / Reload / Solve /
Save row).

**Gates, on the 17:45:29 exe, one instance at a time.**

| gate | numbers |
|---|---|
| `water_ui.sh` (first run ever) | **30 checks, 0 failures, 0 skips**, PASS (floor 24) |
| `water_weights.sh` | X-gates green **17** (floor 16), PASS; its selftest 64 / 8 on body 2 |
| `water_flow.sh` | F-gates green **17** (floor 17); FAIL (3) = the selftest's exit code + F2 island bank + F5 p99, all pre-registered red |
| `water_mark.sh` | dock **20 / 0** (floor 16), `WW_WATER_MARK_BODY=3` printed; FAIL (2) = the same reds through the model half |
| `water_window.sh` | **46 / 0** (floor 24), PASS |
| `lodl_water.sh` | **33 `ok`, 0 `FAIL`**, control PASS |
| `lodl_open.sh` | **23 / 0**, PASS |
| `ui_align.sh` | **11 / 0**, PASS -- unchanged from the old exe |
| `top_bar.sh` | **43 / 5** = BUILD9's baseline, all five pre-existing |
| `loaded_nifs.sh` | **166 / 2** -- BUILD9's baseline was 166 / **3** |
| `files_tab.sh` | **28 / 2** = BUILD11's baseline |
| `hkxanim_ui.sh` | **48 / 1** = BUILD9/BUILD11's baseline |
| `animws.sh` | **57 / 0**, 1 skip, PASS -- the number that had to hold, held |

**Of BUILD10's five reds, four are green and one is half.** X2b, the new
net-flux ring, reads **0.595** on body 2 against the > 0.5 floor registered
before the code (green, control 0.173 < 0.2) and **0.407** on body 3 (RED,
control 0.155 < 0.2). Both floors hold, so the ring measures the pin and not
the channel; what is unsettled is whether 0.5 is the right floor or body 3's
pin is genuinely a weaker source. The retired disc metric printed beside it
still reads 0.742 / 0.371, so the new instrument's disagreement between the two
bodies is 1.46x where the old one's was 2.00x -- smaller, not gone. Reds 2, 3
and 4 are green by their own printed arithmetic, and red 5 is gated by name in
both halves: `ok F7 a dye pin's weight one half-distance downstream is 1/2
(0.5000)` and `ok the override came back out of the table (name 'harness
river')`, with `ok P8 round trip: save, reopen, save is byte-identical
(39,235,147 vs 39,235,147 bytes)` holding the property the byte-0 reservation
had to preserve. `water_weights.sh` is the one spell that still does NOT pin
its body and runs on body 2, the marsh, which is why its selftest carries 8
failures against body 3's 3 -- four of the five extra are the dye gates that
have no mouth on that body.
"""))


def main():
    b = open(P, "rb").read()
    cr, n = b.count(b"\r"), len(b)
    assert cr == CR_EXPECTED, "CR is %d, expected %d" % (cr, CR_EXPECTED)
    for old, new in SUBS:
        c = b.count(old)
        assert c == 1, "anchor matches %d times: %r" % (c, old[:60])
        assert b"\r" not in old and b"\r" not in new, "CRLF in a splice text"
        b = b.replace(old, new)
    open(P, "wb").write(b)
    a = open(P, "rb").read()
    assert a.count(b"\r") == CR_EXPECTED, a.count(b"\r")
    print("WW_CHANGES.md %d -> %d bytes, CR %d (unmoved), LF %d"
          % (n, len(a), a.count(b"\r"), a.count(b"\n")))


main()
