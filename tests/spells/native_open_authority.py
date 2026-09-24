#!/usr/bin/env python
"""Right-hand side for `tests/spells/native_open.sh` -- the native-bake VIEWER
gates.

Three jobs, and none of them is a second parser:

  chunk <ws.lodo> <ws.lodi> <cellX> <cellY> <dim>
      the instances the `.lodi` ITSELF puts in that chunk, read through
      `lodgen_native_decode.py` -- the independent decoder written from the
      format document, sharing no code with src/lodifile.cpp. Prints a header
      line `chunk.instances N` and then one row per instance:
      `<ref hex> <part> <base hex> <x> <y> <z> <scale>` -- the census's own
      column order, so one reader parses both sides.

  compare <census.txt> <authority.txt> [tolerance]
      the viewer's own census (`WW_LODI_DUMP`, written by the builder that
      PLACED each instance) against that list: same count, same (ref, part)
      keys, and every world position within `tolerance` units (default 1).

  iou <a.png> <b.png>          coverage-mask intersection-over-union
  mad <a.png> <b.png> [mask]   mean absolute colour difference, 0..255

The two picture jobs take the background to be the colour of the four corner
pixels when they agree, which is what `WW_RENDER_CLEAN=1` guarantees; a pixel
is COVERED when it differs from that colour by more than 12 on any channel.
`mad` averages over the union of the two masks, i.e. over the chunk footprint,
so an empty margin cannot flatter the number.

Exit 0 = the comparison passed; 1 = it failed, named; 2 = it could not be run.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

CELL_UNITS = 4096.0


def die(msg, rc=2):
    print('REFUSED: %s' % msg)
    sys.exit(rc)


def load_pair(lodo, lodi):
    try:
        import lodgen_native_decode as D
    except ImportError as e:
        die('cannot import lodgen_native_decode.py: %s' % e)
    try:
        L = D.read_lodo(lodo)
        T = D.read_lodi(lodi)
    except Exception as e:
        die('%s' % e)
    return L, T


def chunk_indices(T, cx0, cy0, dim):
    """Every chunk-table index whose 4x4-cell chunk lies in the cell rectangle
    [cx0, cx0+dim-1] x [cy0, cy0+dim-1]. The table is DENSE and NORTH-UP: row 0
    is the northmost chunk row (docs/LODGEN_NATIVE_LODO_LODI.md)."""
    h = T['header']
    cells = h['chunkCells']
    if dim % cells or cx0 % cells or cy0 % cells:
        die('a chunk rectangle is a multiple of %d cells, aligned to %d' % (cells, cells))
    w = h['chunkEast'] - h['chunkWest'] + 1
    out = []
    for cy in range(cy0 // cells, (cy0 + dim) // cells):
        for cx in range(cx0 // cells, (cx0 + dim) // cells):
            if not (h['chunkWest'] <= cx <= h['chunkEast']):
                continue
            if not (h['chunkSouth'] <= cy <= h['chunkNorth']):
                continue
            row = h['chunkNorth'] - cy
            out.append(row * w + (cx - h['chunkWest']))
    return out


def job_chunk(argv):
    if len(argv) < 5:
        die('chunk <lodo> <lodi> <cellX> <cellY> <dim>')
    lodo, lodi = argv[0], argv[1]
    cx0, cy0, dim = int(argv[2]), int(argv[3]), int(argv[4])
    L, T = load_pair(lodo, lodi)
    baseForm = [b['formId'] for b in L['bases']]
    rows = []
    for ci in chunk_indices(T, cx0, cy0, dim):
        c = T['chunks'][ci]
        for i in range(c['instanceFirst'], c['instanceFirst'] + c['instanceCount']):
            r = T['instances'][i]
            k = T['cold'][i]
            rows.append((k['refFormId'], k['scolPart'], baseForm[r['baseId']],
                         r['x'], r['y'], r['z'], r['scaleF']))
    print('chunk.instances %d' % len(rows))
    for r in rows:
        print('%08X %d %08X %.4f %.4f %.4f %.6f' % r)
    return 0


def read_rows(path, kind):
    """`ref part base x y z ...` rows out of either file; `#` = comment."""
    out = {}
    n = 0
    try:
        fh = open(path, encoding='utf-8')
    except OSError as e:
        die('%s: %s' % (kind, e))
    with fh:
        for line in fh:
            line = line.strip()
            if not line or line.startswith('#') or line.startswith('chunk.instances'):
                continue
            t = line.split()
            if len(t) < 6:
                continue
            try:
                key = (int(t[0], 16), int(t[1]))
                pos = (float(t[3]), float(t[4]), float(t[5]))
            except ValueError:
                continue
            n += 1
            out.setdefault(key, []).append(pos)
    return out, n


def job_compare(argv):
    if len(argv) < 2:
        die('compare <census> <authority> [tolerance]')
    tol = float(argv[2]) if len(argv) > 2 else 1.0
    got, gotN = read_rows(argv[0], 'census')
    want, wantN = read_rows(argv[1], 'authority')
    print('  census %d rows, authority %d rows' % (gotN, wantN))
    fails = 0
    if gotN != wantN:
        print('  FAIL counts differ: %d against %d' % (gotN, wantN))
        fails += 1
    missing = [k for k in want if k not in got]
    extra = [k for k in got if k not in want]
    if missing:
        print('  FAIL %d (ref,part) keys the viewer did not place, first %s'
              % (len(missing), ['%08X/%d' % k for k in missing[:4]]))
        fails += 1
    if extra:
        print('  FAIL %d (ref,part) keys the viewer invented, first %s'
              % (len(extra), ['%08X/%d' % k for k in extra[:4]]))
        fails += 1
    worst = 0.0
    worstKey = None
    off = 0
    for k, ws in want.items():
        if k not in got:
            continue
        gs = got[k]
        for i in range(min(len(ws), len(gs))):
            d = max(abs(ws[i][j] - gs[i][j]) for j in range(3))
            if d > worst:
                worst, worstKey = d, k
            if d > tol:
                off += 1
    print('  worst position difference %.4f u (%s), %d past %.3f u'
          % (worst, ('%08X/%d' % worstKey) if worstKey else '-', off, tol))
    if off:
        fails += 1
    return 1 if fails else 0


def load_png(path):
    try:
        import numpy as np
        from PIL import Image
    except ImportError:
        die('numpy and PIL are needed for the picture jobs')
    try:
        im = Image.open(path).convert('RGB')
    except OSError as e:
        die('%s: %s' % (path, e))
    return np.asarray(im, dtype=int)


def mask_of(a, tol=12):
    """Non-background pixels. `WW_RENDER_CLEAN=1` paints one flat background, so
    the four corners agree and ARE that colour; when they do not agree the modal
    corner is taken and the run says so, because a silent guess would make the
    number unreadable."""
    h, w = a.shape[0], a.shape[1]
    corners = [tuple(a[0, 0]), tuple(a[0, w - 1]), tuple(a[h - 1, 0]), tuple(a[h - 1, w - 1])]
    bg = corners[0]
    if len(set(corners)) != 1:
        from collections import Counter
        bg = Counter(corners).most_common(1)[0][0]
        print('  note: the four corners are not one colour; background taken as %s' % (bg,))
    import numpy as np
    d = np.abs(a - np.array(bg, dtype=int))
    return (d.max(axis=2) > tol), bg


def job_iou(argv):
    if len(argv) < 2:
        die('iou <a.png> <b.png>')
    a = load_png(argv[0])
    b = load_png(argv[1])
    if a.shape != b.shape:
        die('sizes differ: %s against %s' % (a.shape, b.shape))
    ma, _ = mask_of(a)
    mb, _ = mask_of(b)
    inter = int((ma & mb).sum())
    union = int((ma | mb).sum())
    iou = (inter / union) if union else 0.0
    print('  A covers %d px, B covers %d px, intersection %d, union %d'
          % (int(ma.sum()), int(mb.sum()), inter, union))
    print('IOU %.4f' % iou)
    return 0


def job_cover(argv):
    """ONE-SIDED coverage of B by A, and how much wider A is (lane BTOFREE1).

    The .lodi scene and the .BTO of the same chunk are not the same geometry:
    the scene places whole library models, the chunk carries a mesh that the
    bake merged, cut at the far rings and clipped to the chunk box. Asking
    them for the same SILHOUETTE (IoU) asks for something neither the bake nor
    the viewer promises. What the pair does promise is that the scene draws
    every object the chunk draws, in the same place, at about the same size.

    COVER is the first half of that and FAT is the second; each is vacuous on
    its own and the two together are not. IOU is still printed because it is
    the number every earlier run of this gate printed, but nothing is gated on
    it any more."""
    if len(argv) < 2:
        die('cover <a.png> <b.png>')
    import numpy as np
    a = load_png(argv[0])
    b = load_png(argv[1])
    if a.shape != b.shape:
        die('sizes differ: %s against %s' % (a.shape, b.shape))
    ma, _ = mask_of(a)
    mb, _ = mask_of(b)
    na, nb = int(ma.sum()), int(mb.sum())
    inter = int((ma & mb).sum())
    union = int((ma | mb).sum())
    flip = int((ma & mb[::-1, :]).sum())
    print('  A covers %d px, B covers %d px, intersection %d, union %d'
          % (na, nb, inter, union))
    print('COVER %.4f' % ((inter / nb) if nb else 0.0))
    print('FAT %.4f' % ((na / nb) if nb else 0.0))
    print('COVERFLIP %.4f' % ((flip / nb) if nb else 0.0))
    print('IOU %.4f' % ((inter / union) if union else 0.0))
    return 0


def job_solid(argv):
    """Write a mask-shaped REFUTER: every pixel of the frame is foreground.

    It exists so the harness can show FAT failing on an input that scores a
    perfect COVER, which is the whole reason COVER is not gated alone."""
    if len(argv) < 2:
        die('solid <like.png> <out.png>')
    import numpy as np
    a = load_png(argv[0])
    h, w = a.shape[0], a.shape[1]
    img = np.full((h, w, 3), 200, dtype=np.uint8)
    img[0, 0] = img[0, w - 1] = img[h - 1, 0] = img[h - 1, w - 1] = (0, 0, 0)
    from PIL import Image
    Image.fromarray(img).save(argv[1])
    print('  wrote a solid %dx%d refuter frame' % (w, h))
    return 0


def job_mad(argv):
    if len(argv) < 2:
        die('mad <a.png> <b.png>')
    import numpy as np
    a = load_png(argv[0])
    b = load_png(argv[1])
    if a.shape != b.shape:
        die('sizes differ: %s against %s' % (a.shape, b.shape))
    ma, _ = mask_of(a)
    mb, _ = mask_of(b)
    m = ma | mb
    n = int(m.sum())
    if not n:
        print('  compared over 0 px')
        print('MAD 0.000')
        return 0
    d = np.abs(a - b).mean(axis=2)
    mad = float(d[m].mean())
    print('  compared over %d px (the union of the two footprints)' % n)
    print('MAD %.3f' % mad)
    return 0


def job_manifest(argv):
    """manifest <manifest.txt> <cellX> <cellY> <dim>

    A .BTO manifest carries every placement the chunk DRAWS, and that includes a
    few whose origin sits just outside the chunk's own cells (measured on
    (-20,24) dim 4: two trees, 46 and 117 units out).  The .lodi chunk table is
    keyed by the cell the origin falls in, so the two counts can only be
    compared after those rows are separated out.  Both counts are printed."""
    path, cx, cy, dim = argv[0], int(argv[1]), int(argv[2]), int(argv[3])
    x0 = cx * 4096.0
    x1 = (cx + dim) * 4096.0
    y0 = cy * 4096.0
    y1 = (cy + dim) * 4096.0
    inside = 0
    outside = []
    for line in open(path, encoding='utf-8', errors='replace'):
        line = line.strip()
        if not line or line.startswith('#'):
            continue
        t = line.split()
        if len(t) < 11 or not t[0].isdigit():
            continue          # the I / A / M rows of the same file
        try:
            x, y = float(t[3]), float(t[4])
        except ValueError:
            continue
        if x0 <= x <= x1 and y0 <= y <= y1:
            inside += 1
        else:
            outside.append((t[9], t[10], x, y))
    print('manifest.rows %d' % (inside + len(outside)))
    print('manifest.inside %d' % inside)
    print('manifest.outside %d' % len(outside))
    for ref, part, x, y in outside[:20]:
        dx = 0.0 if x0 <= x <= x1 else (x0 - x if x < x0 else x - x1)
        dy = 0.0 if y0 <= y <= y1 else (y0 - y if y < y0 else y - y1)
        print('  outside %s %s by %.1f,%.1f units' % (ref, part, dx, dy))
    return 0


def job_ncc(argv):
    """ncc <a.png> <b.png>

    Normalised cross-correlation of the two lumas, plus the same number with b
    mirrored in Y.  Structure, not tone: it survives the sheets being coarser
    and darker than the legacy per-chunk texture, and it goes to nothing when
    the picture is the wrong chunk or the wrong way up."""
    import numpy as np
    a = load_png(argv[0])
    b = load_png(argv[1])
    h = min(a.shape[0], b.shape[0])
    w = min(a.shape[1], b.shape[1])
    A = a[:h, :w].mean(axis=2)
    B = b[:h, :w].mean(axis=2)

    def ncc(x, y):
        x = x - x.mean()
        y = y - y.mean()
        d = float(np.sqrt((x * x).sum() * (y * y).sum()))
        return float((x * y).sum() / d) if d else 0.0

    print('  compared over %d x %d px' % (w, h))
    print('NCC %.4f' % ncc(A, B))
    print('NCCFLIP %.4f' % ncc(A, B[::-1, :]))
    return 0


JOBS = {'chunk': job_chunk, 'compare': job_compare, 'cover': job_cover,
        'iou': job_iou, 'mad': job_mad, 'manifest': job_manifest,
        'ncc': job_ncc, 'solid': job_solid}

if __name__ == '__main__':
    if len(sys.argv) < 2 or sys.argv[1] not in JOBS:
        die('usage: %s {chunk|compare|cover|iou|mad|manifest|ncc|solid} ...'
            % os.path.basename(sys.argv[0]))
    sys.exit(JOBS[sys.argv[1]](sys.argv[2:]))
