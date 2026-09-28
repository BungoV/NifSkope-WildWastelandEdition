"""IDENT1: one line per .lodi header (version, chunk box, instances, occluders, groups)."""
import struct, sys, glob
paths = []
for a in sys.argv[1:]:
    paths += glob.glob(a)
for f in paths:
    b = open(f, 'rb').read(0x200)
    ver, = struct.unpack_from('<I', b, 4)
    ch = struct.unpack_from('<4h', b, 0x48)
    cc, ic, pc = struct.unpack_from('<III', b, 0x54)
    oc, = struct.unpack_from('<I', b, 0xA8)
    gc, = struct.unpack_from('<I', b, 0x108) if ver >= 7 else (0,)
    print(f, 'v', ver, 'chunks', ch, 'count', cc, 'inst', ic, 'present', pc, 'occ', oc, 'groups', gc)
