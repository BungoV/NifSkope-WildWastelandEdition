#!/usr/bin/env python3
"""HARNESSWIN2 phase-2 MISTAKES entries: three things the RUN refuted.

Spliced above the lane's own phase-1 block so the newest sits nearer the top.
CRLF throughout, purely additive, refuses if CR and LF do not move together.
--check is the default.
"""

import io
import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
PATH = os.path.join(ROOT, "MISTAKES.md")
CRLF = chr(13) + chr(10)

NEW_MARK = "## 2026-09-19 -- HARNESSWIN2 -- I ran an UNPATCHED binary against his real settings to bisect"
ANCHOR = "## 2026-09-19 -- HARNESSWIN2 -- I counted baselines with a glob and got it wrong twice"

ENTRY_LINES = [
    NEW_MARK,
    '',
    'This lane exists because harness runs were editing bungo\'s Recent File List.',
    'The repair landed, the gate\'s row (f) went green, and the list was proved',
    'byte-identical after the gate and after `native_open.sh`. Then `native_lighting.sh`',
    'came back with three failures and I wanted to know whether they were mine, so I',
    'ran the same spell twice more on RUNG binaries -- `NifSkope.before_harnesswin2.exe`',
    'and `NifSkope.before_cellview1.exe`.',
    '',
    'Those binaries do not have the guard. That is the whole point of a rung. Both',
    'runs opened three files and **edited his real list**: three entries in, three',
    'off the end.',
    '',
    '    before  882 B  sha1 2cb80e7aaa0366ea   (saved read-only beside this lane)',
    '    after   949 B  sha1 07f3cec4eec0a86c',
    '    added   showcase1_20260912/out/look/obj/Commonwealth.4.-20.24.BTO',
    '            showcase1_20260912/out/look/obj/Commonwealth.4.-20.24.BTR',
    '            showcase1_20260912/out/lodl/Terrain/Commonwealth.lodl',
    '    lost    .../tmp.vV8ZewlHvG/cube512.nif',
    '            .../Landscape/Trees/TreeMapleblasted05.nif',
    '            .../impostorfix3_20260919/fixture/blast_n4/cards/000531b3_oct.lodm',
    '',
    '`tests/spells/harness_window.sh` gets this right: every run of the rung goes',
    'through `run_exe`, which sets `WW_SETTINGS_SCOPE` first, so the red control can',
    'never reach him. I knew that -- I wrote the row -- and then ran the OTHER spell',
    'by hand with only `EXE=` overridden, because to my eye I was "just re-running a',
    'gate on an older exe".',
    '',
    '**A rung binary is an UNREPAIRED binary, and every defect the repair removes is',
    'live in it.** So the isolation cannot live in the exe; it has to come from the',
    'environment the runner sets. The rule that follows, and the only one that would',
    'have stopped this:',
    '',
    '    EXE=<a rung> is never written without WW_SETTINGS_SCOPE=<something> beside it.',
    '',
    'Two smaller things this also settles. A spell that takes `EXE=` but has no scope',
    'knob is only safe on the current build, and should say so in its head comment --',
    '`native_lighting.sh` does not. And "his key came back byte-identical" is a claim',
    'about the whole session, not about one run: I had checked it after the gate and',
    'after `native_open.sh`, both clean, and then stopped checking exactly when the',
    'runs stopped being safe.',
    '',
    'What was NOT damaged, checked read-only: `Window Geometry` and `Window State`',
    'are byte-identical, and a final `native_lighting.sh` on the repaired exe left',
    'the list untouched (949 B / 07f3cec4, twice) -- which is also the cleanest proof',
    'the guard works on the one spell that had been writing it.',
    '',
    '## 2026-09-19 -- HARNESSWIN2 -- a "grep the log is empty" proof rule on an append log',
    '',
    'The re-base plan this lane wrote gave three tests for "this shot moved by size',
    'only". Test (2) was: `grep FLOORED release/ww_harness_window.log` is empty after',
    'the repair and was not before. The first run after the repair printed 3.',
    '',
    'The log is not a verdict, it is a TRACE. `wwHarnessWrite()`',
    '(`src/harnesswindow.cpp:471`) truncates on a process\'s first write and appends',
    'after that, and the recorder re-records on every resize and state change, so one',
    'correct run writes the window\'s whole convergence:',
    '',
    '    shown   asked=1024x1024 window=1741x1024 viewport=857x954  FLOORED',
    '    settled asked=1024x1024 window=1822x1024 viewport=857x954  FLOORED',
    '    settled asked=1024x1024 window=1822x1024 viewport=1822x989 FLOORED',
    '    settled asked=1024x1024 window=1024x1024 viewport=1024x989',
    '',
    'Three FLOORED lines and a clean landing, from the run whose picture is right.',
    'The gate itself never made this mistake -- its `logline` takes the LAST line,',
    'and the comment above it says why in so many words. The plan I wrote for the',
    'director reached for `grep -c` instead, on the same file, in the same hour.',
    '',
    '**A count over an append-only trace measures history, not state.** Ask the trace',
    'for its LAST record about the run in question, or have the writer emit a verdict',
    'line that exists once per run. And when the file already has a reader that does',
    'it correctly, use that reader rather than a fresh grep.',
    '',
    '## 2026-09-19 -- HARNESSWIN2 -- I predicted the post-repair size from the REQUEST',
    '',
    'The before/after table in the plan predicted `render_shot.sh` would go from',
    '1822x445 to **640x445**, because it asks for 640x480 and a shot is width x',
    '(height - 35). It came out **857x445**.',
    '',
    '857 is not wrong and not a surprise once measured: it is the window\'s layout',
    'minimum with the docks already hidden, and the gate\'s own row (e) prints it --',
    '"measured layout minimum for this build: 857x480 (asked 640x480)". The repair',
    'removes the DOCKS from the floor. It does not remove the floor. Every shot above',
    '857 px wide now lands exactly on its request (native_open: 1024x1024, asked',
    '1024x1024, no FLOORED); every shot below it still floors, and still says so.',
    '',
    '**Removing one term from a minimum is not the same as satisfying the request.**',
    'I had measured the dock contribution and treated the remainder as zero without',
    'ever measuring it -- while the gate I wrote in the same file was measuring it on',
    'every run and printing the number. A prediction that contradicts a number your',
    'own harness already prints is not a prediction, it is a reading you skipped.',
    '',
    'Consequence for the plan: `render_shot.sh` was listed in section 3f as a Class U',
    'control that "must come back byte-identical", and its 18 shots all changed size.',
    'Its CHECK COUNT is what is actually invariant (82 checks, 0 failures, before and',
    'after) because every row compares a pair rendered in the same run. A gate with no',
    'stored image is pinned by its counts, never by its artefacts -- I had that right',
    'in section 3b and then contradicted it eight lines later in the command list.',
    '',
    '',
]


