"""Splice the v3 "pass 2" block into src/lodofile.cpp.

A line-range replacement anchored on two lines that each occur EXACTLY ONCE,
per ww-anchored-hookup: the count is printed, the CR count is asserted
unchanged, and --check writes nothing.
"""
import sys

SRC = "src/lodofile.cpp"
NEW = "scratchpad/native1b_20260911/pass2_new.txt"

FIRST = "\t// pass 2: clusters"
LAST = "\t\tstats->boundaryEmitted = lodoBoundaryEdges( allTris, [&]( quint32 v ) { return weld[keys[v]]; } );"

apply = "--apply" in sys.argv

raw = open(SRC, "rb").read()
cr_before = raw.count(b"\r")
text = raw.decode("utf-8")
lines = text.split("\n")

first_hits = [i for i, l in enumerate(lines) if l == FIRST]
last_hits = [i for i, l in enumerate(lines) if l == LAST]
print("anchor 1 %r -> %d hit(s)" % (FIRST, len(first_hits)))
print("anchor 2 (boundaryEmitted line) -> %d hit(s)" % len(last_hits))
if len(first_hits) != 1 or len(last_hits) != 1:
    print("REFUSED: each anchor must match exactly once")
    sys.exit(1)
a = first_hits[0]
b = last_hits[0] + 1          # the closing "\t}" of the stats block
if lines[b].strip() != "}":
    print("REFUSED: the line after the boundaryEmitted line is %r, not the block's close" % lines[b])
    sys.exit(1)
print("replacing lines %d..%d (1-based %d..%d), %d lines" % (a, b, a + 1, b + 1, b - a + 1))

new_raw = open(NEW, "rb").read()
if new_raw.count(b"\r"):
    print("REFUSED: the replacement text carries %d CR bytes; src/ is LF-only" % new_raw.count(b"\r"))
    sys.exit(1)
new_lines = new_raw.decode("utf-8").split("\n")
while new_lines and new_lines[-1] == "":
    new_lines.pop()

out = lines[:a] + new_lines + lines[b + 1:]
result = "\n".join(out)
print("lines %d -> %d, bytes %d -> %d" % (len(lines), len(out), len(raw), len(result.encode("utf-8"))))
if not apply:
    print("--check only: nothing written")
    sys.exit(0)
data = result.encode("utf-8")
if data.count(b"\r") != cr_before:
    print("REFUSED: CR count %d -> %d" % (cr_before, data.count(b"\r")))
    sys.exit(1)
open(SRC, "wb").write(data)
print("applied; CR %d -> %d" % (cr_before, data.count(b"\r")))
