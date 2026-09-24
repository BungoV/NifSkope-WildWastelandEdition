#!/usr/bin/env python3
"""Lane WATER8's hook-up: the four shared files, as a REFUSING script.

`ww-anchored-hookup`. One EDITS table, exact anchors that must match ONCE,
`--check` as the default that writes NOTHING, the CR byte count asserted
before and after, and the applied-state signal read from a MARKER the
inserted text carries -- never from "the anchor still matches", which it
does for an "after" insert whether or not the edit landed.

WHAT IT DOES, in two halves:

  REVERSE lane WATER7's left-strip insertion (edits R1, R2). bungo, on
  seeing that strip, verbatim: "What? I wanted it in that right panel
  though". The LeftWater mode and the clamp that let it be reached go away
  again, so LeftColumnMode is Blocks/Nifs/Header exactly as it was before
  WATER7. Both are found by WATER7's own "lane WATER7" markers.

  JOIN lane WATER8's new strip to the shared bar row (edits A1, A2) and put
  its harness in the build (edit P1).

Every one of this lane's OWN new files compiles with and without this
script applied -- nothing in src/lodgenmanager.cpp, src/watermarkpanel.cpp
or src/wateruitest_lod.cpp names LeftWater or anything the hook-up adds --
so there is no compile-time switch here and section 2 of the skill does not
apply. The overlay syntax pass (sx_overlay.py) proves the INSERTED text
compiles in place, which is the part a syntax pass over this lane's own
files cannot reach.

Usage:  python hookup.py            # check, writes nothing
        python hookup.py --apply    # all files at once, or none
"""

import sys, os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

# ---------------------------------------------------------------- R1
NH_OLD = """\t/* LeftWater = 3 is the water page (lane WATER7).
\t *
\t * bungo, 2026-09-10: the water tool is a TAB of the LOD Generation
\t * workspace, in this same strip -- Header | Blocks | Files | Water.
\t * The mode number IS the stack page index (setLeftColumnMode does
\t * leftColumnStack->setCurrentIndex( int( mode ) )), and the page is
\t * added by src/watermarkpanel.cpp, which asserts it landed at 3 and
\t * refuses in words if it did not. */
\tenum LeftColumnMode { LeftBlocks = 0, LeftNifs = 1, LeftHeader = 2, LeftWater = 3 };
"""

NH_NEW = """\t/* THREE MODES, AND THE WATER PAGE IS NOT ONE OF THEM (lane WATER8).
\t *
\t * Lane WATER7 added LeftWater = 3 and a fourth page to LeftColumnStack,
\t * reading bungo's "You'd access them like this" over the
\t * Header | Blocks | Files strip as naming the PLACE. He saw the result
\t * and said, verbatim: "What? I wanted it in that right panel though".
\t * The strip was the STYLE; the LOD Generation panel is the place, and
\t * the water rows are a tab of THAT strip now
\t * (src/lodgenmanager.cpp, tlCreateLodGenerationDock -> LodPanelModeSelector
\t * over LodPanelStack; the page is added by src/watermarkpanel.cpp).
\t *
\t * So this enum is back to what it was before WATER7, and there is no
\t * unreachable fourth mode left behind: the mode number IS the stack page
\t * index (setLeftColumnMode does setCurrentIndex( int( mode ) )), so a
\t * mode with no page would be a tab that bounces. */
\tenum LeftColumnMode { LeftBlocks = 0, LeftNifs = 1, LeftHeader = 2 };
"""

# ---------------------------------------------------------------- R2
UI_CLAMP_OLD = """\t/* lane WATER7: LeftWater, not LeftHeader. Clamping 3 back to 0 here is
\t * what would make the Water tab bounce to Blocks the moment it is
\t * clicked -- gate T4 of tests/spells/water_ui.sh is named after exactly
\t * that failure, so a resume that skipped this edit reads it by name. */
\tif ( mode < LeftBlocks || mode > LeftWater )
\t\tmode = LeftBlocks;
"""

UI_CLAMP_NEW = """\t/* lane WATER8: back to LeftHeader. WATER7 raised this clamp to LeftWater
\t * so a fourth page of this stack could be reached; bungo's "What? I
\t * wanted it in that right panel though" retired that page, and a clamp
\t * that still admitted 3 would let a stale saved mode select a page this
\t * stack no longer has. Gate L6 of tests/spells/water_ui.sh counts the
\t * strip's tabs and the stack's pages at 3 in both workspace states. */
\tif ( mode < LeftBlocks || mode > LeftHeader )
\t\tmode = LeftBlocks;
"""

# ---------------------------------------------------------------- A1
UI_BARS_ANCHOR = """\tif ( wwCompactTopBars() )
\t\twwBarRowBars.prepend( ui->menubar );
"""

UI_BARS_ADD = """\t/* THE LOD GENERATION PANEL'S STRIP IS THE SAME ROW (lane WATER8).
\t *
\t * bungo's correction of 2026-09-10, verbatim: "What? I wanted it in that
\t * right panel though" -- the water tool is a tab of the LOD Generation
\t * panel, and that panel's strip has to be the same 35 px row and the same
\t * skin as Header | Blocks | Files or the two segmented strips in one
\t * window would be two different controls. It is built in
\t * src/lodgenmanager.cpp and found here by object name, because the rule
\t * about the row lives ONCE, here, for every bar at once -- three
\t * setFixedHeight calls at three call sites is how the bars came to
\t * disagree in the first place.
\t *
\t * It is APPENDED, so the row is still whatever the tallest bar needs: a
\t * 2-tab QTabBar's natural height is about 26 and cannot be the tallest,
\t * and gate L4 prints the number so a strip that DID grow the row is read
\t * as a number rather than seen as a taller window.
\t *
\t * FALLBACK (CONSTITUTION 10): no LOD dock in this window -- a build with
\t * the generator absent -- and nothing is appended; every other bar is
\t * unaffected and the strip, if it appears later, keeps the compact
\t * default it was built with. */
\tQTabBar * lodPanelSelector =
\t\tfindChild<QTabBar *>( QStringLiteral( "LodPanelModeSelector" ) );
\tif ( lodPanelSelector )
\t\twwBarRowBars.append( lodPanelSelector );
"""

