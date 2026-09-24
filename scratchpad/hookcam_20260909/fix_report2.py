P = r"E:\Projects\NifskopeWildWastelandEdition\scratchpad\lane_hookcam_report.md"
b = open(P, "rb").read()
cr0 = b.count(b"\r")
assert cr0 == 0

broken = (
    b"Measured at 00:29:43, so that nobody quotes the 00:13:19 exe as current: lanes\n"
    b"WATER2 and the terrain lane have written since it linked --\n"
    b" 00:20:47,  00:23:51, \n"
    b"00:25:38,  00:26:04,  00:29:43, and a new\n"
    b"**WRITTEN, NOT BUILT** entry at the top of . The exe-newer\n"
)
assert b.count(broken) == 1, "broken block count %d" % b.count(broken)

fixed = (
    b"Measured at 00:29:43, so that nobody quotes the 00:13:19 exe as current: lanes\n"
    b"WATER2 and the terrain lane have written since it linked --\n"
    b"`src/lodgen.cpp` 00:20:47, `src/btdterrain.cpp` 00:23:51, `src/lodtfile.cpp`\n"
    b"00:25:38, `src/lodtfile.h` 00:26:04, `src/nifcli.cpp` 00:29:43, and a new\n"
    b"**WRITTEN, NOT BUILT** entry at the top of `WW_CHANGES.md`. The exe-newer\n"
)

b = b.replace(broken, fixed)
open(P, "wb").write(b)
b2 = open(P, "rb").read()
assert b2.count(b"`src/lodgen.cpp` 00:20:47") == 1
assert b2.count(b"`WW_CHANGES.md`. The exe-newer") == 1
print("repaired; CR %d, bytes %d" % (b2.count(b"\r"), len(b2)))
