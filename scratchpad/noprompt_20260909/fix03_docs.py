#!/usr/bin/env python3
"""NOPROMPT 2026-09-09: the two ledger entries.

WW_CHANGES.md is MIXED (19020 CRLF of 22869) but its newest-first head is LF-only,
which is where the entry goes, so the entry is LF and the CR count must not move.
MISTAKES.md is LF-only throughout.
"""
import sys

WW = "WW_CHANGES.md"
MI = "MISTAKES.md"

ww_entry = """## 2026-09-09 - Headless runs no longer stop on the Save Confirmation dialog

`src/nifskope.cpp`, `src/nifskope.h`, `tests/spells/render_shot.sh`. Code written
and syntax-checked; **not built and not run** (another lane held the build slot).

bungo, verbatim: *"agents keep always hanging on save confirmation"* - with the
box reading "You have unsaved changes to TreeMapleForest3" over a card bake that
was on its way out.

**THE CAUSE IS THAT `qApp->quit()` IS NOT `exit(0)` ANY MORE.**
`QCoreApplicationPrivate::quit()` is virtual (Qt 6.11,
`QtCore/private/qcoreapplication_p.h:105`) and `QGuiApplicationPrivate` overrides
it (`QtGui/private/qguiapplication_p.h:83`) to close every top-level window before
the loop exits. Every WW_* hook ends in `qApp->quit()`, so every one of them runs
`NifSkope::closeEvent` (`src/nifskope.cpp:7420`), which asks `saveConfirm()`
(`src/nifskope_ui.cpp:28489`) - a modal question in a process with nobody to
answer it. The run sits there until its own `timeout` kills it and writes nothing.

**WHICH ROUTE LEAVES THE DOCUMENT MODIFIED.** Two, measured by reading the
writers:

| route | what it writes into the loaded model | why it is dirty |
|---|---|---|
| impostor card bake (`WW_IMPOSTOR_BAKE`) | `LOD1 Size` / `LOD2 Size` = 0, and `Flags\\|1` on every `_L1`/`_L2` shape (`src/nifskope_ui.cpp:21476-21497`) | bungo's exact case: `TreeMapleForest3` is the maple whose near mesh carries those in-cell detail steps |
| generated terrain documents (`.btd`, `.lodt`) | the whole document, which is BUILT, not parsed (`src/nifskope.cpp`, the load chain) | building it fires `NifModel::dataChanged`, wired to `setWindowModified` in the constructor - so a terrain document was born dirty |

`WW_RENDER_SHOT` and `WW_LOD_CHANNEL` write nothing into the model themselves; a
plain render of a .nif was only ever caught by the second row above, or by
whatever the harness around it had edited.

**THE FIX IS ONE PREDICATE AND ONE GUARD.** `NifSkope::wwHeadlessRun()` is the
predicate, and it is deliberately the SAME test `saveUi()` has made since
2026-07-27: any environment variable named `WW_*` (plus `-no-gui`, which never
builds a window anyway). No second list of switches to keep in step. The guard
is at the top of `closeEvent`, and it sets STATE rather than branching, so the
whole close path below it - this window's `saveConfirm()`, every group member's,
and the "unsaved and not on disk anywhere" background-document question - takes
the discard answer by itself. With no `WW_` variable set the block does not run
and interactive behaviour is byte-for-byte what it was.

**AND THE GENERATED DOCUMENT IS NO LONGER BORN DIRTY.** A `.btd`/`.lodt` scene
now clears and cleans its undo stack before `completeLoading`, exactly as the
starter document already did. That is the honest state: nobody edited anything,
and `NifSkope::save()` refuses to write those suffixes back anyway.

**A DISCARD IS WRITTEN DOWN.** The dialog left a trace on screen; a discard
leaves none, and "the harness did not hang" passes just as well on a document
that was never dirty. So the guard names every document it actually decided for
in `release/ww_headless_close.log` (and on `qInfo`), and writes no file at all
when there was nothing to discard. That pair is what the gate fails on in both
directions.

**THE GATE** is `tests/spells/render_shot.sh`, new: `-no-gui new --cube` and one
`set -f Name` derive two fixtures from the application itself - no game corpus,
nothing hand-authored - differing only in whether one shape is named `*_L1`.
Three runs (plain render, plain bake, dirty bake) are each asserted on exit code
0, on wall clock well inside the cap, on their own output file, and on the
discard log: absent for the two clean cases, naming `cube_lod` for the dirty one.
The bake's own `hidden` sidecar line is asserted too, so the dirty case cannot
pass by quietly having been clean. On the old code all three fail on the timeout.

Not measured: nothing here has been built or run. The mechanism above is read
from the sources and from Qt's headers, and the numbers the gate prints do not
exist yet.

"""

mi_entry = """## 2026-09-09 -- `git diff --numstat` was read as this lane's diff in a shared tree

- **Lane NOPROMPT patched `src/nifskope.cpp` and read `git diff --numstat` to
  check the change was small: 152 added, 2 deleted, against a patch that had
  reported adding 103 lines.** The extra 49 were not this lane's: the working
  tree already carried another lane's uncommitted edits to the same file, so
  numstat was measuring the TREE against HEAD, not the lane against its own
  starting point. Found by the two numbers disagreeing and checking
  `git status --porcelain`, which listed forty-odd modified files. The real
  figure is +103/-0, from `diff` against a copy taken before patching.
  Rule: in a shared worktree, a lane measures its own diff against its own
  backup of the file, never against HEAD; and it takes that backup BEFORE the
  first patch script runs.

## 2026-09-09 -- a build gate was checked before the point the brief put it at

- **Lane NOPROMPT was told to check `scratchpad/images_20260909/DONE` and the
  process list ONCE, after the code and the harness were written, and never to
  poll. It checked `DONE` while first listing `scratchpad/`, ~40 minutes early.**
  Harmless here (the answer was the same both times, and no build was started),
  but it is the first half of polling, and a gate read early is a gate that
  invites being read again. Found while re-reading the brief before the real
  check. Rule: a gate is read at the step the brief puts it at, and a directory
  listing is not an excuse to read one that is not due.

"""


def splice(path, marker, entry, expect_cr):
    with open(path, "rb") as f:
        b = f.read()
    cr0 = b.count(b"\r")
    if cr0 != expect_cr:
        sys.exit("%s CR is %d, expected %d" % (path, cr0, expect_cr))
    n = b.count(marker)
    if n != 1:
        sys.exit("%s marker appears %d times" % (path, n))
    out = b.replace(marker, marker + entry.encode("utf-8"), 1)
    if out.count(b"\r") != cr0:
        sys.exit("%s picked up a CR" % path)
    with open(path, "wb") as f:
        f.write(out)
    print("%s +%d lines, CR unchanged at %d" % (path, entry.count("\n"), cr0))


splice(WW, "# NifSkope — Wild Wasteland Edition: Change Log\n\n".encode("utf-8"),
       ww_entry, 19020)
splice(MI, b"Newest at the top.\n\n", mi_entry, 0)
