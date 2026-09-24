#!/usr/bin/env python3
"""Lane WATER8: group T retired, group L in its place.

src/wateruitest.cpp and tests/spells/water_ui.sh are lane WATER8's files, but
lane UI4 was still writing both when this was written (its BUILDING marker was
up and it had touched them at 20:41 and 20:42). So the change to them is a
REFUSING script with exact-once anchors, exactly as the shared files are
(`ww-anchored-hookup`), and it is run only once scratchpad/ui4_20260910/DONE
exists.

The C++ side splices BETWEEN two exact-once markers rather than matching 140
lines verbatim, so a change UI4 makes INSIDE group R or S does not make this
refuse for the wrong reason.

Usage:  python gate_patch.py            # check, writes nothing
        python gate_patch.py --apply
"""

import sys, os

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

CPP = "src/wateruitest.cpp"
SPELL = "tests/spells/water_ui.sh"

# ---- the C++ splice ------------------------------------------------------
T_START = """\t\t\t// =============================================================
\t\t\t//  T -- the Water tab
\t\t\t// =============================================================
"""

T_END = """
\t\t\tlog << st->checks << " checks, " << st->fails << " failures, "
"""

L_BLOCK = """\t\t\t// =============================================================
\t\t\t//  L -- the Water tab, in the LOD GENERATION PANEL (lane WATER8)
\t\t\t//
\t\t\t//  bungo, on seeing lane WATER7's Water tab in the LEFT strip,
\t\t\t//  verbatim: "What? I wanted it in that right panel though".  Group
\t\t\t//  T measured the left strip and is RETIRED; group L measures the
\t\t\t//  LOD Generation panel's own strip -- two tabs, LOD | Water, in the
\t\t\t//  same row and the same sheet as Header | Blocks | Files -- and
\t\t\t//  asserts the left strip went back to its three tabs in both
\t\t\t//  workspace states.
\t\t\t//
\t\t\t//  It lives in src/wateruitest_lod.cpp because THIS file belonged to
\t\t\t//  lane UI4 while WATER8 was written (one lane per file,
\t\t\t//  CONSTITUTION 1), which is the same translation-unit rule every
\t\t\t//  other harness in this tree follows.  It writes into this run's own
\t\t\t//  log and counts, so there is still one spell and one verdict.
\t\t\t// =============================================================
\t\t\tsay( *st, QStringLiteral( "  the window holds: LodGenerationDock %1, LeftColumnDock "
\t\t\t\t"%2, LeftColumnStack %3 pages, ViewWorkspacesMenu %4 actions" )
\t\t\t\t.arg( lodDock ? QStringLiteral( "yes" ) : QStringLiteral( "no" ),
\t\t\t\t\t  leftDock ? QStringLiteral( "yes" ) : QStringLiteral( "no" ) )
\t\t\t\t.arg( stack ? stack->count() : -1 )
\t\t\t\t.arg( wsMenu ? wsMenu->actions().size() : -1 ) );
\t\t\t{
\t\t\t\textern void wwWaterUiLodTabs( NifSkope *, QTextStream &, int &, int &,
\t\t\t\t\tint &, const QString &, const QString & );
\t\t\t\twwWaterUiLodTabs( skope, log, st->checks, st->fails, st->skips,
\t\t\t\t\tqEnvironmentVariable( "WW_WATERUI_LODSHOT" ),
\t\t\t\t\tqEnvironmentVariable( "WW_WATERUI_LODSHOT2" ) );
\t\t\t}
"""

# ---- the spell: the gate-name loop --------------------------------------
SPELL_GATES_OLD = """for g in "(T1) the strip carries a tab" "(T1 floor)" "(T2) the Water tab's mode" \\
\t"(T3) the stack carries the water page" "(T4) selecting the Water tab" \\
\t"(T4 floor)" "(T5) the Water tab is hidden" "(T5) ...and shown" \\
\t"(T6) closing the workspace" "(T7) the Workspaces menu offers no" \\
\t"(T7) ...and no" "(T7 floor)" "(T8) there is no Water Marking dock" \\
"""

