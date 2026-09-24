#!/usr/bin/env python3
"""Lane HARNESSWIN1 -- the hook-up, as a REFUSING script (skill ww-anchored-hookup).

The lane's own work is in two NEW files, src/harnesswindow.h and
src/harnesswindow.cpp.  These are the five lines that existing files need so
those two join the build and are actually called.  This lane may NOT apply them:
lane IMPOSTORSHOW owns the build slot and is inside NifSkope.pro and
src/nifskope_ui.cpp at the same time.  The director applies this AFTER
IMPOSTORSHOW lands.

    python hookup.py            # --check, the default, WRITES NOTHING
    python hookup.py --apply    # refuses unless every anchor still matches once

Every anchor is READ OUT OF THE FILE from a prefix that is safe to type, so no
tab, no trailing comment and no line-continuation backslash is ever retyped
(ww-anchored-hookup 5a; the .pro lines all end in a backslash, which a heredoc
would have halved).  Both files are LF-only, measured: NifSkope.pro CR 0,
src/nifskope_ui.cpp CR 0.  The CR count is asserted unchanged, because a
line-ending slip in either file is invisible in a diff and fatal to the build.

ANCHORS ARE DELIBERATELY FAR FROM src/nifskope_ui.cpp ~22000-23300, the impostor
bake/harness block IMPOSTORSHOW is editing, and far from the src/impostor* lines
of NifSkope.pro.  Chosen sites: the .pro's filestab.h / archlocktest.cpp rows,
and nifskope_ui.cpp's starterscene.h include (line ~39), wwPlaceHeadlessWindow
(~1523) and restoreUi (~31529).
"""

import io
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
LF = chr(10)
TAB = chr(9)
BS = chr(92)

AFTER = "after"
REPLACE = "replace"

# (path, mode, prefix-to-find-the-real-line, lines-to-insert-or-replace-with)
#
# The prefix must identify the line uniquely and must be free of tabs beyond the
# leading one; the script supplies the rest of the line from the file's bytes.
EDITS = [
    (
        "NifSkope.pro", AFTER, TAB + "src/filestab.h",
        [TAB + "src/harnesswindow.h " + BS],
    ),
    (
        "NifSkope.pro", AFTER, TAB + "src/archlocktest.cpp",
        [TAB + "src/harnesswindow.cpp " + BS],
    ),
    (
        "src/nifskope_ui.cpp", AFTER, '#include "starterscene.h"',
        ['#include "harnesswindow.h"'],
    ),
    (
        # REPLACE, and the anchor is repeated in the replacement, so a reader
        # can see nothing was eaten.  The move() must stay FIRST: the default
        # window size is derived from the screen the window was just placed on.
        "src/nifskope_ui.cpp", REPLACE, TAB + "w->move( wwHeadlessWindowOrigin( &arm ) );",
        [
            TAB + "w->move( wwHeadlessWindowOrigin( &arm ) );",
            TAB + "/* THE SIZE IS FORCED TOO, NOT ONLY THE PLACE (lane HARNESSWIN1,",
            TAB + " * 2026-09-19).  A harness run that inherits bungo's window measures",
            TAB + " * his last session: a persisted MAXIMIZED geometry swallows the",
            TAB + " * resize() that WW_RENDER_SIZE asks for without a word, which is the",
            TAB + " * standing native_open.sh (c) red -- asked 1024x1024, got 1822x989.",
            TAB + " * After the move(), so the default size is derived from the screen",
            TAB + " * this window was just placed on.  See src/harnesswindow.cpp. */",
            TAB + "wwApplyHarnessWindow( w );",
        ],
    ),
    (
        # src/main.cpp is NOT contended (git status clean at 11:52). The whole
        # QSettings tree moves under WW_SETTINGS_SCOPE, so a gate can plant a
        # maximized geometry without ever reaching bungo's key.
        "src/main.cpp", AFTER, '#include "version.h"',
        ['#include "harnesswindow.h"'],
    ),
    (
        "src/main.cpp", REPLACE,
        TAB + TAB + "a->setApplicationName( " + chr(34) + "NifSkope ",
        [
            TAB + TAB + "/* WW_SETTINGS_SCOPE gives a harness run its own settings key, so a",
            TAB + TAB + " * gate can PLANT window geometry without borrowing the user's and",
            TAB + TAB + " * without having to restore it afterwards (lane HARNESSWIN1,",
            TAB + TAB + " * 2026-09-19). Empty in an ordinary session. */",
            TAB + TAB + "a->setApplicationName( " + chr(34) + "NifSkope " + chr(34)
                + " + NifSkopeVersion::rawToMajMin( NIFSKOPE_VERSION )",
            TAB + TAB + TAB + "+ wwHarnessSettingsSuffix() );",
        ],
    ),
    (
        "src/nifskope_ui.cpp", REPLACE,
        TAB + "restoreGeometry( settings.value( " + chr(34) + "Window Geometry",
        [
            TAB + "/* A HARNESS RUN DOES NOT INHERIT A WINDOW (lane HARNESSWIN1,",
            TAB + " * 2026-09-19).  saveUi() has refused to PERSIST from a WW_* run",
            TAB + " * since 2026-07-27; the read half never got the same guard, so",
            TAB + " * every harness still replayed the position, the size and the",
            TAB + " * MAXIMIZED BIT of whatever window was last closed by hand.  A",
            TAB + " * maximized window discards resize(), so WW_RENDER_SIZE was",
            TAB + " * silently floored and the gate measured the machine.  Same",
            TAB + " * predicate on both sides, or the two can disagree. */",
            TAB + "if ( !wwHarnessGeometryRestoreSuppressed() )",
            TAB + TAB + "restoreGeometry( settings.value( "
                + chr(34) + "Window Geometry" + chr(34) + "_uip ).toByteArray() );",
        ],
    ),
]


