"""Lane UI4 -- pull the two anchor regions out of src/nifskope_ui.cpp EXACTLY.

An anchor typed by hand is an anchor with the wrong whitespace.  These are read
from the file's real bytes, written to disk, and hookup.py reads them back, so
the anchor is the file and not a transcription of it.
"""
import sys

SRC = "src/nifskope_ui.cpp"
OUT = "scratchpad/ui4_20260910/"

# (name, first line, last line) -- 1-based and inclusive
REGIONS = [
    ("anchor_seg_old.txt", 536, 577),
    ("anchor_call_old.txt", 29903, 29904),
]

with open(SRC, "rb") as f:
    data = f.read()

print("file %d bytes, CR %d" % (len(data), data.count(b"\r")))
lines = data.split(b"\n")
for name, a, b in REGIONS:
    chunk = b"\n".join(lines[a - 1:b]) + b"\n"
    with open(OUT + name, "wb") as f:
        f.write(chunk)
    print("%-22s lines %5d..%-5d  %5d bytes  CR %d  occurrences %d"
          % (name, a, b, len(chunk), chunk.count(b"\r"), data.count(chunk)))
    sys.stdout.write("  first: %r\n  last : %r\n"
                     % (lines[a - 1][:60], lines[b - 1][:60]))
