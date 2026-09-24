#!/usr/bin/env python3
"""Build the two files for bungo's game flight of the interleaved writer.

  flight/JogForward.hkx        the shipped JogForward re-written interleaved,
                               content EXACT (round trip 1 measured 0.0)
  flight/JogForward_marked.hkx the same file with ONE bone -- Head -- yawed 45
                               degrees on every frame

Why two.  The exact file answers "does the engine load a class no shipped file
uses"; if it loads, the jog looks exactly like vanilla, and "nothing changed"
is not by itself proof that our file was the one read.  The marked file is the
POSITIVE CONTROL: a turned head can only come from our bytes.

The marker is applied to the packfile directly, at the measured layout
(transforms[frame * numberOfTransformTracks + track], stride 48, rotation at
+16 as Havok x,y,z,w) -- see docs/HKX_WRITE_FORMAT.md section 3.

usage: python make_flight_files.py OUT.hkx BONE_NAMES.txt [--bone Head] [--deg 45]
"""
import math, os, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import interleaved_decode as dec


def qmul(a, b):
    return (a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1],
            a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
            a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3],
            a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2])


def mark(src, dst, bone_names, bone, deg):
    blob = bytearray(open(src, 'rb').read())
    pf = dec.read_packfile(bytes(blob))
    base = pf['base']
    ao = None
    for off, cls, _ in pf['objs']:
        if cls == 'hkaInterleavedUncompressedAnimation':
            ao = off
    assert ao is not None
    nT, = struct.unpack_from('<i', blob, base + ao + 0x18)
    trp, trN = dec.arr(pf, ao + 0x38, 48, "transforms")
    nF = trN // nT
    # the binding says which bone each track drives
    idxP, idxN = dec.arr(pf, [o for o, c, _ in pf['objs'] if c == 'hkaAnimationBinding'][0] + 0x20, 2, "indices")
    bones = list(struct.unpack_from('<%dh' % nT, blob, base + idxP)) if idxN else list(range(nT))
    want = None
    for i, n in enumerate(bone_names):
        if n.lower() == bone.lower():
            want = i
    if want is None:
        raise SystemExit("bone '%s' is not in the %d-name list" % (bone, len(bone_names)))
    track = None
    for t, b in enumerate(bones):
        if b == want:
            track = t
    if track is None:
        raise SystemExit("bone '%s' (index %d) drives no track of this clip" % (bone, want))
    h = math.radians(deg) * 0.5
    yaw = (0.0, 0.0, math.sin(h), math.cos(h))
    for f in range(nF):
        o = base + trp + 48 * (f * nT + track) + 16
        q = struct.unpack_from('<4f', blob, o)
        struct.pack_into('<4f', blob, o, *qmul(yaw, q))
    open(dst, 'wb').write(bytes(blob))
    print("marked '%s' (bone %d, track %d) by %g degrees about +Z on all %d frames -> %s (%d bytes)"
          % (bone, want, track, deg, nF, dst, os.path.getsize(dst)))


if __name__ == '__main__':
    a = sys.argv[1:]
    bone, deg = "Head", 45.0
    if '--bone' in a:
        i = a.index('--bone'); bone = a[i + 1]; del a[i:i + 2]
    if '--deg' in a:
        i = a.index('--deg'); deg = float(a[i + 1]); del a[i:i + 2]
    names = [l.strip() for l in open(a[1]) if l.strip()]
    out = os.path.join(os.path.dirname(a[0]) or '.', os.path.basename(a[0]).rsplit('.', 1)[0] + "_marked.hkx")
    mark(a[0], out, names, bone, deg)
