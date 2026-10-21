#!/usr/bin/env python3
"""Lane GISHADE1's Goodneighbor sky-visibility gate (2026-10-13), from a run's GI dump (WW_CELL_GI_DUMP).

  cell_gn_vis_check.py <dump> [--fixture tests/fixtures/gishade1_gn_vis.tsv] [--expect pass|fail]

The fixture holds the glass camera's surface points (one per 16 px of 1280x720) and the tracer's cosine sky share
there (bounce_reference, uniform sky). Each point is sampled from the dump's sky-share grid as the shader does
(cell_rooms_check.gi_sample over gi_skygrid + rooms + slots, 2 units off the surface, no gap fill): k = the share of
the flat ambient the grid replaces, vis = the grid's sky share x k. The frame's sky fill, in units of the flat
ambient, under the linear replacement: vis + (1 - k). Regions, chosen by geometry:
  a open ground     up-facing points (tracer share > 0.001) with no ceiling point within 150 u in xy
  b under canopy    up-facing points with a ceiling point within 40 u in xy (the Rexford canopy)
  c door recess     pixels 470..550 x 330..470
PASS: b/a of the frame's sky fill within 0.12 of the tracer's b/a, and c's fill within 0.04 (of the flat ambient) of
the tracer's c share. c/a is reported only: a 1 m recess is below the probes' 4 m spacing.
One verdict line last ("VIS GATE PASS|FAIL"); exit 0 on PASS (or on FAIL with --expect fail), else 1.
"""
import argparse
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cell_rooms_check import read_grid, read_rooms, gi_sample  # noqa: E402


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument('dump')
    ap.add_argument('--fixture', default=os.path.join(here, '..', 'fixtures', 'gishade1_gn_vis.tsv'))
    ap.add_argument('--expect', default='pass')
    a = ap.parse_args()
    du = a.dump
    G = read_grid(du + '/gi_skygrid.bin', du + '/gi_slots.bin')
    nv, d = G['nv'], G['dims']
    # the sky part of gi_slots.bin: slot 1's sky grid (the shader's sky lookup reads that one)
    G['g2'] = np.frombuffer(open(du + '/gi_slots.bin', 'rb').read(), '<f4', 6 * nv * 4,
                            32 + 8 * nv + 6 * nv * 4 * 4).reshape(6, d[2], d[1], d[0], 4)
    R = read_rooms(du + '/gi_rooms.bin')
    F = np.loadtxt(a.fixture, comments='#')
    uv, P, N, sh = F[:, :2].astype(int), F[:, 2:5], F[:, 5:8], F[:, 8]
    pv = np.array([gi_sample(G, R, p - n * 2, n, fill=False) for p, n in zip(P, N)])
    k = np.clip(pv[:, 3], 0, 1)
    vis = pv[:, :3].mean(1)
    fill = vis * k + (1 - k)	# as GISHADE1's gate_vis.py wrote it (its 0.639)
    ce = N[:, 2] < -0.9
    from scipy.spatial import cKDTree
    dd, _ = cKDTree(P[ce, :2]).query(P[:, :2])
    up = (N[:, 2] > 0.9) & (sh > 0.001)	# a tracer share of exactly 0 facing up: a point inside a sandbag
    reg = {'a open ground': up & (dd > 150), 'b under canopy': up & (dd < 40),
           'c door recess': (uv[:, 0] >= 470) & (uv[:, 0] < 550) & (uv[:, 1] >= 330) & (uv[:, 1] < 470)}
    S = {}
    for nm, m in reg.items():
        if not m.any():
            print('FAIL  region %s is empty (fixture or dump mismatch)' % nm)
            print('VIS GATE FAIL')
            return 0 if a.expect == 'fail' else 1
        S[nm] = (sh[m].mean(), vis[m].mean(), fill[m].mean())
        print('%-15s %4d pts  tracer %.3f  grid %.3f  frame sky fill %.3f' % ((nm, m.sum()) + S[nm]))
    ra, rb, rc = S['a open ground'], S['b under canopy'], S['c door recess']
    bt, bg, bf = rb[0] / ra[0], rb[1] / max(ra[1], 1e-9), rb[2] / max(ra[2], 1e-9)
    okb = abs(bf - bt) <= 0.12
    print('b/(a): tracer %.3f  grid %.3f  frame %.3f (%s, tol 0.12)' % (bt, bg, bf, 'PASS' if okb else 'FAIL'))
    print('c/(a): tracer %.3f  grid %.3f  frame %.3f   [reported, not gated]' % (
        rc[0] / ra[0], rc[1] / max(ra[1], 1e-9), rc[2] / max(ra[2], 1e-9)))
    dc = abs(rc[2] - rc[0])
    okc = dc <= 0.04
    print('c absolute (fill vs tracer share): %.3f (%s, tol 0.04 of the flat ambient)' % (dc, 'PASS' if okc else 'FAIL'))
    ok = okb and okc
    print('VIS GATE ' + ('PASS' if ok else 'FAIL'))
    return 0 if ok == (a.expect != 'fail') else 1


if __name__ == '__main__':
    sys.exit(main())
