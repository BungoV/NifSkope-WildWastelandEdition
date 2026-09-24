"""Director: RESUME3's WW_CHANGES entry REPLACES the two BUILD PENDING entries (lines 79-168) instead of stacking on top. CR must stay 19020."""
R = "E:/Projects/NifskopeWildWastelandEdition/"
ww = open(R + "WW_CHANGES.md", "rb").read()
ent = open(R + "scratchpad/resume3_20260911/WW_CHANGES_ENTRY.md", "rb").read()
assert b"\r" not in ent
k = ent.find(b"\n## 2026-09-11 -- The bake's thread-safety blocker, NAMED and fixed")
assert k > 0
entry = ent[k + 1:].rstrip(b"\n") + b"\n\n"
lines = ww.split(b"\n")
# 1-based 79..168 removed; verify the boundaries first
assert lines[78].startswith(b"## 2026-09-11 -- lane SPLAT1: the landscape textures are baked 6x too large"), lines[78][:80]
assert lines[168].startswith(b"## 2026-09-11 \xe2\x80\x94 The FO4CS \"Improved LOD\" build plan"), lines[168][:80]
removed = b"\n".join(lines[78:168]) + b"\n"
assert removed.count(b"\r") == 0, removed.count(b"\r")
assert ww.count(removed) == 1
new = ww.replace(removed, entry, 1)
assert new.count(b"\r") == ww.count(b"\r") == 19020
assert new.count(b"\n## 2026-09-11 -- The bake's thread-safety blocker") == 1
open(R + "WW_CHANGES.md", "wb").write(new)
print("WW_CHANGES.md: replaced lines 79-168 (%d B) with RESUME3 entry (%d B); total %d B" % (len(removed), len(entry), len(new)))
