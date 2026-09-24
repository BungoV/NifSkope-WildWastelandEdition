# Splice the two pending lane changelog entries into WW_CHANGES.md, newest first,
# in BINARY. WW_CHANGES.md is a MIXED file (CONSTITUTION 8): the head of the file
# is LF, every recent entry is LF, and the CR count must not move by one.
import os, sys
os.chdir(r"E:\Projects\NifskopeWildWastelandEdition")

TARGET = "WW_CHANGES.md"
TITLE = b"# NifSkope \xe2\x80\x94 Wild Wasteland Edition: Change Log\n\n"

# newest first: HKX1 landed ~06:1x, WATER5 ~05:5x. native0's entry was already
# spliced by lane BUILD6 (heading at line 53, reworded "built into the exe").
ENTRIES = [
 ("scratchpad/hkx1_20260910/WW_CHANGES_ENTRY.md",
  b"**Build state, 2026-09-10 (lane DOCS2 checked it):** `release/NifSkope.exe` is\n"
  b"03:57:46 and `src/hkxanim.cpp` is 05:10:14, so the reader is NOT in the built\n"
  b"exe; `NifSkope.pro` already names both files (lines 187 and 312), so the resume\n"
  b"is `qmake` + `make`, nothing else. `release/hkxanim_dump.exe` (05:10:22) is the\n"
  b"standalone driver every gate above was actually run on.\n\n"),
 ("scratchpad/water5_20260910/WW_CHANGES_ENTRY.md",
  b"**Build state, 2026-09-10 (lane DOCS2 checked it):** `release/NifSkope.exe` is\n"
  b"03:57:46 and the four new sources are 04:50:17 - 05:04:17, so none of them is in\n"
  b"the built exe; `NifSkope.pro` does NOT yet name `src/watercurves.*` or\n"
  b"`src/waterwindow.*` (hook-up H1 unapplied), so the resume is\n"
  b"`scratchpad/water5_20260910/PENDING.md` in full, after WATER4's build.\n\n"),
]

b = open(TARGET, "rb").read()
cr_before, lf_before, n_before = b.count(b"\r"), b.count(b"\n"), len(b)
assert b.startswith(TITLE), "the title block is not what this splice expects"

block = b""
for path, status in ENTRIES:
    e = open(path, "rb").read()
    assert e.count(b"\r") == 0, path
    if not e.endswith(b"\n"):
        e += b"\n"
    # heading line, then the dated build-state paragraph, then the lane's text
    j = e.index(b"\n") + 1
    head, body = e[:j], e[j:]
    assert head.startswith(b"## "), path
    body = body.lstrip(b"\n")
    block += head + b"\n" + status + body + b"\n"
    print("prepared %-52s %d bytes" % (path, len(e)))

out = TITLE + block + b[len(TITLE):]
assert out.count(b"\r") == cr_before, "CR count moved: %d -> %d" % (cr_before, out.count(b"\r"))
open(TARGET, "wb").write(out)
print("CR  %d -> %d   (unchanged: %s)" % (cr_before, out.count(b"\r"), cr_before == out.count(b"\r")))
print("LF  %d -> %d   (+%d)" % (lf_before, out.count(b"\n"), out.count(b"\n") - lf_before))
print("bytes %d -> %d (+%d)" % (n_before, len(out), len(out) - n_before))
