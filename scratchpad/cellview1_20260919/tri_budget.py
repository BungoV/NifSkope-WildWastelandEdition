"""Lane CELLVIEW1 -- the triangle / VRAM half of the budget.

Input: the `models_<x>_<y>_n<n>.txt` files cell_census.py writes (count TAB path).
For each DISTINCT model it reads the real NIF out of the unpacked data tree and
sums the BSTriShape-family vertex and triangle counts, reusing the tree's own
`tools/rigging_prototype/nifparse.py` rather than a second header reader.

Two totals are printed and they are the two ends of the design decision:

  UNIQUE   what ONE load per distinct model costs -- the instanced viewer's bill
  WELDED   what a .bto-style weld costs, i.e. unique x placements -- the bill
           the existing lodinative bucket builder would run up

A model that is not loose (BA2-only) is counted as MISSING and never averaged
into either total; the line says how many.
"""

import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, os.path.join(REPO, 'tools', 'rigging_prototype'))

import contextlib
import io

import nifparse

SHAPES = ('BSTriShape', 'BSSubIndexTriShape', 'BSMeshLODTriShape',
          'BSDynamicTriShape')


def shape_counts(path):
    """(vertices, triangles, shapes) summed over one NIF's shape blocks."""
    buf = io.StringIO()
    with contextlib.redirect_stdout(buf):
        data, hdr, strings, blocks = nifparse.parse(path)
    nv_t = nt_t = ns = 0
    for i, tname, start, size in blocks:
        if tname not in SHAPES:
            continue
        o = start
        o += 4                          # name
        ne = struct.unpack_from('<I', data, o)[0]
        o += 4 + 4 * ne                 # extra data list
        o += 8                          # controller, flags
        o += 12 + 36 + 4                # translation, rotation, scale
        o += 4                          # collision
        o += 16                         # bounding sphere
        o += 4                          # skin
        o += 8                          # shader, alpha
        o += 8                          # vertex desc
        nt = struct.unpack_from('<I', data, o)[0]
        nv = struct.unpack_from('<H', data, o + 4)[0]
        nv_t += nv
        nt_t += nt
        ns += 1
    return nv_t, nt_t, ns


def main():
    data_root = sys.argv[1]
    print('| block | distinct | missing | unique verts | unique tris | '
          'welded verts | welded tris | unique VRAM MB | welded VRAM MB |')
    print('|---|---|---|---|---|---|---|---|---|')
    for f in sys.argv[2:]:
        uv = ut = wv = wt = 0
        distinct = missing = failed = 0
        for line in open(f):
            cnt, path = line.rstrip('\n').split('\t', 1)
            cnt = int(cnt)
            distinct += 1
            p = os.path.join(data_root, 'meshes', path.replace('\\', os.sep))
            if not os.path.isfile(p):
                missing += 1
                continue
            try:
                nv, nt, ns = shape_counts(p)
            except Exception as e:
                failed += 1
                sys.stderr.write('  parse fail %s: %s\n' % (path, e))
                continue
            uv += nv
            ut += nt
            wv += nv * cnt
            wt += nt * cnt
        # VRAM: FO4 full-model vertex is 32 bytes at the common descriptor
        # (position+uv+normal+tangent+colour, stride from vertexDesc low nibble
        # x 4); index is 3 x u16 per triangle.
        uvram = (uv * 32 + ut * 6) / 1048576.0
        wvram = (wv * 32 + wt * 6) / 1048576.0
        print('| %s | %d | %d | %d | %d | %d | %d | %.1f | %.1f |'
              % (os.path.basename(f), distinct, missing, uv, ut, wv, wt,
                 uvram, wvram))
        if failed:
            sys.stderr.write('%s: %d parse failures\n' % (f, failed))


if __name__ == '__main__':
    main()
