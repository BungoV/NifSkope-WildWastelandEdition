#!/usr/bin/env python3
"""PERF1 step 7 -- the IDENTITY refuter.

    python s7_refute_order.py break     flip the ladder retire order, always
    python s7_refute_order.py break2    flip it ONLY when the fan-out is on
    python s7_refute_order.py restore   put either one back

The lane's whole claim is that the parallel object pass retires its work in JOB
ORDER, so the library is assembled in exactly the order the serial loop
assembled it. These flip that order inside each batch and nothing else: the
same meshes, the same staging, the same everything, merged back to front.

`break` flips it ALWAYS -- including on the `--threads 1` arm. Measured: leg
(a) stays GREEN, because leg (a) compares two arms of the SAME exe and both are
equally reordered. Leg (c) goes red (the rung exe is not reordered) and leg (d)
goes red with the payload check naming it: "mesh table is not sorted by model
path at row 1".

`break2` flips it only when `lodgenThreadCount() > 1`, i.e. only when the
fan-out is actually running. That is the shape of a real threading defect --
the serial path is fine and the parallel path is not -- and it is the one leg
(a) is built to catch.
"""
import sys

P = "E:/Projects/NifskopeWildWastelandEdition/src/nativeemit.cpp"

GOOD = (
    "\t\t\t\tfor ( size_t i = 0; i < count; i++ ) {\n"
    "\t\t\t\t\tModel & m = *ladderModels[base + i];\n"
    "\t\t\t\t\tLadderJob & j = batch[i];\n"
)
BAD = (
    "\t\t\t\t/* REFUTER BUILD -- lane PERF1, NOT FOR SHIPPING. The retire runs\n"
    "\t\t\t\t * BACK TO FRONT inside each batch. Everything else is untouched. */\n"
    "\t\t\t\tfor ( size_t rr = 0; rr < count; rr++ ) {\n"
    "\t\t\t\t\tconst size_t i = count - 1 - rr;\n"
    "\t\t\t\t\tModel & m = *ladderModels[base + i];\n"
    "\t\t\t\t\tLadderJob & j = batch[i];\n"
)

BAD2 = (
    "\t\t\t\t/* REFUTER BUILD -- lane PERF1, NOT FOR SHIPPING. The retire runs\n"
    "\t\t\t\t * BACK TO FRONT inside each batch, but ONLY when the fan-out is\n"
    "\t\t\t\t * actually on -- the shape of a real threading defect. */\n"
    "\t\t\t\tconst bool refuteFlip = lodgenThreadCount() > 1;\n"
    "\t\t\t\tfor ( size_t rr = 0; rr < count; rr++ ) {\n"
    "\t\t\t\t\tconst size_t i = refuteFlip ? ( count - 1 - rr ) : rr;\n"
    "\t\t\t\t\tModel & m = *ladderModels[base + i];\n"
    "\t\t\t\t\tLadderJob & j = batch[i];\n"
)

mode = sys.argv[1]
s = open(P, "rb").read().decode("utf-8")
assert s.count("\r") == 0, "CRLF in nativeemit.cpp"
if mode == "break":
    assert s.count(GOOD) == 1, s.count(GOOD)
    s = s.replace(GOOD, BAD)
elif mode == "break2":
    assert s.count(GOOD) == 1, s.count(GOOD)
    s = s.replace(GOOD, BAD2)
elif mode == "restore":
    n = s.count(BAD) + s.count(BAD2)
    assert n == 1, n
    s = s.replace(BAD, GOOD).replace(BAD2, GOOD)
else:
    raise SystemExit("break | break2 | restore")
open(P, "wb").write(s.encode("utf-8"))
print(mode, "ok; CR", s.count("\r"))
