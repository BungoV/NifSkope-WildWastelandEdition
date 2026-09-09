"""Prepend the lane OFFSCREEN entry to WW_CHANGES.md.

The file is MIXED (older entries CRLF, the newest ones LF) and stays so: the
splice is binary, the new text is LF like its neighbours at the top, and the CR
count is asserted unchanged.
"""
import sys

DOC = "WW_CHANGES.md"
ENTRY = "scratchpad/offscreen_20260909/changelog_entry.md"
ANCHOR = b"# NifSkope \xe2\x80\x94 Wild Wasteland Edition: Change Log\n\n"

b = open(DOC, "rb").read()
new = open(ENTRY, "rb").read()

assert b.count(ANCHOR) == 1, "anchor matches %d times" % b.count(ANCHOR)
assert new.count(b"\r") == 0, "the new entry must be LF, like the entries it joins"
cr_before = b.count(b"\r")

out = b.replace(ANCHOR, ANCHOR + new)
assert out.count(b"\r") == cr_before, "CR count moved: %d -> %d" % (cr_before, out.count(b"\r"))
assert len(out) == len(b) + len(new)

open(DOC, "wb").write(out)
print("WW_CHANGES.md %d -> %d bytes, CR %d unchanged" % (len(b), len(out), cr_before))
