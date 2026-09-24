#!/usr/bin/env python3
"""CELLVIEW4 item 3 -- THE DISCRIMINATOR for the solid-black shape.

Two candidate causes were on the table for the black arrow in
`scratchpad/cellview3_20260919/images/after_downtown.png`:

  (A) it is drawn at all -- `isMarkerModel()` (src/cellview.cpp:200) misses
      marker models that sit at the MESHES ROOT, because every one of its four
      tests needs a backslash or the exact suffix `markerx.nif`;
  (B) it is BLACK -- its source NIF's vertex descriptor has no VF_TANGENT
      (bit 4), lodgen fills `geom.tan` with ZEROS anyway
      (src/lodgen.cpp:2175 appends one entry per row whatever the descriptor
      says; 2386 then keeps it because the array is full-length), and
      src/cellview.cpp:1036's fallback only fires when the array is SHORT.
      A zero tangent is a degenerate TBN in the shader.

(A) alone would not explain the colour, and (B) alone would not explain a
marker being in the picture.  They are separate defects.

THE DISCRIMINATOR FOR (B): if a model with no VF_TANGENT draws black, then the
set of tangent-less models in the cell must be small and must contain the
marker; and any tangent-less model that is NOT black in the picture refutes it.
This prints that set.  Run it before believing (B).

Usage: python tangent_census.py <dump.txt> <data root>
"""
import os
import struct
import sys

VF_TANGENT = 16
VF_COLORS = 32
VF_UV = 2
VF_NORMAL = 8


def blocks(path):
    """-> [(type, payload bytes)] for a BSVersion>=100 NIF, or None."""
    with open(path, 'rb') as fh:
        b = fh.read()
    if not b.startswith(b'Gamebryo') and not b.startswith(b'NetImmerse'):
        return None
    try:
        o = b.index(b'\n') + 1
        o += 4                                  # version
        o += 1                                  # endian
        o += 4                                  # user version
        nblk = struct.unpack_from('<I', b, o)[0]
        o += 4
        bsver = struct.unpack_from('<I', b, o)[0]
        o += 4
        nexp = 4 if bsver >= 130 else 3
        for _ in range(nexp):
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
        for _ in range(ns):
            L = struct.unpack_from('<I', b, o)[0]
            o += 4 + L
        ng = struct.unpack_from('<I', b, o)[0]
        o += 4 + 4 * ng
        out = []
        for k in range(nblk):
            out.append((types[idx[k]], b[o:o + sizes[k]]))
            o += sizes[k]
        return out
    except Exception:                            # noqa: BLE001
        return None


# Bytes each subclass adds AFTER the triangle array.  BSMeshLODTriShape's 12
# are its three LOD triangle counts; measured over cell 5,-11 by probe_tails.py
# (BSTriShape tail 0 x801, BSMeshLODTriShape tail 12 x270, nothing else
# present), and the dsize identity holds for all 1071 shapes either way.
TRISHAPES = {'BSTriShape': 0, 'BSMeshLODTriShape': 12}


def descs(path):
    """Every BSTriShape vertex descriptor, read by the FIELD ORDER nif.xml
    declares for NIF 20.2.0.7 / BSVersion >= 130 -- NOT by scanning for a
    plausible u64, which is how this file first got 0x02F out of a shape whose
    descriptor is 0x029 (docs/MISTAKES: a byte-scan used as evidence about a
    format that has a reader in this tree).

      NiObjectNET  Name u32, NumExtraDataList u32 + n*u32, Controller u32
      NiAVObject   Flags u32 (BSVER>26), Translation 12, Rotation 36,
                   Scale 4, CollisionObject u32
      BSTriShape   BoundingSphere 16, Skin u32, ShaderProperty u32,
                   AlphaProperty u32, VertexDesc u64,
                   NumTriangles u32 (BSVER>=130), NumVertices u16, DataSize u32

    Self-check: MarkerXHeading.nif must come back flags 0x029, vsize 16,
    40 vertices, 18 triangles, and the header must be 118 bytes.
    """
    bl = blocks(path)
    if bl is None:
        return None
    out = []
    for t, p in bl:
        if t not in TRISHAPES:
            continue
        try:
            q = 4
            nx = struct.unpack_from('<I', p, q)[0]
            q += 4 + 4 * nx + 4          # extra data list + controller
            q += 4 + 12 + 36 + 4 + 4     # flags, T, R, S, collision
            q += 16 + 4 + 4 + 4          # bound, skin, shader, alpha
            v = struct.unpack_from('<Q', p, q)[0]
            q += 8
            ntri = struct.unpack_from('<I', p, q)[0]
            q += 4
            nvert = struct.unpack_from('<H', p, q)[0]
            q += 2
            dsize = struct.unpack_from('<I', p, q)[0]
            q += 4
            flags = (v >> 44) & 0xFFFFF
            size = (v & 0xF) * 4
            # THE FLOOR: the record has to describe its own bytes, or the
            # layout is wrong and the flags word is not evidence of anything.
            if dsize != nvert * size + ntri * 6 \
                    or q + dsize + TRISHAPES[t] != len(p):
                out.append((None, None, 'layout refused'))
                continue
            out.append((flags, size, None))
        except Exception:                        # noqa: BLE001
            out.append((None, None, 'short block'))
    return out


def main():
    dump, root = sys.argv[1], sys.argv[2]
    models = []
    seen = set()
    for ln in open(dump):
        if ln.startswith('#'):
            continue
        f = ln.split()
        if len(f) < 21:
            continue
        m = ' '.join(f[20:])
        if m not in seen:
            seen.add(m)
            models.append(m)
    print('distinct models in the dump: %d' % len(models))
    missing = notan = refused = 0
    bad = []
    for m in models:
        p = os.path.join(root, 'meshes', m.replace('\\', os.sep))
        if not os.path.exists(p):
            d, n = os.path.split(p)
            hit = None
            if os.path.isdir(d):
                for f in os.listdir(d):
                    if f.lower() == n.lower():
                        hit = os.path.join(d, f)
                        break
            if not hit:
                missing += 1
                continue
            p = hit
        ds = descs(p)
        if not ds:
            continue
        if any(why for _f, _s, why in ds):
            refused += 1
            continue
        for flags, size, _why in ds:
            if not (flags & VF_TANGENT):
                notan += 1
                bad.append((m, flags, size))
                break
    print('models not found on disk: %d' % missing)
    print('models whose layout the parser refused: %d' % refused)
    print('models with a shape that has NO VF_TANGENT: %d' % notan)
    for m, flags, size in bad:
        print('   flags 0x%03X (%s)  vsize %2d   %s' % (
            flags,
            '|'.join(n for bit, n in ((1, 'VERT'), (2, 'UV'), (4, 'UV2'),
                                      (8, 'NRM'), (16, 'TAN'), (32, 'COL'),
                                      (64, 'SKIN'), (0x400, 'FULLPREC'))
                     if flags & bit),
            size, m))
    return 0


if __name__ == '__main__':
    sys.exit(main())
