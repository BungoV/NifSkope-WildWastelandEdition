#!/usr/bin/env python3
"""CELLVIEW4 item 3 -- THE DISCRIMINATOR, part 3.

Part 1 (tangent_census.py) found 12 tangent-less models; part 2
(black_probe.py) measured 18 of their instances on screen and found exactly
one black, so "no VF_TANGENT" is NOT the property that makes the arrow black.
Mechanism agrees: the placeholder normal cellview writes for an untextured
shape, `#FFFF8080n`, decodes through TexCache::texLoadColor
(src/gl/gltexloaders.cpp:987-1019) as R8G8B8A8_SNORM after `color ^ 0x80808080`
-> bytes 00 00 7F 7F -> tangent-space (0, 0, +1), a FLAT normal that never
touches the tangent vector at all.

So test the other property the arrow has: it is the shape that reaches
src/cellview.cpp:347's "No material at all" branch.  That branch is taken when
lodgen ends with BOTH `matName` and `tex0` empty, which for FO4 means a shape
whose Shader Property is a BSEffectShaderProperty with an empty `Name` (an
effect property carries no Texture Set, and lodgen only reads its
`Source Texture` inside the `.bgem` branch, src/lodgen.cpp:2296-2301 -- a
branch an empty name never enters).

This lists every model in the cell that has such a shape.  If the arrow is the
only one, the white branch is effectively untested and the cause has to be
argued from the branch itself; if there are others, black_probe.py can measure
them and settle it.

Usage: python shader_census.py
"""
import os
import struct
import sys

DUMP = '../cellview3_20260919/dump_downtown.txt'
ROOT = r'E:/Tools/Fallout 4/DataUnpacked/Data'
TRISHAPES = {'BSTriShape': 0, 'BSMeshLODTriShape': 12}
SHADERS = ('BSLightingShaderProperty', 'BSEffectShaderProperty')


def read_nif(path):
    """-> (types, strings, [(type, payload)]) or None."""
    with open(path, 'rb') as fh:
        b = fh.read()
    if not b.startswith(b'Gamebryo'):
        return None
    try:
        o = b.index(b'\n') + 1
        o += 4 + 1 + 4
        nblk = struct.unpack_from('<I', b, o)[0]
        o += 4
        bsver = struct.unpack_from('<I', b, o)[0]
        o += 4
        for _ in range(4 if bsver >= 130 else 3):
            o += 1 + b[o]
        nt = struct.unpack_from('<H', b, o)[0]
        o += 2
        types = []
        for _ in range(nt):
            L = struct.unpack_from('<I', b, o)[0]
            o += 4
            types.append(b[o:o + L].decode('latin1'))
            o += L
        idx = struct.unpack_from('<%dH' % nblk, b, o)
        o += 2 * nblk
        sizes = struct.unpack_from('<%dI' % nblk, b, o)
        o += 4 * nblk
        ns = struct.unpack_from('<I', b, o)[0]
        o += 8
        strings = []
        for _ in range(ns):
            L = struct.unpack_from('<I', b, o)[0]
            o += 4
            strings.append(b[o:o + L].decode('latin1'))
            o += L
        ng = struct.unpack_from('<I', b, o)[0]
        o += 4 + 4 * ng
        blk = []
        for k in range(nblk):
            blk.append((types[idx[k]], b[o:o + sizes[k]]))
            o += sizes[k]
        return strings, blk
    except Exception:                            # noqa: BLE001
        return None


def name_of(p, strings):
    """NiObjectNET's Name, the first field of every block here."""
    i = struct.unpack_from('<i', p, 0)[0]
    return strings[i] if 0 <= i < len(strings) else ''


def shape_shader(p):
    """BSTriShape's Shader Property link, by the field walk verified against
    MarkerXHeading.nif (118-byte header)."""
    q = 4
    nx = struct.unpack_from('<I', p, q)[0]
    q += 4 + 4 * nx + 4
    q += 4 + 12 + 36 + 4 + 4
    q += 16 + 4                              # bounding sphere, skin
    return struct.unpack_from('<i', p, q)[0]


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

    kinds = {}
    hits = []
    allEmpty = []                # models where EVERY shape is material-less
    for m in models:
        nEmpty = nShape = 0
        p = os.path.join(ROOT, 'meshes', m.replace('\\', os.sep))
        if not os.path.exists(p):
            d, n = os.path.split(p)
            for fn in os.listdir(d):
                if fn.lower() == n.lower():
                    p = os.path.join(d, fn)
                    break
        r = read_nif(p)
        if not r:
            continue
        strings, blk = r
        for t, pp in blk:
            if t not in TRISHAPES:
                continue
            try:
                link = shape_shader(pp)
            except Exception:                    # noqa: BLE001
                continue
            nShape += 1
            if not (0 <= link < len(blk)):
                kinds['no shader link'] = kinds.get('no shader link', 0) + 1
                continue
            st, sp = blk[link]
            nm = name_of(sp, strings)
            if not nm:
                nEmpty += 1
            if st == 'BSEffectShaderProperty' and not nm:
                kinds['effect, EMPTY name'] = kinds.get('effect, EMPTY name', 0) + 1
                if m not in hits:
                    hits.append(m)
            elif st == 'BSEffectShaderProperty':
                kinds['effect, named'] = kinds.get('effect, named', 0) + 1
            elif st == 'BSLightingShaderProperty' and not nm:
                kinds['lighting, EMPTY name'] = kinds.get('lighting, EMPTY name', 0) + 1
                if m not in hits:
                    hits.append(m)
            else:
                kinds['lighting, named'] = kinds.get('lighting, named', 0) + 1
        if nShape and nEmpty == nShape:
            allEmpty.append((m, nShape))

    print('shapes by shader property, over %d distinct models:' % len(models))
    for k, v in sorted(kinds.items()):
        print('   %-24s %d' % (k, v))
    print('\nmodels with a shape whose material name is EMPTY: %d' % len(hits))
    print('models where EVERY shape is material-less (the whole model takes'
          ' the #FFFFFFFF branch): %d' % len(allEmpty))
    for m, n in allEmpty:
        print('   %2d shape(s)   %s' % (n, m))
    return 0


if __name__ == '__main__':
    sys.exit(main())
