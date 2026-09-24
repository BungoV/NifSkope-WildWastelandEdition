"""Step 2: put the newest surviving copies in place. Read everything into variables first, close, then write.
Refuses unless the new content is LONGER than the current file."""
R = "E:/Projects/NifskopeWildWastelandEdition/"
SRC = {
    "HANDOFF.md": "C:/Users/bungo/.claude/file-history/843169d8-671d-4705-ba26-ce53a8012252/5fcfe493b278bdd9@v76",
    "WW_CHANGES.md": R + "scratchpad/build8_20260910/WW_CHANGES.md.bak",
}
for name, src in SRC.items():
    with open(src, "rb") as f:
        new = f.read()
    with open(R + name, "rb") as f:
        old = f.read()
    assert len(new) > len(old) > 10000, (name, len(new), len(old))
    with open(R + name, "wb") as f:
        f.write(new)
    with open(R + name, "rb") as f:
        back = f.read()
    assert back == new
    print("restored", name, len(old), "->", len(new), "CR", back.count(b"\r"))