def read_lines(path):
    with io.open(os.path.join(ROOT, path), "rb") as fh:
        raw = fh.read()
    return raw, raw.decode("utf-8").split(LF)


def find_one(lines, prefix):
    """The indices of every line starting with prefix. Must be exactly one."""
    return [i for i, ln in enumerate(lines) if ln.startswith(prefix)]


def run(apply_it):
    per_file = {}
    ok = True
    print("root: " + ROOT)
    print("")
    for path, mode, prefix, text in EDITS:
        raw, lines = read_lines(path)
        if path not in per_file:
            per_file[path] = {"raw": raw, "lines": list(lines), "cr": raw.count(b"\r")}
        hits = find_one(per_file[path]["lines"], prefix)
        state = "ok  " if len(hits) == 1 else "REFUSE"
        real = per_file[path]["lines"][hits[0]] if len(hits) == 1 else None
        print("%-6s %-22s %-8s %d hit(s)  prefix=%s"
              % (state, path, mode, len(hits), repr(prefix)))
        if real is not None:
            print("       line %d: %s" % (hits[0] + 1, repr(real)))
        if len(hits) != 1:
            ok = False
            continue
        # Already applied? A marker the inserted text CARRIES, never the anchor
        # itself -- an 'after' anchor still matches once the text is in place,
        # and a 'replace' whose replacement REPEATS the anchor (as both of
        # these do, so the edit reads as additive) matches too
        # (ww-anchored-hookup 4).  Caught by running --check: with text[0] as
        # the marker, edit 4 reported ALREADY APPLIED on a clean tree, because
        # its first replacement line IS the anchor.  text[-1] is a line that
        # exists only after the edit in both modes.
        marker = text[-1]
        if marker in per_file[path]["lines"]:
            print("       ALREADY APPLIED (marker present): " + repr(marker))
            continue
        at = hits[0]
        if mode == AFTER:
            per_file[path]["lines"][at + 1:at + 1] = text
        else:
            per_file[path]["lines"][at:at + 1] = text
        print("       would become:")
        for ln in text[:2]:
            print("         " + repr(ln))
        if len(text) > 2:
            print("         ... %d more, last %s" % (len(text) - 2, repr(text[-1])))
        print("")

    print("")
    for path, st in per_file.items():
        new = LF.join(st["lines"]).encode("utf-8")
        added = new.count(b"\r") - st["cr"]
        print("%-22s CR before %d, CR after %d (delta %d), bytes %d -> %d"
              % (path, st["cr"], new.count(b"\r"), added, len(st["raw"]), len(new)))
        if added != 0:
            print("  REFUSE: the CR count moved -- a line ending was mangled")
            ok = False
        st["new"] = new

    if not ok:
        print("")
        print("REFUSED: not every anchor matched exactly once. Nothing written.")
        return 1

    if not apply_it:
        print("")
        print("--check only. Nothing written.")
        return 0

    for path, st in per_file.items():
        with io.open(os.path.join(ROOT, path), "wb") as fh:
            fh.write(st["new"])
        print("wrote " + path)
    print("")
    print(RESUME)
    return 0


