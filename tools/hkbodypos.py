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
            di = pk.local.get(psd + 0x30)
            ni = pk.u32(psd + 0x38)
            for k in range(pk.u32(psd + 0x48)):
                o = cin + k * 0x60
                idx = struct.unpack_from('<I', pk.data, o + 0x0c)[0]
                # The inertia record's OWN index, at its +0x00 as the low u16.
                # 0xffff means "no dyn_motion record behind me", and the engine
                # tests exactly this before indexing the dyn_motion array --
                # writing a real index without the array is a null deref, which
                # is what crashed OfficeFileCabinet01 on 2026-08-22.
                tag = 'none'
                if di is not None and idx != 0x7fffffff and idx < ni:
                    tag = '%04x' % struct.unpack_from('<H', pk.data, di + idx * 0x70)[0]
                out.append('motion=%d inertia=%d idx=%08x itag=%s quat=%.4f,%.4f,%.4f,%.4f pos=%.1f,%.1f,%.1f'
                           % (pk.u32(psd + 0x28), ni, idx, tag,
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
