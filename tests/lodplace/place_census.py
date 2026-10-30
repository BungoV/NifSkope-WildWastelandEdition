"""LODPLACE1 (CODE1030 2a): the far field AS THE CELL VIEW BUILT IT, against the map and the bake.

    python -I place_census.py <streamwalk --place dump> <bake dir> <worldspace> [--tol 50] [--region x0,y0,x1,y1]

The dump (NifSkope -no-gui streamwalk ... --place <file>, src/cellstreamwalk.cpp) holds, in WORLD units:
  I lines  every row of the instanced far object rings: its REFR, origin, the z range of its rotated sub-mesh box,
           the up axis's z and whether it draws now
  S lines  every other shape's world box (near welded shapes, far terrain/water, animated-object copies, ...)
  V lines  every far terrain vertex
Each is put against the LAND height at its XY (VHGT of the bake's own plugins, last one wins) and, for the far object
rows, against the independent decode of the bake's .lodi (tests/spells/lodgen_native_decode.py): a row the READER
moved (origin off its .lodi placement, or flipped against it) is the defect this lane hunts.
Prints `place census: ... VERDICT PASS|FAIL`: FAIL when any drawn far row is moved / flipped by the reader, any drawn
far row floats over its own placement, or any far terrain vertex is off the LAND by more than --vtol.
"""
import argparse, collections, math, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', 'spells'))
import bake_census as B
import lodgen_native_decode as D


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('dump')
    ap.add_argument('bakedir')
    ap.add_argument('ws')
    ap.add_argument('--tol', type=float, default=50.0)
    ap.add_argument('--vtol', type=float, default=1024.0)
    ap.add_argument('--region', default='')
    a = ap.parse_args()
    base = os.path.join(a.bakedir, a.ws + '.')
    reg = [int(v) for v in a.region.split(',')] if a.region else None
    T = D.read_lodi(base + 'lodi')
    L = D.read_lodo(base + 'lodo')
    refs, land = B.read_plugins(B.plugins_of(base + 'lodb'), B.COMMONWEALTH)
    byref = collections.defaultdict(list)
    for r, c in zip(T['instances'], T['cold']):
        byref[c['refFormId']].append(r)
    t = collections.Counter()
    names = collections.defaultdict(collections.Counter)
    samples = collections.defaultdict(list)
    vdev = []
    for line in open(a.dump, encoding='utf-8', errors='replace'):
        f = line.split()
        if not f or f[0].startswith('#'):
            continue
        if f[0] == 'V':
            x, y, z = float(f[2]), float(f[3]), float(f[4])
            if reg and not (reg[0] <= math.floor(x / 4096) <= reg[2] and reg[1] <= math.floor(y / 4096) <= reg[3]):
                continue
            h = B.height_at(land, x, y)
            if h is None:
                continue
            t['V far terrain vertices'] += 1
            d = z - h
            vdev.append(abs(d))
            if abs(d) > a.vtol:
                t['V far terrain vertex off the LAND > vtol'] += 1
                if len(samples['V']) < 12:
                    samples['V'].append('%s at %.0f,%.0f z %.0f LAND %.0f' % (f[1], x, y, z, h))
            continue
        if f[0] == 'I':
            name, ref, x, y, zlo, zhi, upz, drawn = f[1], int(f[3], 16), float(f[5]), float(f[6]), float(f[7]), \
                float(f[8]), float(f[9]), f[10] == '1'
            if reg and not (reg[0] <= math.floor(x / 4096) <= reg[2] and reg[1] <= math.floor(y / 4096) <= reg[3]):
                continue
            t['I far object rows'] += 1
            if not drawn:
                t['I rows hidden (a loaded cell draws them)'] += 1
                continue
            t['I rows drawn'] += 1
            h = B.height_at(land, x, y)
            # the bake's own placement: the same REFR, the nearest XY (a collection's parts share the REFR)
            cand = byref.get(ref, [])
            # every placement of this REFR at the row's XY (a collection's parts share the REFR and can share the XY)
            here = [r for r in cand if max(abs(r['x'] - x), abs(r['y'] - y)) <= 1.0]
            if not cand:
                t['I drawn rows with no .lodi placement of their REFR'] += 1
            elif not here:
                t['I MOVED by the reader: XY off its .lodi placement > 1 u'] += 1
                best = min(cand, key=lambda r: max(abs(r['x'] - x), abs(r['y'] - y)))
                if len(samples['M']) < 12:
                    samples['M'].append('%s ref %08x at %.0f,%.0f, .lodi %.0f,%.0f' % (name, ref, x, y, best['x'], best['y']))
            else:
                # the row's box is a sub-mesh of the placement's model: the corners of its ROTATED box lie within
                # sqrt(3) x the model's bound sphere (the .lodo base's boundRadius x scale) round the .lodi origin
                # (a box cannot be rotated: its corners reach past the sphere the vertices fill)
                def inside(r):
                    R = L['bases'][r['baseId']]['boundRadius'] * r['scaleF'] * 1.7321 + 2.0
                    return zlo >= r['z'] - R and zhi <= r['z'] + R
                if not any(inside(r) for r in here):
                    t['I MOVED by the reader: Z box outside its placement bound'] += 1
                    if len(samples['Z']) < 12:
                        r = here[0]
                        samples['Z'].append('%s ref %08x z %.0f..%.0f .lodi z %.0f R %.0f' % (
                            name, ref, zlo, zhi, r['z'], L['bases'][r['baseId']]['boundRadius'] * r['scaleF']))
                if upz < -0.1 and not any(r['m'][8] < -0.1 for r in here):
                    t['I FLIPPED by the reader (up z < 0, no .lodi placement there is)'] += 1
            if upz < -0.1:
                t['I drawn rows with up z < 0'] += 1
            if h is not None:
                if zhi < h - a.tol:
                    t['I drawn rows wholly under the LAND - tol'] += 1
                    names['I under'][name.split('_')[0] + '_' + (name.split('_')[1] if '_' in name else '')] += 1
                if zlo > h + 4096.0:
                    t['I drawn rows floating > 4096 u over the LAND'] += 1
                    if len(samples['F']) < 12:
                        samples['F'].append('%s ref %08x at %.0f,%.0f z %.0f..%.0f LAND %.0f' % (name, ref, x, y, zlo, zhi, h))
            continue
        if f[0] == 'S':
            name = f[1]
            xlo, ylo, zlo, xhi, yhi, zhi, drawn = [float(v) for v in f[5:11]] + [f[11] == '1']
            cx, cy = (xlo + xhi) / 2, (ylo + yhi) / 2
            if reg and not (reg[0] <= math.floor(cx / 4096) <= reg[2] and reg[1] <= math.floor(cy / 4096) <= reg[3]):
                continue
            kind = 'far' if name.startswith('FarLOD') else 'other'
            t['S %s shapes' % kind] += 1
            if not drawn:
                continue
            # the lowest LAND over the box's footprint corners and centre: a shape wholly under all of it is under
            hs = [B.height_at(land, px, py) for px in (xlo, cx, xhi) for py in (ylo, cy, yhi)]
            hs = [v for v in hs if v is not None]
            if not hs:
                continue
            if zhi < min(hs) - a.tol and (xhi - xlo) < 65536:
                t['S %s shapes drawn wholly under the LAND - tol' % kind] += 1
                if len(samples['S' + kind]) < 15:
                    samples['S' + kind].append('%s box %.0f,%.0f,%.0f .. %.0f,%.0f,%.0f LAND min %.0f'
                                               % (name, xlo, ylo, zlo, xhi, yhi, zhi, min(hs)))
            if zlo > max(hs) + 8192.0:
                t['S %s shapes drawn wholly > 8192 u over the LAND' % kind] += 1
                if len(samples['K' + kind]) < 15:
                    samples['K' + kind].append('%s box %.0f,%.0f,%.0f .. %.0f,%.0f,%.0f LAND max %.0f'
                                               % (name, xlo, ylo, zlo, xhi, yhi, zhi, max(hs)))
    vdev.sort()
    if vdev:
        t['V |z - LAND| median'] = int(vdev[len(vdev) // 2])
        t['V |z - LAND| p99'] = int(vdev[int(len(vdev) * 0.99)])
        t['V |z - LAND| max'] = int(vdev[-1])
    print('place census %s (region %s)' % (os.path.basename(a.dump), a.region or 'all'))
    for k in sorted(t):
        print('  %-62s %8d' % (k, t[k]))
    for k in sorted(samples):
        print('  samples %s:' % k)
        for s in samples[k]:
            print('    ' + s)
    for k, c in names.items():
        print('  %s by shape: %s' % (k, ', '.join('%s %d' % kv for kv in c.most_common(8))))
    bad = (t['I MOVED by the reader: XY off its .lodi placement > 1 u'] + t['I MOVED by the reader: Z box outside its placement bound']
           + t['I FLIPPED by the reader (up z < 0, no .lodi placement there is)'] + t['V far terrain vertex off the LAND > vtol'])
    ok = bad == 0 and t['I rows drawn'] > 0
    print('place census: reader-moved/flipped rows + off terrain vertices %d, drawn rows %d VERDICT %s'
          % (bad, t['I rows drawn'], 'PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
