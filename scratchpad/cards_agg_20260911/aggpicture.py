#!/usr/bin/env python3
"""Lane CARDS-AGG, gate A3: the CALIBRATED picture gate.

THE QUESTION. Does the aggregate card, at ring-3 distance, show the same
silhouette as the per-tree cards it replaces?

THE REFERENCE, and why it is this one. The thing the aggregate replaces is the
cell's per-tree CARDS, not its meshes -- bungo's ruling of 08:2x is that the
bake "takes each cell's trees, places their cards ... and photographs the whole
cluster". So the reference is the SAME cluster of the same cards, composited at
a frame so fine that it carries no loss of its own: at `--aggregate-tile 1024`
a 4,096-unit cell is 8.2 units a texel, against 23 units a texel inside a tree's
own 48-texel card frame. The reference therefore resolves the cards better than
the cards resolve themselves, which is what makes it a reference and not another
subject.

THE CALIBRATION (ww-control-calibration).

  known answer  the reference against ITSELF                 -> IoU 1.000
  CEILING       the reference box-filtered to the subject's
                own texel pitch and read back                -> the best any
                                                                64-texel frame
                                                                could do
  SUBJECT       the shipped tile-64 aggregate
  FLOOR         a DIFFERENT cell's aggregate, same view      -> must be far
                                                                outside any
                                                                tolerance

A verdict is only reportable when the floor is outside the tolerance and the
subject is at or near the ceiling. If the subject sits far below its own
ceiling, the aggregate is losing something the resolution does not account for,
and that is a refusal with a number.

Everything is compared in WORLD SPACE, never in texels: each sheet's own
`half`, `frame` and `frameOffset` put its texels on the ground, and both are
resampled onto one common grid. Two sheets of different frame sizes have
different extents, and comparing them texel for texel would measure the frame
law rather than the picture (ww-silhouette-compare).
"""

import json
import os
import struct
import sys

import numpy as np


