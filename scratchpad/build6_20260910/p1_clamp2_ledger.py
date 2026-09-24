"""BUILD6: replace the CLAMP2 entry's NOT-BUILT paragraph in WW_CHANGES.md with the
measured status. Byte splice, CR count must not move (19,020), LF-only text."""
import sys
P = 'E:/Projects/NifskopeWildWastelandEdition/WW_CHANGES.md'
b = open(P, 'rb').read()
cr0, lf0 = b.count(b'\r'), b.count(b'\n')
old = (b"**NOT BUILT AND NOT GATED YET** -- the code, the control and this entry are on\n"
       b"disk; the resume is `scratchpad/clamp2_20260910/PENDING.md` and it carries the\n"
       b"gate table. No number below is a reading off a built exe; the seam measurements\n"
       b"are lane BUILD4's, off the master.\n")
new = """**BUILT AND GATED 2026-09-10 (lane BUILD6), one gate RED.** The exe that carries
this code is `release/NifSkope.exe` **03:38:56** -- lane BUILD5b's link, which
picked up `src/lodgen.cpp` (03:33:44) on its way (`lodgen.o` 03:38:54 holds the
`lodgenTerrainRingSelfTest` symbols; `make` had nothing to do). The seven gates
pre-registered in `PENDING.md`, in order: (1) `ringcontrol.sh` **13 checks, 0
failures** (12 were pre-registered; the 13th is the self-test's own verdict),
the CONTROL line reads the old order **REFUSED** at 1096/1096 on the inner
boundary; (2) `lodgen_terrain_vt.sh` **35 checks, 1 failure, RESULT FAIL**; (3)
V9a byte-identical tint ON and OFF on all four chunks, **V9b FAILS**: the
pyramid-assembled and direct `_msn` sheets differ on `Commonwealth.4.-20.28`
only, by **32 texels** (0.0122%), two BC1 blocks, rows 0-3 of the NORTH border
at x = 252..259, centred on x = 256 = the -19|-18 cell corner on the region's
outer y=31|32 edge; -24.28 and both y=24 chunks are identical between the two
paths (lane BUILD4 read this bar green at 35/0, so this is the ownership rule's
own regression, cause measured, NOT cured here); (4) V9c edge step **1.961**
(bar 2.60), E/W ratio 2.75, N/S 2.87, interior 1.803/1.603 -- the same digits
as BUILD4's; (5) `lodgen_terrain.sh` **26/0**, pyramid `UP=G D0=76 D1=32 D2=51
D3=32` on both paths; (6) `lodgen_identity.sh` **8 ok, RESULT PASS**, baseline
unmoved; (7) the edge band against lane CLAMP's clamped bake: colour and `_msn`
**0 beyond 4 on all four borders of all four chunks**, cover and no-cover --
BUILD4's 994 and 1,405 north `_msn` misses are gone -- and `_data` **16 texels
beyond 64 survive on -20.28** (the wetness-domain defect, left as the resume
says); colour moved 127 texels with `--cover` and 0 without (the same reading
as BUILD4's, not new). The second run, against BUILD4's own after-sheets: the
y=24 chunks came back **byte-identical, all 12 files, cover and no-cover** as
predicted; the y=28 `_msn` sheets moved 3,199 and 2,519 texels, **every one in
rows 0-7 of the north border** (maxd 7, none in the east column beyond the
corners, none interior), narrower than the prediction's "north or east".
Verdict and the numbers: `scratchpad/lane_clamp2_report.md` "## Build (BUILD6)".
""".replace('\r', '').encode('utf-8')
assert b.count(old) == 1, b.count(old)
b2 = b.replace(old, new)
assert b2.count(b'\r') == cr0 == 19020, (b2.count(b'\r'), cr0)
open(P, 'wb').write(b2)
print('WW_CHANGES.md CLAMP2 paragraph replaced; CR', cr0, '->', b2.count(b'\r'), 'LF', lf0, '->', b2.count(b'\n'))
