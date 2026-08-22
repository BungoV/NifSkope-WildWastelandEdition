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


def state(path):
    """The body's whole rest state: which arrays exist, its motion index, its
    orientation and its position.

    A KEYFRAMED body -- a door, a gate, a pushable car -- has an inertia record
    and a motion INDEX but no dyn_motion record. Compile wrote it as a plain
    static, and a static body cannot be driven by an animation however right its
    collision is. 170 of 1,200 vanilla files are in that state and every one of
    them is something the game moves.
    """
    out = []
    for bi, at, blob in nif_blobs(path):
        try:
            pk = Pack(blob)
        except ValueError:
            continue
        for psd in pk.of_class('hknpPhysicsSystemData'):
            cin = pk.local.get(psd + 0x40)
            if cin is None:
                continue
            for k in range(pk.u32(psd + 0x48)):
                o = cin + k * 0x60
                out.append('motion=%d inertia=%d idx=%08x quat=%.4f,%.4f,%.4f,%.4f pos=%.1f,%.1f,%.1f'
                           % (pk.u32(psd + 0x28), pk.u32(psd + 0x38),
                              struct.unpack_from('<I', pk.data, o + 0x0c)[0],
                              *struct.unpack_from('<4f', pk.data, o + 0x40),
                              *[v * SCALE for v in struct.unpack_from('<3f', pk.data, o + 0x30)]))
    print(' | '.join(out))
    return 0


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
    if '--state' in sys.argv:
        sys.exit(state(sys.argv[1]))
    sys.exit(main(sys.argv[1]))
