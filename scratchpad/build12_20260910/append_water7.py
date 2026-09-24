"""Append `## Build (BUILD12)` to lane WATER7's report. Append-only: the
lane's own text is never rewritten (CONSTITUTION 8)."""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
P = os.path.join(ROOT, "scratchpad", "lane_water7_report.md")

TEXT = """
## Build (BUILD12)

Appended by lane BUILD12, 2026-09-10. Nothing above this line was changed.

**The exe.** `release/NifSkope.exe` **17:45:29**, 20,751,360 bytes. Built from
the applied hook-up on the 17:08:39 tree. Game check `rc=1` before the build and
before every exe launch; no NifSkope was running, so no exe was renamed aside.

**The hook-up.** `--check` 7 of 7 anchors exactly once; `--apply` wrote
`src/nifskope.h` 48,915 -> 49,371, `src/nifskope_ui.cpp` 1,488,803 -> 1,492,544,
`NifSkope.pro` 20,185 -> 20,208, CR 0 -> 0 on all three. Markers read back:
`LeftWater = 3` 2, `mode > LeftWater` 1, `wwBarRowButtonQss` **3**,
`UI/CompactTopBars` **3**, `wwWaterUiHarness` 2, `src/wateruitest.cpp` 1 --
each equal to the script's own derived expectation. **Two rows of PENDING.md's
marker table were wrong** (both tabulated as 2); the DERIVATION was right, and
section 5's own mistake entry states 2 as "the truth" where the truth is 3.
That is BUILD12's first MISTAKES entry.

**Build chain.** qmake before make (RC 0, `wateruitest` 7 times in
`Makefile.Release`); no `DEFINES`/`CXXFLAGS` change, so no object deleted for a
changed flag; `make` exit 0; exe newer than all **115** changed paths (0 stale);
0 stale objects among the **27** TUs including `src/wwskin.h` and the **34**
including `src/nifskope.h`; `res/style.qss` = `release/style.qss`.

**The 35-px check: ANSWERED, and the change ships.** `wwBarRowHeight() = 35`,
`UI/CompactTopBars = true`, 5 visible bars all at the row height, menu bar 35
against the tab strip's 35. 35 is BUILD9's number; the tab strip is still at
top 35, the viewport toolbar at top 35 and the search row at top 70, exactly
where the 17:08:39 exe had them. `tMode` and `tRender` moved from 33 px at
top 36 to 35 px at top 35 -- they joined the row. **The top did not grow, the
refuter did not fire, and `UI/CompactTopBars` stays true.**

R3, the button half, is the one measured miss: the buttons are **39 px inside
the 35 px row** and the check passes because it allows 8 px where the spell's
own header promises 1. Left as a verdict for the director; second MISTAKES
entry.

**Gates** (all on the 17:45:29 exe, sequential, one instance):
`water_ui.sh` **30 / 0 / 0 skips** PASS (floor 24) -- its first run ever;
`water_weights.sh` X-gates green **17** (floor 16) PASS;
`water_flow.sh` F-gates green **17** (floor 17), spell FAIL (3) = the
selftest's exit code plus F2 island bank and F5 p99, both pre-registered red;
`water_mark.sh` dock **20 / 0** (floor 16) with `WW_WATER_MARK_BODY=3` printed,
spell FAIL (2) = the same reds through the model half;
`water_window.sh` **46 / 0**; `lodl_water.sh` 33 ok / 0 FAIL;
`lodl_open.sh` **23 / 0**. Neighbours: `ui_align.sh` **11 / 0** (unchanged),
`top_bar.sh` **43 / 5** (baseline), `loaded_nifs.sh` **166 / 2** (baseline was
166 / 3), `files_tab.sh` **28 / 2** (baseline), `hkxanim_ui.sh` **48 / 1**
(baseline), `animws.sh` **57 / 0** with 1 skip -- the number that had to hold.

**The five reds:** 2, 3, 4 and 5 green by their own printed numbers; **X2b is
half** -- 0.595 on body 2 (green, > 0.5) and **0.407 on body 3 (red)**, both
controls under 0.2. The retired disc metric beside it reads 0.742 / 0.371, so
the new ring's body-to-body disagreement is 1.46x where the old one's was
2.00x. Candidates, not a cause: the floor was registered off body 2's number,
or the ring at two pin widths clips body 3's narrower channel; the
discriminator is the same ring at one and four pin widths on both bodies.

**Pictures, both OWED items discharged** (in
`scratchpad/build12_20260910/images/`): `seam_before.png` (the 17:08:39 exe,
taken with `ui_align.sh` because `water_ui.sh`'s harness does not exist in that
exe), `seam_after.png`, `topbar_after.png` and `watertab.png`.
"""


def main():
    before = open(P, "rb").read()
    cr, n = before.count(b"\r"), len(before)
    add = TEXT.encode("utf-8")
    assert b"\r" not in add
    assert before.endswith(b"\n")
    open(P, "wb").write(before + add)
    after = open(P, "rb").read()
    assert after.startswith(before) and len(after) == n + len(add)
    assert after.count(b"\r") == cr
    print("lane_water7_report.md %d -> %d bytes, CR %d -> %d"
          % (n, len(after), cr, after.count(b"\r")))


main()
