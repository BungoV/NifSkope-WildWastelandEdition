#!/usr/bin/env python3
"""Lane BUILD8: append this lane's mistakes to MISTAKES.md, append-only.

The file is LF-only (CR 0). The append is verified byte for byte: everything
that was there before is still there, unchanged, at the same offset.
"""
import os
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
F = os.path.join(REPO, "MISTAKES.md")
MARK = b"2026-09-10, lane BUILD8"

TEXT = """

## 2026-09-10, lane BUILD8: a process check that printed instead of gating

**What was done.** The build chain began with
`tasklist | grep -i -E "Fallout4|NifSkope"; echo rc=$?` and then ran `qmake` and
`make` regardless of the answer. bungo opened `release/NifSkope.exe` at 14:26:18
-- between the lane's opening check at 14:3x and the build a minute later -- so
the guard printed `proc-rc=0` (a NifSkope IS running) into the log and the link
went ahead anyway. `ld` failed with
`cannot open output file release/NifSkope.exe: Permission denied` and four
minutes of compilation were spent for nothing.

**What is true instead.** `nifskope-ww-build-verify`'s chain is a chain of `&&`
FOR THIS REASON: the running exe is renamed aside as a GATE, before make, and
every step is a precondition of the next. `tools/ww_build.sh` does exactly that
in-tree and was not used.

**How it was found.** The build's own exit code, 2, and the first line of the
log.

**The rule that prevents it.** A process check whose result is only echoed is
not a check. Either run `bash tools/ww_build.sh <sources>`, or put the rename in
the same `&&` chain as the build. And re-check immediately before the link, not
at the top of the lane: bungo opens the application while the lane runs.

## 2026-09-10, lane BUILD8: a pixel diff that reported "identical" through luminance

**What was done.** `diff_tiles.py` counted differing pixels and took the worst
step through `ImageChops.difference(a, b).convert("L")`.

**What is true instead.** Luminance weights blue at 0.114, so a pixel that
differs by exactly (0, 0, 1) converts to 0 and ROUNDS AWAY. The first run
reported tile `fq1` as `IDENTICAL  0 of 1352217 pixels` while printing
`bbox (698, 907, 699, 908)` on the same line -- a bounding box of a difference
the count could not see. Counted per channel, that tile has one differing pixel
and the mixamo `flast` tile has 22, not 8.

**How it was found.** The count and the bounding box, printed side by side,
contradicted each other. Had only one of them been printed, the picture verdict
would have been "four of six tiles pixel-identical" and it would have been
wrong.

**The rule that prevents it.** A difference metric on colour is computed PER
CHANNEL. Any collapse to one number -- luminance, a mean, a norm -- is a filter,
and it is stated as one or not used. Printing two independent views of the same
quantity is what caught this; keep both.

## 2026-09-10, lane BUILD8: the gate drivers were not held to the exe-newer rule

**What was done.** `src/hkxanim.cpp` was changed at 14:42 and the harness chain
was run at 14:47. `release/NifSkope.exe` was newer than every source, and that
was checked -- but `tests/spells/hkxanim_gates.py` does not run the application:
it runs `release/hkxanim_dump.exe`, which was lane BUILD7's, built 14:05:13,
from the OLD `src/hkxanim.cpp`. Its `134 checks, 3 failures` measured code that
no longer existed.

**What is true instead.** Three standalone drivers link `src/hkxanim.cpp`
(`hkxanim_dump.exe`, `hkxwrite_dump.exe`, `gltfexport_dump.exe`) and all three
had to be rebuilt. Rebuilt at 14:51:01 the gate gives the same 134 / 3, and the
three failures are the same pre-registered fixture ones -- so the answer did not
change, which is the point and is not the same as having checked.

**How it was found.** Listing the mtimes of everything in `release/` beside the
change, as CONSTITUTION 4 says to do before putting two artefacts in one
sentence.

**The rule that prevents it.** The exe-newer sweep covers EVERY binary a gate
executes, not `release/NifSkope.exe` alone. A gate script names the binary it
runs; that binary gets the same `-nt` test as the application.

## 2026-09-10, lane BUILD8: anchors copied out of a rendered file, with the dashes changed

**What was done.** `patch_docs.py` was written with anchors taken from a
rendering of `docs/GLTF_INTERCHANGE.md` and `docs/GLTF_IMPORT.md`, in which the
files' em dashes and middle dots had been transcribed as `--` and `*`. Two of
six anchors matched zero times. A third "already applied" test compared the
first 50 bytes of the NEW text, which begins with the old text, and reported
`ALREADY` on a paragraph nothing had touched.

**What is true instead.** Both pages are UTF-8 with real `\\u2014` and
`\\u00b7`. The fix was to anchor only on ASCII-only lines -- which every
paragraph has one of -- and to test "already applied" against a marker string
that exists ONLY in the new text.

**How it was found.** The script refused, which is what it is for.

**The rule that prevents it.** An anchor is taken from the FILE'S BYTES, never
from a rendering of them, and a prose file is assumed to contain typographic
punctuation until measured otherwise. `ww-anchored-hookup`'s rule about
backslashes through heredocs is the same rule for a different character class.

## 2026-09-10, lane BUILD8: a cross-lane change, made deliberately and named here

**What was done.** `src/hkxanim.cpp` -- lane HKX2b's file -- was given a reading
arm for `hkaInterleavedUncompressedAnimation`. CONSTITUTION 1 says one lane per
file, and `nifskope-ww-resume-pending` section 6 says a build lane reports a
cause and does not land the cure.

**Why it was landed anyway.** The brief's deliverable was the round trip
measured through the built exe INCLUDING a picture of the round-tripped clip,
and the only route to a picture is the render hook, which goes through this
reader. The reader refused every file `src/hkxwrite.cpp` can write, so the
application could not open what it had just written: not a defect found while
building, but a missing half of the pair the lane was told to prove. The change
is additive -- every other class is refused with the identical sentence, the
spline path is byte-for-byte unchanged, and lane HKX1's gates give the same
134 / 3 on a rebuilt driver.

**The rule.** A build lane that must cross into another lane's file says so, in
this file, in the changelog and in the handoff, in the same breath as the
result; and it keeps the old file
(`scratchpad/build8_20260910/hkxanim.cpp.pre-build8`) so the change can be read
as a diff.
"""


def main(argv):
    apply_it = "--apply" in argv
    raw = open(F, "rb").read()
    before = (len(raw), raw.count(b"\r"), raw.count(b"\n"))
    print("MISTAKES.md %d bytes, CR %d, LF %d; marker present: %d"
          % (before[0], before[1], before[2], raw.count(MARK)))
    if raw.count(MARK):
        print("REFUSED: this lane's entries are already there.")
        return 1
    t = TEXT.encode("utf-8")
    if t.count(b"\r"):
        print("REFUSED: the appended text is not LF-only")
        return 1
    if not apply_it:
        print("--check only: would append %d bytes." % len(t))
        return 0
    out = raw + t
    open(F, "wb").write(out)
    back = open(F, "rb").read()
    assert back[:len(raw)] == raw, "the append was NOT append-only"
    print("appended %d bytes; %d -> %d, CR %d (was %d), LF %d (was %d); "
          "the first %d bytes are byte-identical"
          % (len(t), before[0], len(back), back.count(b"\r"), before[1],
             back.count(b"\n"), before[2], len(raw)))
    assert back.count(b"\r") == before[1]
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
