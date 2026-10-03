#!/usr/bin/env python3
"""Lane FXREST1 checker: editor-only shapes in the cell view (tests/spells/cell_edmark.sh).

Independent of the viewer's code: reads every model the cell placed (the run's WW_CELL_DUMP) straight from the
loose data folder with this file's own NIF reader (cell_glow_check's header walk), and applies the game's rule as
read from the game's code (notes, private): a node or shape whose NAME CONTAINS "EditorMarker", any case, is
removed with everything under it. The viewer used to drop only names that START with it.

  A  the census line "editor markers left out" names the same models (word inside the name) as this script
  B  the picture: pixels that change when every effect is hidden (WW_CELL_FX_RED=hide), over the ground of the
     models of A (each pixel's surface from the position probes 2 / 3, inside their placed box + 200 units) -- a
     drawn editor box is an effect shape, so with the markers gone the two pictures agree there (bar <= 40 px)
  C  (census only, no bar) the effect shapes of the cell's models: base colour alpha 0, greyscale palette

usage: cell_edmark_check.py <run dir> <data dir>   (a red run is checked the same way and must FAIL)
"""
import os
import struct
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cell_glow_check import NODES, av_object, nif_read  # noqa: E402  (the NIF header and AV objects)

SHAPES = ('BSTriShape', 'BSSubIndexTriShape', 'BSMeshLODTriShape', 'BSDynamicTriShape')
WORD = 'editormarker'


def model_facts(path):
    """(shapes dropped by prefix, shapes dropped with the word inside, effect census rows)"""
    b, blocks, strings, _ = nif_read(path)
    name, parent, verts, shader = {}, {}, {}, {}
    for i, (t, off, _size) in enumerate(blocks):
        if t in NODES or t in SHAPES:
            nm, _xf, o = av_object(b, off, strings)
            name[i] = nm
            if t in NODES:
                n, = struct.unpack_from('<I', b, o)
                for k in struct.unpack_from('<%di' % n, b, o + 4):
                    if k >= 0:
                        parent[k] = i
            else:
                shader[i], = struct.unpack_from('<i', b, o + 16 + 4)
                verts[i], = struct.unpack_from('<H', b, o + 16 + 12 + 8 + 4)
    pre = ins = 0
    fx = []
    for i in verts:
        if verts[i] == 0:
            continue
        k, hit = i, None
        for _ in range(64):     # the first name up the chain that holds the word decides
            if k is None:
                break
            if WORD in name.get(k, '').lower():
                hit = name[k].lower().startswith(WORD)
                break
            k = parent.get(k)
        if hit is True:
            pre += 1
            continue
        if hit is False:
            ins += 1
            continue
        s = shader[i]
        if not (0 <= s < len(blocks)) or blocks[s][0] != 'BSEffectShaderProperty':
            continue
        o = blocks[s][1]
        _ni, nextra = struct.unpack_from('<iI', b, o)
        o += 8 + 4 * nextra + 4
        sf1, _sf2 = struct.unpack_from('<II', b, o)
        o += 8 + 16
        n, = struct.unpack_from('<I', b, o)
        o += 4 + n + 4 + 16
        col = struct.unpack_from('<4f', b, o)
        fx.append((name[i], col[3] == 0.0, bool(sf1 & 0x10)))
    return pre, ins, fx


def norm(m):
    m = m.strip().lower().replace('/', '\\')
    return m[7:] if m.startswith('meshes\\') else m


def dump_rows(run):
    rows = []
    for ln in open(os.path.join(run, 'celldump.txt'), encoding='latin1'):
        if ln.startswith('#'):
            continue
        f = ln.split()
        if len(f) < 21:
            continue
        rows.append((' '.join(f[20:]), [float(v) for v in f[14:20]]))
    return rows


def census_line(notes):
    for ln in open(notes, encoding='latin1', errors='replace'):
        if 'editor markers left out:' in ln:
            return ln.strip()
    return ''


def main():
    run, data = sys.argv[1], sys.argv[2]
    rows = dump_rows(run)
    models = sorted({m for m, _ in rows})
    inside, fxrows, missing = {}, [], 0
    for m in models:
        p = os.path.join(data, 'Meshes', m.replace('\\', '/'))
        if not os.path.isfile(p):
            missing += 1
            continue
        try:
            pre, ins, fx = model_facts(p)
        except (struct.error, ValueError, IndexError):
            missing += 1
            continue
        if ins:
            inside[norm(m)] = ins
        placed = sum(1 for mm, _ in rows if mm == m)
        fxrows += [(m, nm, a0, pal, placed) for nm, a0, pal in fx]
    ok = True
    line = census_line(os.path.join(run, 'lit.notes'))
    print('  models %d (not read from the loose folder %d); with the word inside a shape name: %d %s'
          % (len(models), missing, len(inside), sorted(inside)))
    print('  census: ' + line)
    # A
    said = set()
    if ' models (' in line:
        said = {norm(s) for s in line.split(' models (', 1)[1].rstrip(')').split(';') if s.strip()}
    want = set(inside)
    a_ok = said == want and bool(line)
    print('  A  %s  census names %d models, this script %d%s' % ('PASS' if a_ok else 'FAIL', len(said), len(want),
          '' if a_ok else ' (census %s / script %s)' % (sorted(said), sorted(want))))
    ok &= a_ok
    # B: the screen box of the inside-marker models, from the eye camera of the run
    on = np.asarray(Image.open(os.path.join(run, 'lit.png')).convert('RGB')).astype(int)
    off = np.asarray(Image.open(os.path.join(run, 'fxhide.png')).convert('RGB')).astype(int)
    diff = np.abs(on - off).sum(2) > 30
    # the surface behind each pixel, from the position probes 2 / 3 (shot with the effects hidden)
    p2 = np.asarray(Image.open(os.path.join(run, 'p2.png')).convert('RGB')).astype(float)
    p3 = np.asarray(Image.open(os.path.join(run, 'p3.png')).convert('RGB')).astype(float)
    cen = [float(v) for v in open(os.path.join(run, 'center.txt')).read().split(',')]
    P = p2 * 256 + p3 - 32768 + np.array(cen)
    mask = np.zeros_like(diff)
    for m, bb in rows:
        if norm(m) not in inside:
            continue
        g = 200.0   # a line drawn at the box's top edge covers ground up to a little past the box
        mask |= ((P[:, :, 0] >= bb[0] - g) & (P[:, :, 0] <= bb[3] + g) & (P[:, :, 1] >= bb[1] - g)
                 & (P[:, :, 1] <= bb[4] + g))
    n = int((diff & mask).sum())
    b_ok = n <= 40
    print('  B  %s  pixels the effects change over the marker models\' ground: %d of %d (bar <= 40)'
          % ('PASS' if b_ok else 'FAIL', n, int(mask.sum())))
    ok &= b_ok and mask.sum() > 0
    # C
    a0 = [r for r in fxrows if r[2]]
    pal = [r for r in fxrows if r[3]]
    print('  C  effect shapes in the cell\'s models: %d (placed %d); base alpha 0: %d (placed %d); greyscale palette: %d '
          '(placed %d)' % (len(fxrows), sum(r[4] for r in fxrows), len(a0), sum(r[4] for r in a0), len(pal),
                           sum(r[4] for r in pal)))
    for r in sorted(set((r[0], r[1], r[2], r[3]) for r in a0 + pal)):
        print('     %s | %s | alpha0 %d palette %d' % r)
    print('VERDICT ' + ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
