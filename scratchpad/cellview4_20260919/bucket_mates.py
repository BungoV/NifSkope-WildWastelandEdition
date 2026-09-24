#!/usr/bin/env python3
"""CELLVIEW4 item 3 -- THE DISCRIMINATOR, part 4: who shares the arrow's
BSTriShape?

src/cellview.cpp:854 keys every bucket on
    mat | hasAlpha | alphaThreshold | ownEmit | withColour | matUnreadable
and shapes with equal keys WELD into one BSTriShape with one shader.  The
arrow's key is "" | 1 | 128 | 0 | 0 | 0 (no material, an NiAlphaProperty,
lodgen's flat 128 because it names no BGSM, no own-emit since its effect
property's Shader Flags 1 = 0x80000008 and LOD_OWN_EMIT = 0x400000, no
overlay).

That makes the question sharp: every other shape with the same key is drawn by
the SAME shader with the SAME two one-texel textures.  If any of them is not
black, the black cannot be a property of that shader or those textures, and
has to be per-vertex.  If the arrow is alone in its bucket, the untextured
branch has no witness in this picture and the cause stays open -- which is a
result too, and the honest one to report.

Usage: python bucket_mates.py
"""
import os
import struct
import sys

import shader_census as sc

DUMP = '../cellview3_20260919/dump_downtown.txt'
ROOT = r'E:/Tools/Fallout 4/DataUnpacked/Data'
LOD_OWN_EMIT = 0x400000


def shape_links(p):
    """-> (shaderLink, alphaLink) for a BSTriShape, by the verified walk."""
    q = 4
    nx = struct.unpack_from('<I', p, q)[0]
    q += 4 + 4 * nx + 4
    q += 4 + 12 + 36 + 4 + 4
    q += 16 + 4
    sh = struct.unpack_from('<i', p, q)[0]
    al = struct.unpack_from('<i', p, q + 4)[0]
    return sh, al


def sf1_of(p, strings):
    """BSShaderProperty's Shader Flags 1, straight after NiObjectNET +
    NiProperty (which adds nothing) for BSVersion >= 130."""
    q = 4
    nx = struct.unpack_from('<I', p, q)[0]
    q += 4 + 4 * nx + 4
    return struct.unpack_from('<I', p, q)[0]


def main():
    here = os.path.dirname(__file__) or '.'
    seen, models = set(), []
    for ln in open(os.path.join(here, DUMP)):
        if ln.startswith('#'):
            continue
        f = ln.split()
        if len(f) < 21:
            continue
        m = ' '.join(f[20:])
        if m not in seen:
            seen.add(m)
            models.append(m)

    mates = {}
    for m in models:
        p = os.path.join(ROOT, 'meshes', m.replace('\\', os.sep))
        if not os.path.exists(p):
            d, n = os.path.split(p)
            for fn in os.listdir(d):
                if fn.lower() == n.lower():
                    p = os.path.join(d, fn)
                    break
        r = sc.read_nif(p)
        if not r:
            continue
        strings, blk = r
        for t, pp in blk:
            if t not in sc.TRISHAPES:
                continue
            try:
                sh, al = shape_links(pp)
            except Exception:                    # noqa: BLE001
                continue
            if not (0 <= sh < len(blk)):
                continue
            st, sp = blk[sh]
            if sc.name_of(sp, strings):
                continue                         # named material: not this bucket
            if st == 'BSEffectShaderProperty':
                pass                             # no texture set -> mat stays empty
            elif st != 'BSLightingShaderProperty':
                continue
            try:
                ownEmit = 1 if (sf1_of(sp, strings) & LOD_OWN_EMIT) else 0
            except Exception:                    # noqa: BLE001
                ownEmit = 0
            key = '|%d|%d|%d|0|0' % (1 if al >= 0 else 0,
                                     128 if al >= 0 else 0, ownEmit)
            mates.setdefault(key, []).append(m)

    print('untextured buckets this cell would build (key = |hasAlpha|thr|emit|0|0):')
    for k, v in sorted(mates.items()):
        print('   %-14s %3d shape(s) from %d model(s)' % (k, len(v), len(set(v))))
    tgt = '|1|128|0|0|0'
    print('\nmodels welded into the ARROW\'s bucket %s:' % tgt)
    for m in sorted(set(mates.get(tgt, []))):
        print('   %s' % m)
    return 0


if __name__ == '__main__':
    sys.exit(main())
