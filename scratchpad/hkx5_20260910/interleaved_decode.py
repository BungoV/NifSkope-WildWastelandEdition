#!/usr/bin/env python3
"""INDEPENDENT decoder for an hkaInterleavedUncompressedAnimation packfile.

Written from docs/HKX_ANIMATION_FORMAT.md sections 1, 5 and 6 plus the measured
class layout of section 9 of the ww-hkx-animation skill -- NOT from
src/hkxwrite.cpp -- so that holding the writer's output against it is not a
check of our own code against our own code (CONSTITUTION rule 4).

Emits the same TSV as tests/spells/hkxanim_decode.py and tests/hkxwrite_dump.cpp:
    frame  track  bone  tx ty tz  qx qy qz qw  sx sy sz
with root motion as track -1 (tx ty tz yaw).

usage: python interleaved_decode.py FILE.hkx [--out frames.tsv] [--quiet]
"""
import struct, sys


class Refusal(Exception):
    pass


def read_packfile(blob):
    """Header + sections + the three fixup tables.  Refuses by name."""
    if len(blob) < 0x50:
        raise Refusal("file is %d bytes, shorter than a packfile header" % len(blob))
    if blob[0:8] != bytes.fromhex('57e0e05710c0c010'):
        raise Refusal("magic is %s, not a Havok packfile" % blob[0:8].hex())
    ver, = struct.unpack_from('<i', blob, 0x0c)
    if ver != 11:
        raise Refusal("fileVersion is %d, not 11" % ver)
    nsec, = struct.unpack_from('<i', blob, 0x14)
    if nsec != 3:
        raise Refusal("numSections is %d, not 3" % nsec)
    predpad, = struct.unpack_from('<H', blob, 0x3e)
    sechdr = 0x40 + predpad
    secs = {}
    for s in range(nsec):
        off = sechdr + s * 0x40
        if off + 0x40 > len(blob):
            raise Refusal("section header %d ends at %d, past the %d-byte file" % (s, off + 0x40, len(blob)))
        tag = blob[off:off + 19].split(b'\0')[0].decode('latin-1')
        secs[tag] = struct.unpack_from('<7i', blob, off + 20)
    for want in ('__classnames__', '__types__', '__data__'):
        if want not in secs:
            raise Refusal("section '%s' is missing (found %s)" % (want, ', '.join(sorted(secs))))

    cn = secs['__classnames__']
    if cn[0] < 0 or cn[0] + cn[1] > len(blob):
        raise Refusal("__classnames__ spans %d..%d of a %d-byte file" % (cn[0], cn[0] + cn[1], len(blob)))
    cnames = {}
    p, end = cn[0], cn[0] + cn[1]
    while p + 5 < end:
        if blob[p + 4] != 0x09:
            break
        sig, = struct.unpack_from('<I', blob, p)
        e = blob.find(b'\0', p + 5, end)
        if e < 0:
            raise Refusal("a class name at 0x%x is not NUL-terminated inside __classnames__" % p)
        cnames[p + 5 - cn[0]] = (sig, blob[p + 5:e].decode('latin-1'))
        p = e + 1

    dt = secs['__data__']
    base = dt[0]
    if base < 0 or base + dt[6] > len(blob):
        raise Refusal("__data__ spans %d..%d of a %d-byte file" % (base, base + dt[6], len(blob)))
    if not (0 <= dt[1] <= dt[2] <= dt[3] <= dt[6]):
        raise Refusal("__data__ fixup offsets are not in order: local %d global %d virtual %d end %d"
                      % (dt[1], dt[2], dt[3], dt[6]))
    local = {}
    for p in range(base + dt[1], base + dt[2] - 7, 8):
        src, dst = struct.unpack_from('<ii', blob, p)
        if src != -1:
            local[src] = dst
    glob = {}
    for p in range(base + dt[2], base + dt[3] - 11, 12):
        src, sec, dst = struct.unpack_from('<iii', blob, p)
        if src != -1:
            glob[src] = (sec, dst)
    objs = []
    for p in range(base + dt[3], base + dt[4] - 11, 12):
        src, sec, cno = struct.unpack_from('<iii', blob, p)
        if src == -1:
            continue
        if cno not in cnames:
            raise Refusal("a virtual fixup at object 0x%x names class-name offset 0x%x, which is not in __classnames__" % (src, cno))
        objs.append((src, cnames[cno][1], cnames[cno][0]))
    return dict(blob=blob, base=base, dt=dt, local=local, glob=glob, objs=objs, cnames=cnames)