SPELL_GATES_NEW = """for g in "(L floor) the LOD Generation panel's strip" \\
\t"(L1) the LOD panel's strip carries exactly 2 tabs" "(L1) they are" "(L1 floor)" \\
\t"(L2) the stack carries the generator's page" "(L2) each tab's data" "(L2 floor)" \\
\t"(L3) selecting Water shows the water page" "(L3) ...and the marking rows" \\
\t"(L3) ...and the full-screen flow window's button" "(L3 floor)" \\
\t"(L4) the LOD panel's strip is the shared row" \\
\t"(L4) ...the same height as the left strip" "(L4 floor)" \\
\t"(L5) the two strips carry the SAME stylesheet" "(L5 floor)" \\
\t"(L6) the left strip has exactly 3 tabs" "(L6) ...and exactly 3 with it CLOSED" \\
\t"(L6) no tab named" "(L6) LeftColumnStack is back to 3 pages" "(L6 floor)" \\
\t"(L7) the Workspaces menu offers no" "(L7) ...and no" "(L7 floor)" \\
\t"(L7) there is no Water Marking dock" \\
\t"(L8) the LOD strip's first segment" "(L8 floor)" \\
"""

# ---- the spell: the environment it passes -------------------------------
SPELL_ENV_OLD = """\tWW_WATERUI_STRIPSHOT="${STRIPSHOT:+$(winpath "$STRIPSHOT")}" \\
"""

SPELL_ENV_NEW = """\tWW_WATERUI_LODSHOT="${LODSHOT:+$(winpath "$LODSHOT")}" \\
\tWW_WATERUI_LODSHOT2="${LODSHOT2:+$(winpath "$LODSHOT2")}" \\
"""

# ---- the spell: the floor ------------------------------------------------
SPELL_FLOOR_OLD = """echo "checks run: ${COUNT:-none} (floor 41)"
case "${COUNT:-}" in
\t''|*[!0-9]*) echo "FAIL: the log carries no check count"; fails=$((fails+1)) ;;
\t*) [ "$COUNT" -ge 41 ] || { echo "FAIL: only $COUNT checks ran, floor is 41"; fails=$((fails+1)); } ;;
esac
"""

SPELL_FLOOR_NEW = """# LANE WATER8 re-counted it: group T (15 checks, one of them a picture) is
# retired and group L takes its place -- 30 with both LOD-panel pictures, 28
# with none, and 5 of those 30 skip if the shared bar row is off
# (UI/CompactTopBars false), so the smallest run that is still WORKING is
# 44 - 14 + 23 = 53.  48 is five below that, so losing a check still goes red
# however the spell is called, and the first green run tightens it.
echo "checks run: ${COUNT:-none} (floor 48)"
case "${COUNT:-}" in
\t''|*[!0-9]*) echo "FAIL: the log carries no check count"; fails=$((fails+1)) ;;
\t*) [ "$COUNT" -ge 48 ] || { echo "FAIL: only $COUNT checks ran, floor is 48"; fails=$((fails+1)); } ;;
esac
"""

# ---- the spell: the header ----------------------------------------------
SPELL_HEAD_OLD = """#   T1  the strip carries a tab named exactly "Water"     (floor: "Wagter" -> none)
#   T2  its mode number IS the water page's stack index
#   T3  the stack has four pages and one of them is the marking panel
#   T4  selecting the tab shows that page                 (floor: Blocks -> page 0)
#       ** a red on T4 with everything else green means hookup.py edit E2, the
#          setLeftColumnMode clamp, did not land **
#   T5  the tab is hidden while the LOD workspace is closed AND shown while it
#       is open -- both halves in one run
#   T6  closing the workspace with the Water tab current leaves the strip on a
#       VISIBLE tab
#   T7  the Workspaces menu offers neither "Water Marking" nor "Water window"
#       (floor: the same scan still finds "LOD Generation")
#   T8  no WaterMarkDock is left in the window            (floor: LodGenerationDock is)
"""

SPELL_HEAD_NEW = """# (A2) CORRECTED, 2026-09-10 20:3x, verbatim: "What? I wanted it in that right
#      panel though".  The strip was the STYLE and the LOD GENERATION PANEL is
#      the PLACE.  Group T measured the left strip and is retired; group L
#      (src/wateruitest_lod.cpp) measures the panel's own strip.
#
#   L1  the LOD panel's strip carries exactly 2 tabs, LOD then Water
#       (floor: the same search finds no "Watre")
#   L2  its pages are LodgenPanel at 0 and WaterMarkPanel at 1, and each tab's
#       data IS its page index
#       (floor: the SAME predicate asked at the wrong index goes false)
#   L3  selecting Water shows the marking rows AND the flow-window button
#       (floor: selecting LOD puts the generator's page and Generate back)
#   L4  that strip is wwBarRowHeight() and the same height as the left strip
#   L5  ...and carries the SAME stylesheet, byte for byte, hashes printed
#       (floor: the compact default is a different string)
#   L6  the LEFT strip is back to 3 tabs and 3 pages, with the LOD workspace
#       OPEN and again with it CLOSED -- both states in one run
#   L7  no WaterMarkDock, and no "Water Marking" / "Water window" in the
#       Workspaces menu (floors: the same scans still find the LOD dock and
#       the "LOD Generation" entry)
#   L8  the panel's strip paints its segments 4 px clear of the row, top and
#       bottom (floor: the LEFT strip reads the same two numbers)
"""

