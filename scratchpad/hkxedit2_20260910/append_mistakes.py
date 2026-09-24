"""Append lane HKXEDIT2's MISTAKES entries to the root MISTAKES.md, append-only,
byte-verified (CONSTITUTION 2; the file is LF-only: asserted before and after)."""
import os
R = r"E:\Projects\NifskopeWildWastelandEdition"
dst = os.path.join(R, "MISTAKES.md")
src = os.path.join(R, "scratchpad", "hkxedit2_20260910", "MISTAKES_ENTRIES.md")
before = open(dst, "rb").read()
entry = open(src, "rb").read()
assert before.count(b"\r") == 0, "MISTAKES.md is not LF-only: %d CR" % before.count(b"\r")
assert entry.count(b"\r") == 0
if b"lane HKXEDIT2 (the animation workspace)" in before:
    print("already appended; nothing written")
else:
    sep = b"" if before.endswith(b"\n\n") else (b"\n" if before.endswith(b"\n") else b"\n\n")
    after = before + sep + entry
    assert after.startswith(before) and len(after) == len(before) + len(sep) + len(entry)
    open(dst, "wb").write(after)
    print("appended %d bytes; %d -> %d bytes, CR %d -> %d" % (len(entry), len(before), len(after), before.count(b"\r"), after.count(b"\r")))
