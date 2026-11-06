"""lane FIX1101A (LATE ITEM 28b), moved by lane WEATHERRECX1: our moon maps ship in the MO2 mod "Fusion Shaders - Assets"
(<mod>/Textures/Sky/Moon), read by src/gamemanager.cpp init_archives after NifSkope's own Data (recx.h wwAssetsModRoot).
check.py <assets mod dir>: the files are the SKYDISC1 maps (BC1 2048x1024 albedo, BC5 1024x512 normal, >= 1 mips).
A folder without them FAILS."""
import os
import struct
import sys

mod = sys.argv[1]
WANT = {'Moon_d.dds': (b'DXT1', 2048, 1024), 'Moon_n.dds': (b'ATI2', 1024, 512)}
ok = True
for name, (fcc, w, h) in WANT.items():
    src = os.path.join(mod, 'Textures', 'Sky', 'Moon', name)
    if not os.path.isfile(src):
        print('FAIL %s: not in %s' % (name, mod)); ok = False; continue
    b = open(src, 'rb').read()
    hh, ww, mips = struct.unpack_from('<III', b, 12)[0], struct.unpack_from('<I', b, 16)[0], struct.unpack_from('<I', b, 28)[0]
    four = b[84:88]
    good = b[:4] == b'DDS ' and four == fcc and (ww, hh) == (w, h) and mips >= 1
    print('%s %s: %s %dx%d mips %d %d bytes' % ('ok' if good else 'FAIL', name, four.decode('ascii', 'replace'), ww, hh, mips, len(b)))
    ok &= good
print('OWNDATA1 %s' % ('PASS' if ok else 'FAIL'))
sys.exit(0 if ok else 1)
