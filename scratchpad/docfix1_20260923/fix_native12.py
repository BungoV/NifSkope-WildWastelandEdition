"""DOCFIX1 NATIVE 10: §12 free-room table against src/lodifile.cpp:56-78 (header word offsets),
:1229-1238 (reserved sweep: v3..v6 pad to 0xF4 on v6, v7/v9 0x11C..0x1FF, v8 0x130..0x1FF),
src/lodifile.h:293-304 (header block 256 / 512 at v7+)."""
import sys
P = 'E:/Projects/NifskopeWildWastelandEdition/docs/LODGEN_NATIVE_LODO_LODI.md'
reps = [
(b"| aggregate ring-3 impostors (CARDS-AGG) | **DONE, v4** \xe2\x80\x94 \xc2\xa74.6; the room this row named at `.lodi` 0xB0 is now the aggregate table, the covered blob and their six header words | 0xD4\xe2\x80\xa60xFF = **44 reserved bytes** left |",
 b"| aggregate ring-3 impostors (CARDS-AGG) | **DONE, v4** \xe2\x80\x94 \xc2\xa74.6; the room this row named at `.lodi` 0xB0 is now the aggregate table, the covered blob and their six header words | \xe2\x80\x94 (the 44 bytes it left at 0xD4\xe2\x80\xa60xFF went to v5 and v6; see the occluder row) |", 1),
(b"| the ladder starting from the NEAR model (\xc2\xa73.5.4) | **DONE, v4** \xe2\x80\x94 \xc2\xa73.5.7; no format change, `--library mnam` is the way back | \xe2\x80\x94 |",
 b"| the ladder starting from the NEAR model (\xc2\xa73.5.4) | **DONE, v4, then REVERSED as the default 2026-09-17** (*\"Authored LODs only\"*) \xe2\x80\x94 \xc2\xa73.5.7; no format change; the default is now `--library mnam` with no ladder, and `--library near` / `--native-ladder` bake this row | \xe2\x80\x94 |", 1),
(b"| `.lodi` header **0xF1\xe2\x80\xa60xFF = 15 reserved bytes** (v4 took 0xB0\xe2\x80\xa60xD3, v5 took 0xD4\xe2\x80\xa60xF0) |",
 b"| `.lodi` v3\xe2\x80\x93v6 header is 256 B and FULL: v4 took 0xB0\xe2\x80\xa60xD3, v5 0xD4\xe2\x80\xa60xF0, v6 0xF4\xe2\x80\xa60xFF, leaving **0xF1\xe2\x80\xa60xF3 = 3 reserved bytes**. A **v7+ header is 512 B** (\xc2\xa74, `lodiHeaderBytes()`): v7 took 0x100\xe2\x80\xa60x10D (group table) and 0x110\xe2\x80\xa60x11B (sky stream), leaving **0x11C\xe2\x80\xa60x1FF = 228 reserved bytes** on v7 and v9 (retired v8 spent 0x11C\xe2\x80\xa60x12F). 0x10E\xe2\x80\xa60x10F sits between the two tables and the reader's reserved sweep does not cover it |", 1),
]
b = open(P, 'rb').read(); cr0 = b.count(b'\r\n'); bad = False
for old, new, n in reps:
    c = b.count(old)
    if c != n: print('REFUSE', old[:70], c); bad = True; continue
    b = b.replace(old, new)
if bad or b.count(b'\r\n') != cr0: sys.exit(1)
if '--check' in sys.argv: print('check OK'); sys.exit(0)
open(P, 'wb').write(b); print('written', len(b), 'CRLF', b.count(b'\r\n'))
