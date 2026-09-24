"""Lane UI4 -- append this lane's entries to MISTAKES.md, append-only.

Refuses if the file's existing bytes would change, and reports the CR count on
both sides (CONSTITUTION 8: line endings are measured with Python byte counts).
"""
import sys

TARGET = "MISTAKES.md"
SRC = "scratchpad/ui4_20260910/MISTAKES_ENTRIES.md"
MARKER = b"lane UI4: the probe was fed an UNSUBSTITUTED stylesheet"

with open(TARGET, "rb") as f:
    old = f.read()
with open(SRC, "rb") as f:
    add = f.read()

# drop the HTML comment header of the entries file, keep the entries
cut = add.find(b"## 2026-09-10")
if cut < 0:
    print("REFUSED: no entry heading in %s" % SRC)
    sys.exit(1)
add = add[cut:]

if old.count(MARKER):
    print("REFUSED: MISTAKES.md already carries this lane's first entry")
    sys.exit(1)

sep = b"" if old.endswith(b"\n\n") else (b"\n" if old.endswith(b"\n") else b"\n\n")
new = old + sep + add
if not new.startswith(old):
    print("REFUSED: not an append")
    sys.exit(1)

with open(TARGET, "wb") as f:
    f.write(new)
print("MISTAKES.md %d -> %d bytes (append %d), CR %d -> %d, entries %d"
      % (len(old), len(new), len(new) - len(old), old.count(b"\r"),
         new.count(b"\r"), add.count(b"\n## ") + add.startswith(b"## ")))