# ---------------------------------------------------------------- A2
UI_SHEET_ANCHOR = """\t\tleftColumnSelector->setStyleSheet( wwSegmentedTabBarQss( wwBarRowHeight(), this ) );
"""

UI_SHEET_ADD = """\tif ( lodPanelSelector && wwBarRowHeight() > 0 )
\t\t/* lane WATER8: THE SAME CALL, with the same two arguments, so the two
\t\t * strips cannot carry different sheets. Gate L5 compares them byte for
\t\t * byte and hashes both into the log, which is the only way a drift
\t\t * between two call sites is ever noticed. */
\t\tlodPanelSelector->setStyleSheet( wwSegmentedTabBarQss( wwBarRowHeight(), this ) );
"""

# ---------------------------------------------------------------- P1
PRO_ANCHOR = "\tsrc/wateruitest.cpp \\\n"
PRO_ADD = "\tsrc/wateruitest_lod.cpp \\\n"

# (file, mode, anchor, text, marker-that-says-it-is-applied)
EDITS = [
    ("src/nifskope.h",      "replace", NH_OLD,          NH_NEW,        "lane WATER8"),
    ("src/nifskope_ui.cpp", "replace", UI_CLAMP_OLD,    UI_CLAMP_NEW,  "lane WATER8: back to LeftHeader"),
    ("src/nifskope_ui.cpp", "after",   UI_BARS_ANCHOR,  UI_BARS_ADD,   "LodPanelModeSelector"),
    ("src/nifskope_ui.cpp", "after",   UI_SHEET_ANCHOR, UI_SHEET_ADD,  "lodPanelSelector->setStyleSheet"),
    ("NifSkope.pro",        "after",   PRO_ANCHOR,      PRO_ADD,       "src/wateruitest_lod.cpp"),
]


def read(path):
    with open(os.path.join(ROOT, path), "rb") as f:
        return f.read()


def write(path, data):
    with open(os.path.join(ROOT, path), "wb") as f:
        f.write(data)


def variants(text):
    """The anchor with the file's own line ending. `src/` is LF-only today, but
    a file that becomes CRLF later must not silently count 0 and read as a bad
    anchor (ww-anchored-hookup 3a)."""
    lf = text.encode("utf-8")
    crlf = text.replace("\n", "\r\n").encode("utf-8")
    return [("LF", lf), ("CRLF", crlf)]


def main():
    apply = "--apply" in sys.argv
    files = {}
    for path, _, _, _, _ in EDITS:
        if path not in files:
            files[path] = read(path)

    print("lane WATER8 hook-up -- %s" % ("APPLY" if apply else "check only, writes nothing"))
    ok = 0
    planned = {p: b for p, b in files.items()}
    for i, (path, mode, anchor, text, marker) in enumerate(EDITS, 1):
        buf = planned[path]
        chosen = None
        for kind, raw in variants(anchor):
            n = buf.count(raw)
            if n == 1:
                chosen = (kind, raw, n)
                break
        counts = ", ".join("%s %d" % (k, buf.count(r)) for k, r in variants(anchor))
        applied = files[path].count(marker.encode("utf-8"))
        print("  E%-2d %-22s %-7s anchor: %s | marker %r x%d"
              % (i, path, mode, counts, marker, applied))
        if chosen is None:
            print("      REFUSED: the anchor does not match exactly once")
            continue
        ok += 1
        if not apply:
            continue
        kind, raw, _ = chosen
        ins = text if kind == "LF" else text.replace("\n", "\r\n")
        ins = ins.encode("utf-8")
        planned[path] = buf.replace(raw, ins if mode == "replace" else raw + ins, 1)

    print("  %d of %d anchors match exactly once" % (ok, len(EDITS)))
    for path, buf in files.items():
        print("  %-22s CR %d, %d bytes" % (path, buf.count(b"\r"), len(buf)))

    if not apply:
        return 0 if ok == len(EDITS) else 1
    if ok != len(EDITS):
        print("  NOTHING WRITTEN: every anchor must match once before any file is touched")
        return 1
    for path, buf in planned.items():
        before = files[path]
        # a rename at equal length must move nothing: the CR count may only
        # change by the CRs of the text this script inserted
        added = sum(
            (t if variants(a)[0][1] in before else t.replace("\n", "\r\n")).count("\r")
            for p, m, a, t, _ in EDITS if p == path
        )
        removed = sum(
            a.count("\r") for p, m, a, t, _ in EDITS if p == path and m == "replace"
        )
        want = before.count(b"\r") + added - removed
        assert buf.count(b"\r") == want, (
            "%s: CR %d -> %d, expected %d" % (path, before.count(b"\r"), buf.count(b"\r"), want)
        )
        write(path, buf)
        print("  wrote %-22s %d -> %d bytes, CR %d"
              % (path, len(before), len(buf), buf.count(b"\r")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
