"""Print every body's own position, in game units.

    python tools/hkbodypos.py <file.nif>

cinfo +0x30, the field a door keeps its HINGE in. Decompile dropped it on static
bodies, which is invisible to any comparison of where the collision SITS -- the
offset had simply moved into the hull -- and fatal the moment the engine rotates
the body to follow an animation.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from hkmatrun import nif_blobs, Pack

SCALE = 69.99125


def main(path):
    out = []
    for bi, at, blob in nif_blobs(path):
        try:
            pk = Pack(blob)
        except ValueError:
            continue
        for psd in pk.of_class('hknpPhysicsSystemData'):
            base = pk.local.get(psd + 0x40)
            if base is None:
                continue
            for k in range(pk.u32(psd + 0x48)):
                v = struct.unpack_from('<3f', pk.data, base + k * 0x60 + 0x30)
                out.append('%.1f,%.1f,%.1f' % tuple(x * SCALE for x in v))
    print(' | '.join(out))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1]))
