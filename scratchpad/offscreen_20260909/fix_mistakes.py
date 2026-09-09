"""Prepend the lane OFFSCREEN entries to MISTAKES.md (LF-only file, newest at top)."""
DOC = "MISTAKES.md"
ENTRY = "scratchpad/offscreen_20260909/mistakes_entry.md"
ANCHOR = b"Newest at the top.\n\n"

b = open(DOC, "rb").read()
new = open(ENTRY, "rb").read()

assert b.count(ANCHOR) == 1, "anchor matches %d times" % b.count(ANCHOR)
assert b.count(b"\r") == 0, "MISTAKES.md was LF-only; it is not any more"
assert new.count(b"\r") == 0, "the new entry must be LF"

out = b.replace(ANCHOR, ANCHOR + new)
assert out.count(b"\r") == 0
assert len(out) == len(b) + len(new)
open(DOC, "wb").write(out)
print("MISTAKES.md %d -> %d bytes, CR 0" % (len(b), len(out)))