def arr(pf, off, stride, name):
    """An hkArray<T> at section offset `off`: (payload offset or None, size)."""
    n, = struct.unpack_from('<i', pf['blob'], pf['base'] + off + 8)
    if n < 0:
        raise Refusal("%s has size %d" % (name, n))
    p = pf['local'].get(off)
    if n and p is None:
        raise Refusal("%s has size %d but no payload pointer (no local fixup at 0x%x)" % (name, n, off))
    if p is not None:
        endb = pf['base'] + p + stride * n
        if p < 0 or endb > pf['base'] + pf['dt'][1]:
            raise Refusal("%s payload spans 0x%x..0x%x, past the %d-byte __data__ payload"
                          % (name, p, p + stride * n, pf['dt'][1]))
    return p, n


def rdstr(pf, off):
    p = pf['local'].get(off)
    if p is None:
        return ""
    b = pf['blob']
    e = b.find(b'\0', pf['base'] + p)
    return b[pf['base'] + p:e].decode('latin-1')


def decode(path, quiet=False):
    blob = open(path, 'rb').read()
    pf = read_packfile(blob)
    b, base = pf['blob'], pf['base']

    byclass = {}
    for off, cls, sig in pf['objs']:
        byclass.setdefault(cls, []).append(off)
    if 'hkaAnimationContainer' not in byclass:
        raise Refusal("the file carries no hkaAnimationContainer (objects: %s)"
                      % ', '.join(sorted(byclass)) if byclass else "the file carries no objects")
    if 'hkaInterleavedUncompressedAnimation' not in byclass:
        raise Refusal("the file carries no hkaInterleavedUncompressedAnimation (objects: %s)"
                      % ', '.join(sorted(byclass)))
    if 'hkaAnimationBinding' not in byclass:
        raise Refusal("the file carries no hkaAnimationBinding (objects: %s)" % ', '.join(sorted(byclass)))

    ao = byclass['hkaInterleavedUncompressedAnimation'][0]
    atype, dur, nT, nF_tracks = struct.unpack_from('<ifii', b, base + ao + 0x10)
    if atype != 1:
        raise Refusal("hkaAnimation::type is %d; HK_INTERLEAVED_ANIMATION is 1" % atype)
    if nT <= 0:
        raise Refusal("numberOfTransformTracks is %d" % nT)
    if nF_tracks:
        raise Refusal("numberOfFloatTracks is %d; this decoder reads transform tracks only" % nF_tracks)

    annP, annN = arr(pf, ao + 0x28, 0x18, "annotationTracks")
    if annN != nT:
        raise Refusal("annotationTracks has %d entries and the animation has %d transform tracks" % (annN, nT))
    trP, trN = arr(pf, ao + 0x38, 48, "transforms")
    if trN % nT:
        raise Refusal("transforms has %d elements, not a whole multiple of the %d transform tracks" % (trN, nT))
    numFrames = trN // nT
    if numFrames < 2:
        raise Refusal("transforms holds %d frames; Havok stores a one-frame pose as two" % numFrames)
    flP, flN = arr(pf, ao + 0x48, 4, "floats")
    if flN:
        raise Refusal("floats has %d elements; this decoder reads transform tracks only" % flN)

    bo = byclass['hkaAnimationBinding'][0]
    skel = rdstr(pf, bo + 0x10)
    idxP, idxN = arr(pf, bo + 0x20, 2, "transformTrackToBoneIndices")
    if idxN and idxN != nT:
        raise Refusal("the binding maps %d tracks, the animation has %d" % (idxN, nT))
    if idxN:
        bones = list(struct.unpack_from('<%dh' % nT, b, base + idxP))
    else:
        bones = list(range(nT))          # lane FIXTURE's rule: empty = identity
    blend = b[base + bo + 0x50]
    if blend not in (0, 1, 2):
        raise Refusal("blendHint is %d; NORMAL 0, ADDITIVE_DEPRECATED 1, ADDITIVE 2" % blend)
    anim_ref = pf['glob'].get(bo + 0x18)
    if anim_ref is None or anim_ref[1] != ao:
        raise Refusal("the binding's animation pointer does not reach the interleaved animation at 0x%x" % ao)

    # duration law, the same one the reader gates on
    frameDuration = dur / (numFrames - 1) if numFrames > 1 else 0.0

    rootmotion = None
    if 'hkaDefaultAnimatedReferenceFrame' in byclass:
        mo = byclass['hkaDefaultAnimatedReferenceFrame'][0]
        ref = pf['glob'].get(ao + 0x20)
        if ref is None or ref[1] != mo:
            raise Refusal("the animation carries a reference frame at 0x%x that extractedMotion does not point at" % mo)
        up = struct.unpack_from('<4f', b, base + mo + 0x20)
        fwd = struct.unpack_from('<4f', b, base + mo + 0x30)
        mdur, = struct.unpack_from('<f', b, base + mo + 0x40)
        sP, sN = arr(pf, mo + 0x48, 16, "referenceFrameSamples")
        if sN != numFrames:
            raise Refusal("the reference frame has %d samples for %d frames" % (sN, numFrames))
        rootmotion = dict(up=up[:3], forward=fwd[:3], duration=mdur,
                          samples=[struct.unpack_from('<4f', b, base + sP + 16 * i) for i in range(sN)])

    frames = []
    for f in range(numFrames):
        row = []
        for t in range(nT):
            # MEASURED frame-major: transforms[frame * numberOfTransformTracks + track]
            # (hkaInterleavedUncompressedAnimation::transformTrack rva 0x01fa1ac0,
            #  the index arithmetic at 0x01fa1b05-0x01fa1b33)
            o = base + trP + 48 * (f * nT + t)
            v = struct.unpack_from('<12f', b, o)
            row.append(v)
        frames.append(row)

    if not quiet:
        print("interleaved: %d tracks x %d frames, duration %.6f (frameDuration %.6f), skeleton '%s', blendHint %d, "
              "annotations %s, root motion %s" %
              (nT, numFrames, dur, frameDuration, skel, blend,
               "table of %d" % annN, "yes (%d samples)" % len(rootmotion['samples']) if rootmotion else "no"))
        print("objects: %s" % ", ".join("%s@0x%x" % (c, o) for o, c, _ in pf['objs']))
    return dict(tracks=nT, frames=numFrames, duration=dur, frameDuration=frameDuration,
                skeleton=skel, blend=blend, bones=bones, data=frames, rootmotion=rootmotion,
                identityBinding=(idxN == 0), objects=[(c, o) for o, c, _ in pf['objs']])


