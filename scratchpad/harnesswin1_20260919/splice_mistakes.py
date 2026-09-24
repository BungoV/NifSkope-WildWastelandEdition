#!/usr/bin/env python3
"""Byte-splice one section into the top of MISTAKES.md.

MISTAKES.md is CRLF THROUGHOUT (measured: CR 9421, LF 9421) and newest-at-top,
so the entry goes after the six-line header block and before the first "## ".
Written as a script rather than as an editor edit because a heredoc and a
line-based tool both arrive with the wrong line ending in a mixed tree, and a
line-ending slip in this file is invisible in a diff (CONSTITUTION rule 8).
"""

import io
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
PATH = os.path.join(ROOT, "MISTAKES.md")
CRLF = chr(13) + chr(10)

ENTRY_LINES = [
    "## 2026-09-19 -- HARNESSWIN1 -- a hook-up script reported ALREADY APPLIED on a clean tree",
    "",
    "The refusing hook-up script (`scratchpad/harnesswin1_20260919/hookup.py`) decides",
    "whether an edit is already in place from a MARKER the inserted text carries. For an",
    "`after` edit it used the last inserted line, which is right. For a `replace` edit it",
    "used the FIRST replacement line -- and both of this lane's `replace` edits repeat the",
    "anchor as their first line on purpose, so that a reader can see the edit is additive.",
    "The marker was therefore the anchor itself, and on a completely clean tree `--check`",
    "printed ALREADY APPLIED for the `wwPlaceHeadlessWindow` edit. Applying it would have",
    "silently skipped that edit, the new code would never have been called, and the lane",
    "would have reported a green `--check` over a hook-up that does nothing.",
    "",
    "This is ww-anchored-hookup section 4 word for word -- \"a `--check` that reads ok AFTER",
    "the edit was applied, because the anchor is the line the text goes after and it still",
    "matches\" -- re-committed by the lane that had the page open. Reading the rule did not",
    "prevent it; RUNNING the script did. Fixed by taking the marker from the LAST inserted",
    "line in both modes, which is a line that exists only after the edit either way.",
    "",
    "The rule: a hook-up script's already-applied marker is a line the edit CREATES, never",
    "a line it repeats, and the script is RUN on the clean tree before it is believed --",
    "a `--check` that has not been executed is a draft, exactly like a harness.",
    "",
]


def main():
    with io.open(PATH, "rb") as fh:
        raw = fh.read()
    before_cr, before_lf, before_len = raw.count(b"\r"), raw.count(b"\n"), len(raw)

    marker = ("## 2026-09-19 -- DIRECTOR" + "").encode("utf-8")
    at = raw.find(marker)
    if at < 0:
        print("REFUSED: the first '## ' section of MISTAKES.md is not the one expected")
        return 1
    if raw.count(b"## 2026-09-19 -- HARNESSWIN1"):
        print("ALREADY APPLIED: the HARNESSWIN1 entry is present. Nothing written.")
        return 0

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
