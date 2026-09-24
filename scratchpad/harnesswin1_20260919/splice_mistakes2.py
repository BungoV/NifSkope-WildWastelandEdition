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
    '## 2026-09-19 -- HARNESSWIN1 -- the hook-up wired the repair and not the record',
    '',
    '`scratchpad/harnesswin1_20260919/hookup.py` applied seven edits, `--check`',
    'reported every one at exactly 1 hit, the build was clean, and',
    '`tests/spells/harness_window.sh` still reported 8 failures out of 9 checks --',
    'every one of them on an empty string.',
    '',
    'Nothing in the tree called `wwHarnessRecordViewport()`. The new file defined it;',
    'the hook-up never placed a call to it, so `release/ww_harness_window.log` was',
    'never created and every row that read a field out of that log read "".',
    '',
    'The reason it was not noticed is that the two halves feel like one change. The',
    'REPAIR (do not restore the persisted geometry; force the size) and the RECORD',
    '(write down the size that was actually obtained) were designed together, live in',
    'one file, and were described in one paragraph of the brief -- so seven green',
    'anchor checks read as "the change is in". They were seven checks on the repair.',
    '',
    'The check that would have caught it costs one line and does not need a build:',
    '',
    '    grep -rn "wwMyNewFunction" src/ | grep -v "<the file that defines it>"',
    '',
    'Run it for EVERY public function a new translation unit exports, after',
    '`--apply` and before `make`. A function with no caller is a compile-time',
    'success and a run-time absence, and `make` has nothing to say about it.',
    '',
    'The same run also showed the second half of the lesson: a harness that cannot',
    'read back what it measured does not fail loudly, it fails as "0" -- the field',
    'extractor returns an empty string, the comparison is false, and the row reports',
    'a defect in the code under test. Eight rows accused the repair of being broken',
    'when what was broken was the wiring of the measurement.',
    '',
    '## 2026-09-19 -- HARNESSWIN1 -- a red control that could not see its own input',
    '',
    'Row (c) of `tests/spells/harness_window.sh` plants a maximized window geometry',
    'in an ISOLATED settings scope and runs the rung exe',
    '(`release/NifSkope.before_harnesswin1.exe`) to watch the old path floor.',
    '',
    'It reported `maximised=0` and went red -- and the reason had nothing to do with',
    'the defect. The isolation is `WW_SETTINGS_SCOPE`, and `WW_SETTINGS_SCOPE` is',
    "part of THIS lane's change. The rung exe predates it by definition. It ignored",
    "the variable, read bungo's real key instead, and was measured against an input",
    'it had never been given.',
    '',
    '**A control built out of the change it is meant to control is not a control.**',
    'Before writing a red row, ask what the OLD binary can actually see: if the',
    'answer involves anything the lane added, the row is measuring the new code',
    'twice.',
    '',
    "The row now runs the rung exe against the settings it can see -- bungo's own,",
    'strictly read-only, exported and compared whole before and after -- because',
    'those are the settings that produced the 0.8978 in the first place, and it',
    'SKIPS by name if his saved window is not maximized on the day, rather than',
    'passing.',
    '',
    '## 2026-09-19 -- HARNESSWIN1 -- `reg.exe export ... /y` is silently not an export',
    '',
    '`export_key()` ran `reg.exe export "$key" "$file" /y >/dev/null 2>&1`. Under',
    'MSYS2/Git-Bash the lone-slash argument is rewritten as a path before `reg.exe`',
    'sees it; `reg.exe` answers `ERROR: Invalid syntax.` with rc=1, the redirect',
    'swallows it, and no file is written.',
    '',
    'Every comparison built on that function then compared two absent files. `cmp -s`',
    'on two missing files is not a pass, but the row that says "his settings are',
    'byte-identical" would have been answered by a function that had never read them.',
    '',
    '`//y` gives rc=0 and an 809,612-byte export. Measured both ways directly rather',
    'than reasoned about.',
    '',
    'The general form: **a helper whose output is redirected to /dev/null must have',
    'its result asserted, not assumed.** `export_key` now ends in `test -s "$2"` and',
    'the caller prints the byte count.',
    '',
    '## 2026-09-19 -- HARNESSWIN1 -- a check that demanded the impossible',
    '',
    'Row (e) asked for a 640x480 window and required `window=640x480`. It went red.',
    'The window came out 1024x480, and it will come out 1024-something for any',
    'request narrower than that, on any build, because the block list, the details',
    'tree and the docks have a layout minimum and Qt does not make a window narrower',
    'than its layout minimum for anybody.',
    '',
    '`tests/spells/native_open.sh:117-119` had already written this down --',
    '"the main window has a minimum WIDTH of about 1024 px, so a narrower request is',
    'silently floored (480x480 came back 1024x445)" -- and the row was written',
    'without reading it.',
    '',
    'A check that a correct build cannot pass is not a floor, it is a second defect',
    '(ww-test-harness-add 5b). The row now checks the thing the repair actually',
    'promises, which is the more useful question anyway: **the floor is not silent.**',
    'Ask for something unobtainable and the run must SAY it was floored, in one line,',
    'carrying both the number asked for and the number obtained.',
    '',
]


def main():
    with io.open(PATH, "rb") as fh:
        raw = fh.read()
    before_cr, before_lf, before_len = raw.count(b"\r"), raw.count(b"\n"), len(raw)

    marker = ("## 2026-09-19 -- HARNESSWIN1 -- a hook-up script reported").encode("utf-8")
    at = raw.find(marker)
    if at < 0:
        print("REFUSED: the phase-1 HARNESSWIN1 entry is not in MISTAKES.md -- nothing to splice above")
        return 1
    if raw.count(b"## 2026-09-19 -- HARNESSWIN1 -- the hook-up wired the repair"):
        print("ALREADY APPLIED: the HARNESSWIN1 phase-2 entries are present. Nothing written.")
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