def main():
    with io.open(PATH, "rb") as fh:
        raw = fh.read()
    before_cr, before_lf, before_len = raw.count(b"\r"), raw.count(b"\n"), len(raw)

    if raw.count(NEW_MARK.encode("utf-8")):
        print("ALREADY APPLIED. Nothing written.")
        return 0
    at = raw.find(ANCHOR.encode("utf-8"))
    if at < 0:
        print("REFUSED: the HARNESSWIN2 phase-1 block is not in MISTAKES.md")
        return 1
    if raw.count(ANCHOR.encode("utf-8")) != 1:
        print("REFUSED: the anchor is not unique")
        return 1

    entry = (CRLF.join(ENTRY_LINES)).encode("utf-8")
    new = raw[:at] + entry + raw[at:]

    added_cr = new.count(b"\r") - before_cr
    added_lf = new.count(b"\n") - before_lf
    print("MISTAKES.md  CR %d -> %d (+%d)  LF %d -> %d (+%d)  bytes %d -> %d"
          % (before_cr, new.count(b"\r"), added_cr,
             before_lf, new.count(b"\n"), added_lf, before_len, len(new)))
    if added_cr != added_lf:
        print("REFUSED: CR and LF did not move together")
        return 1
    if new[at:at + len(entry)] != entry or new[at + len(entry):] != raw[at:]:
        print("REFUSED: the splice is not purely additive")
        return 1

    if "--apply" not in sys.argv:
        print("--check only. Nothing written.")
        return 0
    with io.open(PATH, "wb") as fh:
        fh.write(new)
    print("wrote MISTAKES.md (%d bytes inserted at offset %d)" % (len(entry), at))
    return 0


if __name__ == "__main__":
    sys.exit(main())
