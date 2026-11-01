"""lane FIX1101A (LATE ITEM 28b): our moon maps ship with NifSkope (res/Data/Textures/Sky/Moon -> release/Data/...).
check.py <repo root> <release dir>: the shipped files are the SKYDISC1 maps (BC1 2048x1024 albedo, BC5 1024x512 normal,
9 / >= 1 mips) and the release copies are byte-identical. A release without them (today's build) FAILS."""
import os
import struct
import sys

root, rel = sys.argv[1], sys.argv[2]
WANT = {'Moon_d.dds': (b'DXT1', 2048, 1024), 'Moon_n.dds': (b'ATI2', 1024, 512)}
ok = True
for name, (fcc, w, h) in WANT.items():
    src = os.path.join(root, 'res', 'Data', 'Textures', 'Sky', 'Moon', name)
    dst = os.path.join(rel, 'Data', 'Textures', 'Sky', 'Moon', name)
    if not os.path.isfile(src):
        print('FAIL %s: not in res/Data' % name); ok = False; continue
    b = open(src, 'rb').read()
    hh, ww, mips = struct.unpack_from('<III', b, 12)[0], struct.unpack_from('<I', b, 16)[0], struct.unpack_from('<I', b, 28)[0]
    four = b[84:88]
    good = b[:4] == b'DDS ' and four == fcc and (ww, hh) == (w, h) and mips >= 1
    print('%s %s: %s %dx%d mips %d %d bytes' % ('ok' if good else 'FAIL', name, four.decode('ascii', 'replace'), ww, hh, mips, len(b)))
    ok &= good
    if not os.path.isfile(dst):
        print('FAIL %s: no release copy at %s' % (name, dst)); ok = False; continue
    same = open(dst, 'rb').read() == b
    print('%s %s: release copy %s' % ('ok' if same else 'FAIL', name, 'identical' if same else 'differs'))
    ok &= same
print('OWNDATA1 %s' % ('PASS' if ok else 'FAIL'))
sys.exit(0 if ok else 1)
