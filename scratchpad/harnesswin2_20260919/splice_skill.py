#!/usr/bin/env python3
"""Append section 11 to ww-test-harness-add/SKILL.md in BOTH trees.

Both copies are LF-only and must end byte-identical. Purely additive; refuses
if the two copies differ before the splice, or if any CR appears.
--check is the default.
"""

import io
import os
import sys

LF = chr(10)
COPIES = [
    r"E:\Projects\NifskopeWildWastelandEdition\.claude\skills\ww-test-harness-add\SKILL.md",
    r"E:\Projects\Claude\.claude\skills\ww-test-harness-add\SKILL.md",
]

MARK = "## 11. Proving a repair that changes the WINDOW every gate is photographed in (2026-09-19, lane HARNESSWIN2)"

SECTION = LF.join([
    '',
    MARK,
    '',
    'A repair to the harness window -- the dock order, a persisted-geometry guard,',
    'anything that changes how big the framebuffer comes out -- is not one gate\'s',
    'business. It moves the picture under EVERY spell that takes a `WW_RENDER_SHOT`.',
    'The work is almost entirely in deciding, before the build, what each of those',
    'spells is allowed to do afterwards. The procedure that worked:',
    '',
    '**Sort the spells by what they actually COMPARE, not by what they write.** Three',
    'kinds, and only the first can be "re-based":',
    '',
    '* **A stored baseline.** An image on disk that a later run is diffed against',
    '  (`tests/baselines/<gate>/`). Find these by asking for the CONSUMER, not the',
    '  files: `grep -rn "baseline\\|golden\\|expect" tests/ tools/ --include=*.sh`.',
    '  A glob for `*.png` counts fixtures as baselines and misses every store that',
    '  lives outside the directory you globbed.',
    '* **A same-run pair.** The gate renders A and B in one run and compares them to',
    '  each other, or reads the size back out of the PNG it just wrote. Its artefacts',
    '  may all change size without a single number moving. **Pinned by its COUNTS.**',
    '* **An independent source.** A census, a plugin walk, a known-answer table. The',
    '  window cannot reach it at all.',
    '',
    '**Take the census BEFORE the build**: every shot\'s path, width, height and sha1',
    'into a file. Afterwards take it again and diff. Then, per line:',
    '',
    '* size unchanged + sha1 unchanged -> the repair did not reach it. Expected for',
    '  any path that already sized itself correctly, and for any grab site the edit',
    '  deliberately did not touch -- which is how you prove the edit was scoped.',
    '* **size unchanged + sha1 CHANGED -> STOP.** That is a rendering change wearing',
    '  a window change\'s clothes, and re-basing it would bury the evidence.',
    '* size changed -> read the new size against the gate\'s own settled log line and',
    '  against the request, and do not assume which one it will match (below).',
    '',
    '**The floor you removed is not the only floor.** Hiding the docks takes the',
    'docks out of the layout minimum; the rest of the layout is still in it. On this',
    'machine the window went from a 1822 px minimum to 857 px, so a gate asking 1024',
    'now lands exactly and a gate asking 640 still floors -- and still says FLOORED,',
    'which is the honest outcome, not a miss. Have the harness PRINT the measured',
    'minimum ("measured layout minimum for this build: 857x480") and read that number',
    'rather than predicting the new size from the request.',
    '',
    '**Read a trace\'s LAST record, never a count of its lines.** The window recorder',
    'writes on every resize and state change, so one correct run logs its whole',
    'convergence and several of those lines legitimately carry the refusal word. A',
    '`grep -c` over that file measures history. If the file already has a reader that',
    'takes the last line, use it.',
    '',
    '**A rung binary is an UNREPAIRED binary.** Every red control, and every "was it',
    'me or was it already broken" bisect, runs a build in which the defect is live.',
    'If the defect is that runs write the user\'s settings, then the bisect writes the',
    'user\'s settings. The isolation has to come from the environment, never from the',
    'exe: `EXE=<a rung>` is never written without `WW_SETTINGS_SCOPE=<something>`',
    'beside it, and a spell that offers `EXE=` but no scope knob says in its head',
    'comment that it is only safe on the current build.',
    '',
    '**A gate that goes from 8 failures to 3 is a result, not a failure.** Run the',
    'neighbour on the rung as well (in a scope) and put the two logs side by side:',
    'rows that fail identically on both binaries, with identical values, are not',
    'yours, and saying so with the pair of numbers is worth more than a green gate.',
    'Bisect one exe further back before naming a lane.',
    '',
])


def main():
    bodies = []
    for p in COPIES:
        with io.open(p, "rb") as fh:
            bodies.append(fh.read())
    for p, b in zip(COPIES, bodies):
        print("%-70s %7d bytes  CR %d  LF %d" % (p, len(b), b.count(b"\r"), b.count(b"\n")))
    if bodies[0] != bodies[1]:
        print("REFUSED: the two copies differ BEFORE the splice -- reconcile them first")
        return 1
    if bodies[0].count(b"\r"):
        print("REFUSED: this file was LF-only and is not any more")
        return 1
    if bodies[0].count(MARK.encode("utf-8")):
        print("ALREADY APPLIED. Nothing written.")
        return 0

    add = SECTION.encode("utf-8")
    if b"\r" in add:
        print("REFUSED: the new section carries a CR")
        return 1
    new = bodies[0] + add
    print("after: %d bytes (+%d), LF %d (+%d)"
          % (len(new), len(add), new.count(b"\n"), new.count(b"\n") - bodies[0].count(b"\n")))
    if new[:len(bodies[0])] != bodies[0]:
        print("REFUSED: not purely additive")
        return 1

    if "--apply" not in sys.argv:
        print("--check only. Nothing written.")
        return 0
    for p in COPIES:
        with io.open(p, "wb") as fh:
            fh.write(new)
    back = [io.open(p, "rb").read() for p in COPIES]
    print("wrote both copies; identical after: %s (%d bytes)"
          % (back[0] == back[1], len(back[0])))
    return 0 if back[0] == back[1] else 1


if __name__ == "__main__":
    sys.exit(main())
