#!/usr/bin/env python3
# BUILD2: the finished-work skill review (CONSTITUTION 1a), appended to both
# lane reports. LF-only files.

import sys

BASE = "E:/Projects/NifskopeWildWastelandEdition/scratchpad/"

TEXT = """
### Finished-work skill review (BUILD2)

**Loaded and used.** `nifskope-ww-build-verify` -- the gated chain, make's own
exit code, the stylesheet copy, and above all *"A successful build is not a
consistent one"*, which is why `qmake` ran first and the three dependencies were
read back per object. `nifskope-ww-lodgen` -- the CLI table (`--lodl`,
`--verify-only`, the worldspace IDs) and the editing traps; every patch here was
a script file with an anchor count and a CR assert, never a heredoc.
`nifskope-ww-render-shot` -- the switches, and its off-screen section, which this
build disproved.

**Amended, in the LIVE tree** `E:\\Projects\\Claude\\.claude\\skills`:
`nifskope-ww-render-shot`. Its first section said, as fact, that a headless run
never puts a window on a screen. That is false as built, and the skill is what
every lane in this repo reads before a bake. It now opens with what is TRUE as
built -- the fix is inert, the strobe is live, and the one-line change that does
move the window off screen stops rendering altogether -- and its front-matter
description no longer advertises the off-screen behaviour. No repo-tree copy of
that skill exists, so there was nothing to reconcile.

**Written:** `nifskope-ww-resume-pending` -- "build and gate a BUILD PENDING
lane". Two lanes have now done this exact job in one day (BUILD1, BUILD2) and
both re-derived the same steps from memory: the PENDING/`*_CHANGE_NEEDED` read
order, applying another lane's owed change at equal byte length, qmake before
make with the dependency read BACK per object (the `awk` walk, because `grep -A3`
misses a dependency ten continuation lines down), the exe-newer sweep over every
changed file rather than the one you edited, the sequential harness chain with
its summary echo, and the four documents that have to stop saying "not built".
It also carries the rule this lane learned the expensive way: **a build lane that
finds a design failure measures the cause and stops.** Written to the live tree
and copied to the repo tree `.claude/skills/`, because a lane whose cwd is this
repo reads only the second.

**Declined, with the reason.** A skill for "compare two window placements from
`ww_headless_windows.log`" -- one file, four lines, and the reading of it is now
a paragraph in `nifskope-ww-render-shot` where a lane will actually meet it. And
a skill for the `--verify-only` sweep over bungo's five installed worldspaces:
the command is one line in `nifskope-ww-lodgen` already, and the only thing this
lane added -- that the mod folder itself is the `--lodl` argument, because the
writer appends `Terrain/` -- belongs beside that line rather than in a document
of its own.
"""

for rel in ("lane_offscreen_report.md", "lane_rename_report.md"):
    path = BASE + rel
    with open(path, "rb") as fh:
        b = fh.read()
    if b.count(b"\r"):
        print("ABORT: %s has CR" % rel); sys.exit(2)
    b = b + TEXT.encode("utf-8")
    with open(path, "wb") as fh:
        fh.write(b)
    print("OK %s: %d bytes, CR %d" % (rel, len(b), b.count(b"\r")))