# ---------------------------------------------------------------- DDS / BC3
def _bc4_block(bits, endpoints):
    a0, a1 = endpoints
    # BC4's own interpolation, and the off-by-one that made a lut value read
    # 291 on the first run: the eight-value mode is (6-i)*a0 + (i+1)*a1 over 7,
    # NOT (7-i); the six-value mode is (4-i)*a0 + (i+1)*a1 over 5.
    if a0 > a1:
        lut = [a0, a1] + [((6 - i) * a0 + (i + 1) * a1) // 7 for i in range(6)]
    else:
        lut = [a0, a1] + [((4 - i) * a0 + (i + 1) * a1) // 5 for i in range(4)] + [0, 255]
    return [lut[(bits >> (3 * i)) & 7] for i in range(16)]


def dds_alpha(path):
    """Mip 0's ALPHA plane of a BC3 DDS, as a uint8 array [h, w].

    Only the alpha is decoded: the silhouette is the coverage and the coverage
    lives in the BC3 alpha block, which is a BC4 block and decodes exactly.
    """
    with open(path, 'rb') as f:
        b = f.read()
    assert b[:4] == b'DDS ', path
    h, w = struct.unpack_from('<II', b, 12)
    fourcc = b[84:88]
    off = 128
    if fourcc == b'DX10':
        off = 148
    assert fourcc in (b'DXT5', b'DX10'), (path, fourcc)
    out = np.zeros((h, w), np.uint8)
    bw, bh = (w + 3) // 4, (h + 3) // 4
    p = off
    for by in range(bh):
        for bx in range(bw):
            a0, a1 = b[p], b[p + 1]
            bits = int.from_bytes(b[p + 2:p + 8], 'little')
            vals = _bc4_block(bits, (a0, a1))
            for i in range(16):
                x, y = bx * 4 + (i % 4), by * 4 + (i // 4)
                if x < w and y < h:
                    out[y, x] = vals[i]
            p += 16
    return out


# ------------------------------------------------------------------- sheets
class Sheet:
    """One aggregate set: its alpha sheet plus the geometry that puts it on the
    ground. Every number comes out of the set's own `.lodm`."""

    def __init__(self, stem):
        with open(stem + '.lodm', 'rb') as f:
            b = f.read()
        assert b[:4] == b'LODM'
        n = struct.unpack_from('<I', b, 8)[0]
        self.lodm = json.loads(b[12:12 + n].decode('utf-8'))
        a = self.lodm['aggregate']
        self.views = a['views']
        self.frame = a['frame']
        self.half = a['half']
        self.centre = a['center']
        self.off = a['frameOffset']
        self.cov = a['coverage']
        self.trees = a['trees']
        self.cell = a['cell']
        self.depthSpan = a['depthSpan']
        self.alpha = dds_alpha(stem + '_d.DDS')
        assert self.alpha.shape == (self.frame[1], self.views * self.frame[0]), \
            (stem, self.alpha.shape, self.frame, self.views)

    def coverage(self, view):
        """The view's frame as a COVERAGE FRACTION, 0..1, inverting the sheet's
        own coverage contract. This is the quantity the gate measures: a hard
        alpha test asks a THRESHOLD question, and a canopy at 64 texels is
        mostly partial coverage, so a thresholded comparison would measure the
        test and not the bake (ww-silhouette-compare 3)."""
        fw, fh = self.frame
        a = self.alpha[:, view * fw:(view + 1) * fw].astype(np.float64)
        fl, te, ba = self.cov['floor'], self.cov['test'], self.cov['base']
        cov = fl + (a - ba) * (255.0 - fl) / (255.0 - ba)
        cov = np.where(a < te, 0.0, np.clip(cov, fl, 255.0))
        return cov / 255.0

    def cov_world(self, view, grid, extent):
        """That coverage put on the common world grid, nearest-texel."""
        fw, fh = self.frame
        ox, oy = self.off[2 * view], self.off[2 * view + 1]
        left, top = ox - self.half[0], oy + self.half[1]
        upt_x = 2.0 * self.half[0] / fw
        upt_y = 2.0 * self.half[1] / fh
        nx, ny = grid
        r = np.linspace(extent[0], extent[1], nx, endpoint=False) + (extent[1] - extent[0]) / (2 * nx)
        u = np.linspace(extent[3], extent[2], ny, endpoint=False) - (extent[3] - extent[2]) / (2 * ny)
        tx = np.floor((r - left) / upt_x).astype(int)
        ty = np.floor((top - u) / upt_y).astype(int)
        okx = (tx >= 0) & (tx < fw)
        oky = (ty >= 0) & (ty < fh)
        src = self.coverage(view)
        out = np.zeros((ny, nx))
        sub = src[np.clip(ty, 0, fh - 1)][:, np.clip(tx, 0, fw - 1)]
        out[np.ix_(oky, okx)] = sub[np.ix_(oky, okx)]
        return out

    def mask_world(self, view, grid, extent):
        """The view's silhouette resampled onto a common world grid.

        `extent` is (r0, r1, u0, u1) in the view's own right/up, measured from
        the cell centre. `grid` is (nx, ny).
        """
        fw, fh = self.frame
        ox, oy = self.off[2 * view], self.off[2 * view + 1]
        left, top = ox - self.half[0], oy + self.half[1]
        upt_x = 2.0 * self.half[0] / fw
        upt_y = 2.0 * self.half[1] / fh
        nx, ny = grid
        r = np.linspace(extent[0], extent[1], nx, endpoint=False) + (extent[1] - extent[0]) / (2 * nx)
        u = np.linspace(extent[3], extent[2], ny, endpoint=False) - (extent[3] - extent[2]) / (2 * ny)
        tx = np.floor((r - left) / upt_x).astype(int)
        ty = np.floor((top - u) / upt_y).astype(int)
        okx = (tx >= 0) & (tx < fw)
        oky = (ty >= 0) & (ty < fh)
        src = self.alpha[:, view * fw:(view + 1) * fw]
        out = np.zeros((ny, nx), np.uint8)
        sub = src[np.clip(ty, 0, fh - 1)][:, np.clip(tx, 0, fw - 1)]
        out[np.ix_(oky, okx)] = sub[np.ix_(oky, okx)]
        return out >= self.cov['test']


def box_to_pitch(sheet, view, grid, extent, pitch_w, pitch_h):
    """The reference read at a COARSER texel pitch: box-filter its own frame
    down to (pitch_w, pitch_h) texels, read the coverage back at the same test,
    and put it on the common grid. This is the CEILING -- the best a frame of
    that size could do with this picture."""
    fw, fh = sheet.frame
    src = sheet.alpha[:, view * fw:(view + 1) * fw].astype(np.float64)
    ys = np.linspace(0, fh, pitch_h + 1).astype(int)
    xs = np.linspace(0, fw, pitch_w + 1).astype(int)
    coarse = np.zeros((pitch_h, pitch_w))
    for j in range(pitch_h):
        for i in range(pitch_w):
            blk = src[ys[j]:max(ys[j] + 1, ys[j + 1]), xs[i]:max(xs[i] + 1, xs[i + 1])]
            coarse[j, i] = blk.mean() if blk.size else 0.0
    ox, oy = sheet.off[2 * view], sheet.off[2 * view + 1]
    left, top = ox - sheet.half[0], oy + sheet.half[1]
    upt_x = 2.0 * sheet.half[0] / pitch_w
    upt_y = 2.0 * sheet.half[1] / pitch_h
    nx, ny = grid
    r = np.linspace(extent[0], extent[1], nx, endpoint=False) + (extent[1] - extent[0]) / (2 * nx)
    u = np.linspace(extent[3], extent[2], ny, endpoint=False) - (extent[3] - extent[2]) / (2 * ny)
    tx = np.clip(np.floor((r - left) / upt_x).astype(int), 0, pitch_w - 1)
    ty = np.clip(np.floor((top - u) / upt_y).astype(int), 0, pitch_h - 1)
    return coarse[ty][:, tx] >= sheet.cov['test']


def iou(a, b):
    inter = np.count_nonzero(a & b)
    union = np.count_nonzero(a | b)
    return inter / union if union else 1.0


def area_err(a, b):
    na, nb = np.count_nonzero(a), np.count_nonzero(b)
    return abs(na - nb) / nb if nb else 0.0


def cov_box_to_pitch(sheet, view, grid, extent, pw, ph):
    """The reference's COVERAGE box-filtered to the subject's texel pitch, then
    put on the common grid. A box filter preserves total coverage exactly, so
    this arm's mass error is a KNOWN ANSWER for the metric (it must read ~0) and
    its per-texel error is the honest ceiling for a frame of that size."""
    fw, fh = sheet.frame
    src = sheet.coverage(view)
    ys = np.linspace(0, fh, ph + 1).astype(int)
    xs = np.linspace(0, fw, pw + 1).astype(int)
    coarse = np.zeros((ph, pw))
    for j in range(ph):
        for i in range(pw):
            blk = src[ys[j]:max(ys[j] + 1, ys[j + 1]), xs[i]:max(xs[i] + 1, xs[i + 1])]
            coarse[j, i] = blk.mean() if blk.size else 0.0
    ox, oy = sheet.off[2 * view], sheet.off[2 * view + 1]
    left, top = ox - sheet.half[0], oy + sheet.half[1]
    upt_x = 2.0 * sheet.half[0] / pw
    upt_y = 2.0 * sheet.half[1] / ph
    nx, ny = grid
    r = np.linspace(extent[0], extent[1], nx, endpoint=False) + (extent[1] - extent[0]) / (2 * nx)
    u = np.linspace(extent[3], extent[2], ny, endpoint=False) - (extent[3] - extent[2]) / (2 * ny)
    tx = np.clip(np.floor((r - left) / upt_x).astype(int), 0, pw - 1)
    ty = np.clip(np.floor((top - u) / upt_y).astype(int), 0, ph - 1)
    return coarse[ty][:, tx]


def main():
    subj_dir, ref_dir = sys.argv[1], sys.argv[2]
    tol_mass = float(sys.argv[3]) if len(sys.argv) > 3 else 0.15
    tol_texel = float(sys.argv[4]) if len(sys.argv) > 4 else 0.30

    stems = {}
    for d, tag in ((subj_dir, 'subject'), (ref_dir, 'reference')):
        for f in sorted(os.listdir(d)):
            if f.endswith('_agg.lodm'):
                stems.setdefault(f[:-5], {})[tag] = os.path.join(d, f[:-5])
    cells = sorted(k for k, v in stems.items() if len(v) == 2)
    if not cells:
        print('FAIL: no cell has both a subject and a reference sheet')
        return 2
    print('cells with both arms: %d' % len(cells))

    checks = fails = 0
    rows = []
    grid = (256, 128)
    for name in cells:
        S = Sheet(stems[name]['subject'])
        R = Sheet(stems[name]['reference'])
        other = cells[(cells.index(name) + len(cells) // 2) % len(cells)]
        if other == name:
            other = cells[(cells.index(name) + 1) % len(cells)]
        F = Sheet(stems[other]['subject'])
        assert S.trees == R.trees, (name, S.trees, R.trees)
        for v in range(S.views):
            hx = max(S.half[0], R.half[0]) * 1.05
            hy = max(S.half[1], R.half[1]) * 1.05
            ext = (-hx, hx, -hy, hy)
            cell_area = (2 * hx) * (2 * hy) / (grid[0] * grid[1])   # units^2 a grid cell
            cref = R.cov_world(v, grid, ext)
            csub = S.cov_world(v, grid, ext)
            cceil = cov_box_to_pitch(R, v, grid, ext, S.frame[0], S.frame[1])
            cflo = F.cov_world(v, grid, ext)
            mref = cref.sum() * cell_area
            def mass_err(c):
                return abs(c.sum() * cell_area - mref) / mref if mref else 0.0
            # KNOWN ANSWER 2, taken IN THE FRAME and not on the common grid:
            # a box filter preserves total coverage exactly, so this must read
            # ~0. Doing it on the common grid instead mixes in the nearest-
            # neighbour resample and the padding, which is a different question
            # -- decompose the control stage by stage (ww-control-calibration 2).
            fine = R.coverage(v)
            fine_mass = fine.sum() * (2 * R.half[0] / R.frame[0]) * (2 * R.half[1] / R.frame[1])
            ys = np.linspace(0, R.frame[1], S.frame[1] + 1).astype(int)
            xs = np.linspace(0, R.frame[0], S.frame[0] + 1).astype(int)
            coarse = np.zeros((S.frame[1], S.frame[0]))
            for j in range(S.frame[1]):
                for i in range(S.frame[0]):
                    blk = fine[ys[j]:max(ys[j] + 1, ys[j + 1]), xs[i]:max(xs[i] + 1, xs[i + 1])]
                    coarse[j, i] = blk.mean() if blk.size else 0.0
            coarse_mass = coarse.sum() * (2 * R.half[0] / S.frame[0]) * (2 * R.half[1] / S.frame[1])
            box_err = abs(coarse_mass - fine_mass) / fine_mass if fine_mass else 0.0
            rows.append(dict(
                cell=name, view=v, trees=S.trees, mref=mref, box=box_err,
                known=mass_err(cref), ceil=mass_err(cceil),
                subj=mass_err(csub), floor=mass_err(cflo),
                t_known=float(np.abs(cref - cref).mean()),
                t_ceil=float(np.abs(cceil - cref).mean()),
                t_subj=float(np.abs(csub - cref).mean()),
                t_floor=float(np.abs(cflo - cref).mean()),
                iou_subj=iou(S.mask_world(v, grid, ext), R.mask_world(v, grid, ext)),
                iou_ceil=iou(box_to_pitch(R, v, grid, ext, S.frame[0], S.frame[1]),
                             R.mask_world(v, grid, ext)),
            ))

    def col(k):
        return np.array([r[k] for r in rows])

    print()
    print('== the calibration, over %d cell-views' % len(rows))
    print('   THE MEASURE is the silhouette MASS -- the integral of coverage over the')
    print('   card, in world units squared. It is threshold-free and resolution-free,')
    print('   which a thresholded mask is not.')
    print()
    print('  arm            mass error vs the reference        mean per-texel coverage error')
    for tag, k, kt in (('known answer', 'known', 't_known'), ('CEILING', 'ceil', 't_ceil'),
                       ('SUBJECT', 'subj', 't_subj'), ('FLOOR', 'floor', 't_floor')):
        v, t = col(k), col(kt)
        print('  %-14s mean %.4f  max %.4f            mean %.4f  max %.4f'
              % (tag, v.mean(), v.max(), t.mean(), t.max()))
    print()
    print('  the reference silhouette mass: mean %.0f u^2 over the %d cell-views'
          % (col('mref').mean(), len(rows)))
    print('  the CEILING arm decomposed: box filter in the frame %.5f mean, then the'
          % np.array([r['box'] for r in rows]).mean())
    print('  nearest-neighbour resample onto the common grid takes it to %.4f mean.'
          % col('ceil').mean())
    print('  FOR REFERENCE ONLY, and not the gate: IoU at the 0.5 alpha test reads')
    print('  subject %.3f against a CEILING of %.3f -- a canopy at 64 texels is mostly'
          % (col('iou_subj').mean(), col('iou_ceil').mean()))
    print('  partial coverage, so a hard test measures the test (ww-silhouette-compare 3).')

    def gate(cond, what):
        nonlocal checks, fails
        checks += 1
        print(('ok   ' if cond else 'FAIL ') + what)
        if not cond:
            fails += 1

    known, ceil, subj, floor = col('known'), col('ceil'), col('subj'), col('floor')
    gate(known.max() < 1e-9, 'known answer: the reference against itself is 0 mass error')
    box = col('box')
    gate(box.max() < 0.005,
         'known answer 2: a box filter preserves the mass IN THE FRAME '
         '(mean %.5f, max %.5f)' % (box.mean(), box.max()))
    gate(col('t_subj').mean() <= col('t_ceil').mean() * 1.25,
         'the SUBJECT is at its own CEILING per texel (%.4f vs %.4f)'
         % (col('t_subj').mean(), col('t_ceil').mean()))
    gate(floor.mean() > tol_mass * 2,
         'the FLOOR fails the tolerance it must fail (mass error %.3f > %.3f)'
         % (floor.mean(), tol_mass * 2))
    gate(floor.mean() > subj.mean() * 2,
         'the FLOOR is clearly worse than the subject (%.3f vs %.3f)' % (floor.mean(), subj.mean()))
    gate(subj.mean() <= tol_mass,
         'the SUBJECT meets the mass tolerance (%.4f <= %.3f)' % (subj.mean(), tol_mass))
    gate(subj.max() <= tol_mass * 2,
         'no single cell-view is past twice the tolerance (worst %.4f)' % subj.max())
    gate(col('t_subj').mean() <= tol_texel,
         'the SUBJECT per-texel coverage error is within %.2f (%.4f)' % (tol_texel, col('t_subj').mean()))

    print()
    print('%d checks, %d failures, %s' % (checks, fails, 'PASS' if not fails else 'FAIL'))
    worst = sorted(rows, key=lambda r: -r['subj'])[:5]
    print('the five worst cell-views by mass error:')
    for r in worst:
        print('  %s view %d trees %d: subject %.4f  ceiling %.4f  floor %.4f'
              % (r['cell'], r['view'], r['trees'], r['subj'], r['ceil'], r['floor']))
    return 0 if not fails else 1


if __name__ == '__main__':
    sys.exit(main())
