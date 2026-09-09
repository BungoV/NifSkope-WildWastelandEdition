"""Measure terrain-LOD vertex spacing from a `nifskope-cli verts` dump.

Input: <stem>.verts.txt (block lines "b <n> <type> '<name>'", vertex lines
"v x y z [ident] [uv u v]") and <stem>.world.txt for the per-shape SCALE.
The BTR stores LOD vertices in miniature space; the world position is the
local coordinate times the shape's world scale (4/8/16/32).

Prints, per shape: vertex count, world extent, the sorted unique world x and
y coordinates, the gaps between them, and the implied samples per 4096-unit
cell against the LAND grid's 128 units.
"""
import re
import sys
from collections import Counter


def read_world(path):
    scale = {}
    for line in open(path, encoding='utf-8-sig'):
        m = re.match(r"\[(\d+)\]\s+(\S+)\s+'?(.*?)'?\s+T=.*S=([\d.]+)", line.strip())
        if m:
            scale[int(m.group(1))] = float(m.group(4))
    return scale


def read_verts(path):
    shapes = []
    cur = None
    for line in open(path, encoding='utf-8-sig'):
        line = line.strip()
        if line.startswith('b '):
            p = line.split(None, 3)
            cur = {'block': int(p[1]), 'type': p[2],
                   'name': p[3].strip("'") if len(p) > 3 else '', 'v': []}
            shapes.append(cur)
        elif line.startswith('v ') and cur is not None:
            p = line.split()
            cur['v'].append((float(p[1]), float(p[2]), float(p[3])))
    return shapes


def gaps(vals):
    return [round(b - a, 6) for a, b in zip(vals, vals[1:])]


def report(stem, label):
    scale = read_world(stem + '.world.txt')
    for sh in read_verts(stem + '.verts.txt'):
        s = scale.get(sh['block'], 1.0)
        v = sh['v']
        if not v:
            continue
        wx = sorted({round(p[0] * s, 4) for p in v})
        wy = sorted({round(p[1] * s, 4) for p in v})
        wz = [p[2] * s for p in v]
        gx, gy = gaps(wx), gaps(wy)
        allg = gx + gy
        print(f"--- {label} block {sh['block']} {sh['type']} '{sh['name']}'")
        print(f"    vertices        {len(v)}  (unique xy {len({(p[0], p[1]) for p in v})})")
        print(f"    shape scale     {s}")
        print(f"    world extent x  {wx[0]} .. {wx[-1]}   span {wx[-1] - wx[0]}")
        print(f"    world extent y  {wy[0]} .. {wy[-1]}   span {wy[-1] - wy[0]}")
        print(f"    world z         {min(wz):.1f} .. {max(wz):.1f}")
        print(f"    unique x        {len(wx)}   unique y {len(wy)}")
        if allg:
            print(f"    gap min/max     {min(allg)} / {max(allg)}")
            print(f"    gap histogram   {sorted(Counter(allg).items())[:12]}")
        # occupancy against the full grid the minimum gap implies
        if gx and min(gx) > 0:
            step = min(allg)
            nx = int(round((wx[-1] - wx[0]) / step)) + 1
            ny = int(round((wy[-1] - wy[0]) / step)) + 1
            print(f"    implied grid    {nx} x {ny} at step {step}"
                  f"  -> {nx * ny} full-grid points, mesh has "
                  f"{len({(p[0], p[1]) for p in v})} "
                  f"({100.0 * len({(p[0], p[1]) for p in v}) / (nx * ny):.1f}%)")
            print(f"    step vs LAND    {step} units = {step / 128.0:.3f} x 128")
        print()


if __name__ == '__main__':
    for a in sys.argv[1:]:
        stem, _, label = a.partition('=')
        report(stem, label or stem)