RESUME = """Applied. THE RESUME, IN ORDER (lane HARNESSWIN1 ended BUILD PENDING).

 0. ONLY after lane IMPOSTORSHOW has landed and the game/NifSkope is closed.
    Check first:  tasklist | grep -i nifskope

 1. The red control needs an exe from BEFORE the repair, and it must be taken
    NOW, before the build:
        cp release/NifSkope.exe release/NifSkope.before_harnesswin1.exe

 2. qmake, because NifSkope.pro changed, then make. qmake-before-make is not
    optional here: without it src/harnesswindow.cpp never enters the build and
    everything below links against nothing.
        cd /e/Projects/NifskopeWildWastelandEdition
        C:/msys64/ucrt64/bin/qmake.exe NifSkope.pro
        make -j8 release        # read make's OWN exit code, never grep's

 3. Read the dependency back, not the exe. Three sources changed, so sweep all
    three objects rather than one (nifskope-ww-resume-pending):
        ls -l --time-style=+%H:%M:%S release/.obj/harnesswindow.o \\
              release/.obj/nifskope_ui.o release/.obj/main.o \\
              src/harnesswindow.cpp src/nifskope_ui.cpp src/main.cpp
    Every .o must be NEWER than its .cpp. The exe being newer proves nothing.

 4. The gate, which has NEVER BEEN RUN and whose own defects are still in it:
        bash tests/spells/harness_window.sh
    Rows: (a) a planted maximized geometry does not reach the window;
    (b) the planted settings AND bungo's own key are byte-identical after;
    (c) the RED CONTROL on the rung exe from step 1 -- a SKIP here is not a
    pass; (d) native_open.sh (c) re-measured; (e) the second candidate, whether
    the resize is ALSO floored by the docks.
    Then write the measured check count back into FLOOR at the bottom of that
    script, with the exe timestamp and the date beside it.

 5. The neighbours -- every spell that sets WW_RENDER_SIZE now gets a forced
    window, so their framebuffers may CHANGE SIZE and their baselines may read
    "size mismatch". That is the repair working, not a regression, but it has
    to be looked at:
        bash tests/spells/render_shot.sh
        bash tests/spells/skeleton_overlay.sh
        bash tests/spells/lodl_open.sh
        bash tests/spells/impostor_draw.sh
        bash tests/spells/window_state_roundtrip.sh   # the settings round trip

 6. CHANGE_NEEDED, neither of which this lane could make:
    (i) src/nifskope_ui.cpp ~22096 resizes BEFORE hiding the docks at ~22104,
        so the request can be floored by the dock layout's minimum width even
        with no maximized bit. One-line reorder, inside the block IMPOSTORSHOW
        owns. Row (e) of the gate is its refuter -- do not make the change
        until that row has been RUN and is red.
    (ii) src/animworkspace.cpp:649-654 and src/bodybuildpanel.cpp:560 write
        QSettings from splitterMoved handlers, OUTSIDE saveUi(), so saveUi()'s
        WW_ guard does not cover them. No harness drives either today.

 7. Nothing here is "fixed" until bungo has seen native_open.sh green."""


if __name__ == "__main__":
    sys.exit(run("--apply" in sys.argv))