# ---- the spell: usage ----------------------------------------------------
SPELL_USAGE_OLD = """#   STRIPSHOT=C:/path/strip4x.png bash tests/spells/water_ui.sh
"""

SPELL_USAGE_NEW = """#   LODSHOT=C:/path/lodtab_lod.png LODSHOT2=C:/path/lodtab_water.png \\
#       bash tests/spells/water_ui.sh
"""

# (file, mode, anchor, text, marker)
EDITS = [
    (CPP,   "splice",  (T_START, T_END), L_BLOCK,        "group L"),
    (SPELL, "replace", SPELL_HEAD_OLD,   SPELL_HEAD_NEW, "(A2) CORRECTED"),
    (SPELL, "after",   SPELL_USAGE_OLD,  SPELL_USAGE_NEW, "LODSHOT2="),
    (SPELL, "after",   SPELL_ENV_OLD,    SPELL_ENV_NEW,  "WW_WATERUI_LODSHOT"),
    (SPELL, "replace", SPELL_GATES_OLD,  SPELL_GATES_NEW, "(L1) they are"),
    (SPELL, "replace", SPELL_FLOOR_OLD,  SPELL_FLOOR_NEW, "floor 48"),
]


def read(path):
    with open(os.path.join(ROOT, path), "rb") as f:
        return f.read()


def main():
    apply = "--apply" in sys.argv
    done = os.path.join(ROOT, "scratchpad", "ui4_20260910", "DONE")
    if apply and not os.path.exists(done):
        print("REFUSED: scratchpad/ui4_20260910/DONE does not exist, so lane UI4 still owns")
        print("         %s and %s." % (CPP, SPELL))
        return 2

    files = {}
    for path, _, _, _, _ in EDITS:
        if path not in files:
            files[path] = read(path)
    planned = dict(files)

    print("lane WATER8 gate patch -- %s" % ("APPLY" if apply else "check only, writes nothing"))
    ok = 0
    for i, (path, mode, anchor, text, marker) in enumerate(EDITS, 1):
        buf = planned[path]
        applied = files[path].count(marker.encode("utf-8"))
        if mode == "splice":
            start, end = anchor
            ns, ne = buf.count(start.encode("utf-8")), buf.count(end.encode("utf-8"))
            print("  G%-2d %-24s splice  start %d, end %d | marker %r x%d"
                  % (i, path, ns, ne, marker, applied))
            if ns != 1 or ne != 1:
                print("      REFUSED: both markers must occur exactly once")
                continue
            ok += 1
            if apply:
                s = buf.index(start.encode("utf-8"))
                e = buf.index(end.encode("utf-8"))
                planned[path] = buf[:s] + text.encode("utf-8") + buf[e:]
            continue

        raw = anchor.encode("utf-8")
        n = buf.count(raw)
        print("  G%-2d %-24s %-7s anchor x%d | marker %r x%d"
              % (i, path, mode, n, marker, applied))
        if n != 1:
            print("      REFUSED: the anchor does not match exactly once")
            continue
        ok += 1
        if apply:
            ins = text.encode("utf-8")
            planned[path] = buf.replace(raw, ins if mode == "replace" else raw + ins, 1)

    print("  %d of %d anchors match exactly once" % (ok, len(EDITS)))
    for path, buf in files.items():
        print("  %-24s CR %d, %d bytes" % (path, buf.count(b"\r"), len(buf)))

    if not apply:
        return 0 if ok == len(EDITS) else 1
    if ok != len(EDITS):
        print("  NOTHING WRITTEN: every anchor must match once before any file is touched")
        return 1
    for path, buf in planned.items():
        before = files[path]
        assert buf.count(b"\r") == before.count(b"\r"), (
            "%s: CR %d -> %d, and nothing here inserts a CR"
            % (path, before.count(b"\r"), buf.count(b"\r"))
        )
        with open(os.path.join(ROOT, path), "wb") as f:
            f.write(buf)
        print("  wrote %-24s %d -> %d bytes, CR %d"
              % (path, len(before), len(buf), buf.count(b"\r")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
