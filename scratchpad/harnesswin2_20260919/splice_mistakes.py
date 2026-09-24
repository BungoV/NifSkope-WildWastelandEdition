#!/usr/bin/env python3
"""Byte-splice the HARNESSWIN2 entries into the top of MISTAKES.md.

MISTAKES.md is CRLF throughout and newest-at-top. The entries go immediately
above the newest HARNESSWIN1 block. The splice is asserted purely additive and
refuses if CR and LF do not move together (CONSTITUTION rule 8).
--check is the default; --apply is required to write.
"""

import io
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
PATH = os.path.join(ROOT, "MISTAKES.md")
CRLF = chr(13) + chr(10)

NEW_MARK = "## 2026-09-19 -- HARNESSWIN2 -- I counted baselines with a glob and got it wrong twice"
ANCHOR = "## 2026-09-19 -- HARNESSWIN1 -- my own refusal refused a run that asked for nothing"

ENTRY_LINES = [
    NEW_MARK,
    '',
    'Lane HARNESSWIN1 left a CHANGE_NEEDED that said fixing the dock order would',
    'need "a re-capture of the 5 baseline PNGs under `tests/`". Both halves of that',
    'sentence are wrong, and it was the number a director would have planned from.',
    '',
    'There are **four** baselines under `tests/` -- `tests/baselines/native_lighting/`',
    '-- and the fifth file the glob caught, `tests/fixtures/flowmap_directx_4x4.png`,',
    'is 4x4 pixels and is an INPUT to a gate, not an output of one. And the count',
    'missed `tools/render_regression/baseline/`, which holds **seven** more, because',
    'the search was `find tests -name "*.png"` and that directory is not under',
    '`tests/`.',
    '',
    'So the number was too high by one in the place it looked and too low by seven',
    'in the place it did not. **A glob over one directory is not an inventory.** The',
    'question was "what does this change break", and the honest way to ask it is',
    '"what in this tree COMPARES a rendered image against a stored one", which is a',
    'question about consumers, not about files:',
    '',
    '    grep -rn "baseline\\|golden\\|expect" tests/ tools/ --include=*.sh --include=*.ps1 --include=*.py',
    '',
    'That finds the comparer and the comparer names its store. Run that way, the',
    'answer turned out to change the whole plan: only TWO stores exist, most spells',
    'compare a pair rendered in the same run at the same size and need no re-base at',
    'all, and one of the two stores had been size-mismatched since 2026-08-11 for',
    'reasons that have nothing to do with the repair. "Twelve baselines move" would',
    'have been a day of re-capturing images that nobody compares.',
    '',
    'The smaller lesson underneath: the lane that writes a CHANGE_NEEDED is the lane',
    'that is NOT taking the change, so its blast-radius number is the one figure in',
    'the document nobody will re-measure before acting on it. Measure that one',
    'hardest, or write it as a question.',
    '',
    '## 2026-09-19 -- HARNESSWIN2 -- a comment claimed to copy the code it silently corrected',
    '',
    '`src/impostorpreviewtest.cpp:263-270` (lane IMPOSTORSHOW) documents its',
    '`forceWindow()` as doing what the shot harness does:',
    '',
    '    Not `resize()` alone: the framebuffer is what the docks and the viewport',
    '    header leave over, so both go, exactly as the WW_RENDER_SHOT harness does',
    '    it (`nifskope_ui.cpp:22086..22108`).',
    '',
    'It does not do it the way that harness does it. `forceWindow()` hides the docks',
    'and the header and THEN resizes (lines 291-295), which is correct and is why its',
    'shots come out 1024x1024 exactly. The harness it cites resizes FIRST and hides',
    'afterwards, which is the defect that held `tests/spells/native_open.sh` row (c)',
    'at covered 0.8978 for a day while two lanes looked for it somewhere else.',
    '',
    'The lane wrote the right code, noticed the ordering mattered enough to put it in',
    'the first sentence of the comment, and then described its own fix as a copy of',
    'the thing it had just corrected. Anyone reading that comment -- and HARNESSWIN1',
    'did read it -- comes away believing the two paths agree, which is exactly the',
    'belief that keeps you from diffing them.',
    '',
    '**A comment that says "exactly as X does it" is an assertion about X, and it',
    'ages in X\'s file, not in yours.** Either quote the lines being copied, or say',
    'what you do and why, and leave X out of it. If the point of the sentence is that',
    'the order matters, the sentence has to say which order X uses -- and the moment',
    'you go and look, you find out whether it is a copy or a correction.',
    '',
    'Cheap check, and it would have found this one: a cross-file "same as" comment',
    'gets the two regions printed side by side ONCE, at the time it is written.',
    '',
    '',
]


def main():
    with io.open(PATH, "rb") as fh:
        raw = fh.read()
    before_cr, before_lf, before_len = raw.count(b"\r"), raw.count(b"\n"), len(raw)

    if raw.count(NEW_MARK.encode("utf-8")):
        print("ALREADY APPLIED: the HARNESSWIN2 entries are present. Nothing written.")
        return 0
    at = raw.find(ANCHOR.encode("utf-8"))
    if at < 0:
        print("REFUSED: the HARNESSWIN1 phase-2b block is not in MISTAKES.md -- nothing to splice above")
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
