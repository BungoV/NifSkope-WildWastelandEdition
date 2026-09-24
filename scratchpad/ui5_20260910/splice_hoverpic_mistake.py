"""Director splice: lane UI5-HOVERPIC's mistake entry into MISTAKES.md (LF-only)."""
R = "E:/Projects/NifskopeWildWastelandEdition/"
notes = open(R + "scratchpad/ui5_20260910/HOVERPIC_NOTES.md", "rb").read()
assert b"\r" not in notes
head = b"## Mistakes (for the director to splice into `MISTAKES.md`)\n"
i = notes.index(head) + len(head)
body = notes[i:].strip(b"\n")
# the lane wrote one bold-led paragraph; give it the ledger's heading form
heading = b"## 2026-09-11 -- lane UI5-HOVERPIC: a skill append through python -c in a double-quoted bash string lost every code span\n\n"
entry = heading + body + b"\n"
p = R + "MISTAKES.md"
b = open(p, "rb").read()
assert b.count(b"\r") == 0
if b.count(heading.strip()) == 0:
    b = b.rstrip(b"\n") + b"\n\n" + entry
    assert b.count(b"\r") == 0
    open(p, "wb").write(b)
    print("appended", len(entry), "bytes; backticks in entry:", entry.count(b"`"))
else:
    print("present")
