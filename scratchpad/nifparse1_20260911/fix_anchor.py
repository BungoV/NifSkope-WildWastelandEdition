"""Rebuild F7's g_chunkThreads anchor from the FILE'S OWN BYTES.

Written with the Write tool, not a heredoc: the first attempt at this went
through a Bash heredoc, which halved its backslashes and died with a
SyntaxError -- the trap `nifskope-ww-lodgen` already documents and which this
lane has now paid for as well.

The line it anchors on uses SPACES and `//!<`, not tabs and `//!`, which is why
the typed anchor counted 0.
"""
import io

P = r"E:\Projects\NifskopeWildWastelandEdition\scratchpad\nifparse1_20260911\fixes.py"
SRC = r"E:\Projects\NifskopeWildWastelandEdition\src\lodgenparallel.cpp"
NL = chr(10)
T1 = chr(9)

bad = '"int g_chunkThreads = 1;" + T1 + T1 + T1 + T1 + T1 + "//! ONE until the parser is safe" + NL'

real = None
with io.open(SRC, encoding="utf-8") as f:
    for line in f.read().split(NL):
        if line.startswith("int g_chunkThreads = 1;"):
            real = line
            break
assert real is not None, "the declaration moved"
assert chr(92) not in real and '"' not in real, "anchor needs escaping after all: " + repr(real)
lit = '"' + real + '" + NL'

with io.open(P, encoding="utf-8") as f:
    s = f.read()
n = s.count(bad)
assert n == 2, "expected the bad anchor twice (anchor + replacement), found %d" % n
s = s.replace(bad, lit)
with io.open(P, "w", encoding="utf-8", newline=NL) as f:
    f.write(s)
print("rebuilt from bytes: " + repr(real))
