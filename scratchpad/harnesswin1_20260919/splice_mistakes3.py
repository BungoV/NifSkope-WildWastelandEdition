#!/usr/bin/env python3
"""Byte-splice two more HARNESSWIN1 entries into the top of MISTAKES.md.

Same contract as splice_mistakes2.py: MISTAKES.md is CRLF throughout and
newest-at-top, the entries go immediately above the phase-2 block, the splice
is asserted purely additive and refuses if CR and LF do not move together.
--check is the default; --apply is required to write.
"""

import io
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
PATH = os.path.join(ROOT, "MISTAKES.md")
CRLF = chr(13) + chr(10)

NEW_MARK = "## 2026-09-19 -- HARNESSWIN1 -- my own refusal refused a run that asked for nothing"
ANCHOR = "## 2026-09-19 -- HARNESSWIN1 -- the hook-up wired the repair and not the record"

ENTRY_LINES = [
    NEW_MARK,
    '',
    'The repair prints REFUSED when the window it obtained is not the window that',
    'was asked for. `tests/spells/render_shot.sh` then started printing',
    '',
    '    REFUSED ... asked for 1024x904 ... came out 1822x560',
    '',
    'on a run that had asked for nothing at all. `render_shot.sh` sets no',
    '`WW_RENDER_SIZE`; the 1024x904 in that sentence was MY OWN computed default,',
    'and the shot path has a different default of its own (1280x800). The refusal',
    'was comparing my guess against somebody else\'s guess and calling the',
    'disagreement a defect.',
    '',
    'A neighbour lane reading that line would have spent an afternoon on it. It is',
    'the exact failure mode the refusal was built to prevent, produced by the',
    'refusal.',
    '',
    '**Only an EXPLICIT request can be refused.** A default is not a promise, and a',
    'run that asked for nothing cannot be told it did not get what it asked for. The',
    'refusal is now gated on `askedSizeExplicit()` -- true only when `WW_WINDOW_SIZE`',
    'or `WW_RENDER_SIZE` was actually set by the caller. Verified live afterwards on',
    '`render_shot.sh`: `asked=1024x904(default) window=1822x904`, no FLOORED, no',
    'REFUSED, 82 checks 0 failures.',
    '',
    'The general shape, and it is worth carrying: when you add a new voice to a',
    'shared log, every gate in the tree is now its audience. Run the neighbours',
    'before believing the voice only speaks when spoken to.',
    '',
    '## 2026-09-19 -- HARNESSWIN1 -- "byte-identical" was not a deterministic question',
    '',
    'Row (c) of `tests/spells/harness_window.sh` exported bungo\'s registry key',
    'before and after a read-only control run and asserted the two exports `cmp`',
    'equal. It PASSED at 14:47 and FAILED at 14:58 on the same two binaries with no',
    'edit in between.',
    '',
    'The cause is a third unguarded settings writer: `src/nifskope.cpp:8238`, inside',
    '`setCurrentFile()`, rewrites `File/Recent File List` with no `WW_` guard and',
    'outside `saveUi()`, so `saveUi()`\'s guard does not cover it. Every harness run',
    'that opens a file rewrites that list.',
    '',
    'What made it intermittent rather than simply red: a standalone reproduction gave',
    'before 808842 bytes, after 808842 bytes, and an EMPTY diff. Both results are',
    'honest. The writer only changes the export when the list ORDER changes, and',
    'opening the same fixture twice leaves it already at the head. So the row passed',
    'whenever the previous run happened to be the same file and failed whenever it',
    'was not.',
    '',
    '**A whole-file compare is a compare of everything in the file, including things',
    'nobody promised.** The promise was about window geometry. The row now asserts',
    'exactly that -- his `Window Geometry` and `Window State` lines unchanged, which',
    'is deterministic -- and PRINTS whatever else moved, by name, with the file and',
    'line of the writer, instead of swallowing it or failing on it. A gate that goes',
    'red on something it never promised teaches the next reader to ignore it.',
    '',
    'Two smaller things fell out of this worth keeping:',
    '',
    '* The first attempt at a fix was to make the row tolerant. That is the wrong',
    '  move and it was backed out: tolerance hides the writer. Naming it costs four',
    '  lines of diff output and leaves the defect visible every run.',
    '* "It passed" and "it failed" fifteen minutes apart on one binary is not noise',
    '  to be re-run until green. It is the measurement telling you the question is',
    '  wrong. Reproduce it standalone before touching the row -- the standalone run',
    '  here DISAGREED with the gate, and that disagreement is what located the cause.',
    '',
]


def main():
    with io.open(PATH, "rb") as fh:
        raw = fh.read()
    before_cr, before_lf, before_len = raw.count(b"\r"), raw.count(b"\n"), len(raw)

    if raw.count(NEW_MARK.encode("utf-8")):
        print("ALREADY APPLIED: the HARNESSWIN1 phase-2b entries are present. Nothing written.")
        return 0
    at = raw.find(ANCHOR.encode("utf-8"))
    if at < 0:
        print("REFUSED: the phase-2 HARNESSWIN1 block is not in MISTAKES.md -- nothing to splice above")
        return 1

    entry = (CRLF.join(ENTRY_LINES)).encode("utf-8")
    new = raw[:at] + entry + raw[at:]

    added_cr = new.count(b"\r") - before_cr
    added_lf = new.count(b"\n") - before_lf
    print("MISTAKES.md  CR %d -> %d (+%d)  LF %d -> %d (+%d)  bytes %d -> %d"
          % (before_cr, new.count(b"\r"), added_cr,
             before_lf, new.count(b"\n"), added_lf, before_len, len(new)))
    if added_cr != added_lf:
        print("REFUSED: CR and LF did not move together -- a line ending was mangled")
        return 1
    if new[at:at + len(entry)] != entry or new[at + len(entry):] != raw[at:]:
        print("REFUSED: the splice is not purely additive")
        return 1

    if "--apply" not in sys.argv:
        print("--check only. Nothing written.")
        return 0
    with io.open(PATH, "wb") as fh:
        fh.write(new)
    print("wrote MISTAKES.md (append-only, %d bytes inserted at offset %d)"
          % (len(entry), at))
    return 0


if __name__ == "__main__":
    sys.exit(main())
