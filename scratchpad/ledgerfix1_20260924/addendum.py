"""Add the commit hashes to the HANDOFF top block and the WW_CHANGES recovery entry. Read, close, assert longer, write."""
R = "E:/Projects/NifskopeWildWastelandEdition/"
HASHES = """  a4c2069 ledgers rebuilt; cea8808 .gitignore (game data, extracts, binaries,
  two groups held for bungo); 85c0b14 src + res (203 files); 8fa18e3 tests +
  tools (178); 5e4f069 docs + constitution + this repo's skills (103);
  92c068f scratchpad text (10,414 files, ~98 MB); then this ledger note.
  Held for bungo (ignored, on disk): skill copies from other projects and five
  notes naming another outside RE source. Untracked-not-ignored after: 0.
"""
def edit(name, anchor, insert):
    with open(R + name, "rb") as f:
        old = f.read()
    a = anchor.encode("utf-8")
    assert old.count(a) == 1, (name, old.count(a))
    i = old.index(a) + len(a)
    new = old[:i] + insert.encode("utf-8") + old[i:]
    assert len(old) > 10000 and len(new) > len(old) and new.count(b"\r") == old.count(b"\r")
    with open(R + name, "wb") as f:
        f.write(new)
    with open(R + name, "rb") as f:
        assert f.read() == new
    print(name, len(old), "->", len(new))
edit("HANDOFF.md", "the hashes are in the WW_CHANGES entry \"Ledger recovery\".\n", HASHES)
edit("WW_CHANGES.md", "Entries for 2026-09-10..09-23\nare NOT here; each lane's report stays in its scratchpad folder.\n",
     "\nCommitted and pushed to origin main the same evening, by path list:\n\n" + HASHES.replace("  ", "", 0))