def write_tsv(d, out):
    with open(out, 'w', newline='\n') as fh:
        fh.write("# frame\ttrack\tbone\ttx\tty\ttz\tqx\tqy\tqz\tqw\tsx\tsy\tsz\n")
        for f in range(d['frames']):
            for t in range(d['tracks']):
                v = d['data'][f][t]
                fh.write("%d\t%d\t%d\t%s\n" % (f, t, d['bones'][t],
                    "\t".join("%.9g" % x for x in (v[0], v[1], v[2], v[4], v[5], v[6], v[7], v[8], v[9], v[10]))))
            if d['rootmotion']:
                s = d['rootmotion']['samples'][f]
                fh.write("%d\t-1\t-1\t%s\n" % (f, "\t".join("%.9g" % x for x in s)))


if __name__ == '__main__':
    args = sys.argv[1:]
    quiet = '--quiet' in args
    args = [a for a in args if a != '--quiet']
    out = None
    if '--out' in args:
        i = args.index('--out')
        out = args[i + 1]
        del args[i:i + 2]
    try:
        d = decode(args[0], quiet)
    except Refusal as e:
        print("REFUSED: %s" % e)
        sys.exit(2)
    if out:
        write_tsv(d, out)
        if not quiet:
            print("wrote %s" % out)
