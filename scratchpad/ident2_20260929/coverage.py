"""IDENT1 occluder coverage from street level, read from the FILE's bytes.

usage: python coverage.py <lod base path without extension> [--json out.json] [--png prefix]
                          [--eyes "x,y;x,y;x,y"] [--region x0,y0,x1,y1]

For each eye (street level: z = the top of the nearest road piece + 128 u, the player's eye height):
  * a cylindrical panorama, 360 degrees of azimuth by -10..+40 degrees of elevation, 0.25 degree a pixel;
  * SKYLINE = the pixels any placed level-0 triangle of the region covers (the Boston box by default);
  * OCCLUDED = skyline pixels an occluder box also covers (exact ray/box slab test per pixel). Boxes that
    contain the eye are left out, as a culler must;
  * CULLED = placements a conservative Hi-Z test would drop: the placement's whole angular rectangle is
    under occluders, and its nearest vertex is farther than the farthest occluder depth in that rectangle.
Triangles are drawn as the flat polygon of their three projected corners (PIL), which bends a very near
triangle a little; boxes are exact. Prints one row an eye and the mean."""
import sys, os, json, math, collections
import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, 'E:/Projects/NifskopeWWE-ident2/tests/spells')
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lodgen_native_decode as ND
import lodl_channels_table as CT
from lodi_occluder_building import placed_tris, first_mesh, NO_MESH

RES = 0.25
AZN = int(360 / RES)
EL0, EL1 = -10.0, 40.0
ELN = int((EL1 - EL0) / RES)
EYE_H = 128.0
# three street points (world units): Back Bay (Boylston St. at Copley, west of Trinity), downtown between the
# two tall towers, and the Fens/Kenmore approach to the ballpark. Each is snapped to the nearest road piece.
EYES = [(-8900.0, -24700.0), (700.0, -27200.0), (-20500.0, -21600.0)]


def main():
    argv = list(sys.argv[1:])
    opt = {}
    for key in ('--json', '--png', '--eyes', '--region', '--extra'):
        if key in argv:
            k = argv.index(key); opt[key] = argv[k + 1]; del argv[k:k + 2]
    base = argv[0]
    region = [int(v) for v in opt.get('--region', '-8,-12,3,-1').split(',')]
    eyes = EYES if '--eyes' not in opt else [tuple(float(v) for v in e.split(',')) for e in opt['--eyes'].split(';')]
    L = ND.read_lodo(base + '.lodo')
    T = ND.read_lodi(base + '.lodi')
    s_at = L['string_at']
    inst = T['instances']
    sel = []
    for ii, r in enumerate(inst):
        cx, cy = int(r['x'] // 4096), int(r['y'] // 4096)
        if not (region[0] <= cx <= region[2] and region[1] <= cy <= region[3]):
            continue
        me = first_mesh(L, r)
        if me == NO_MESH:
            continue
        sel.append((ii, me))
    tris = {}
    for ii, me in sel:
        tris[ii] = placed_tris(L, inst[ii], me).astype(np.float32)
    names = {ii: s_at(L['meshes'][me]['modelStringOffset']).lower() for ii, me in sel}
    roads = [ii for ii in tris if 'road' in names[ii] and len(tris[ii])]
    occ = []
    for o in T['occluders']:
        R = np.array(ND.unpack_rotation(o['r0'], o['r1'], o['r2'])).reshape(3, 3)
        occ.append((np.array([o['x'], o['y'], o['z']]), R, np.array([o['hx'], o['hy'], o['hz']])))
    if '--extra' in opt:            # hills.py --json rows: boxes proposed, not in the file
        for b in json.load(open(opt['--extra']))['rows']:
            a = math.radians(b['yawDeg'])
            R = np.array([[math.cos(a), -math.sin(a), 0], [math.sin(a), math.cos(a), 0], [0, 0, 1]])
            occ.append((np.array(b['centre']), R, np.array(b['half'])))
    # the pixel ray directions
    az = (np.arange(AZN) + 0.5) * RES
    el = EL1 - (np.arange(ELN) + 0.5) * RES
    AZ, EL = np.meshgrid(np.radians(az), np.radians(el))
    D = np.stack([np.cos(EL) * np.cos(AZ), np.cos(EL) * np.sin(AZ), np.sin(EL)], -1)     # (ELN, AZN, 3)
    rows = []
    for e_i, (ex, ey) in enumerate(eyes):
        # street level: the median of the lowest placed vertex of the 9 placements nearest the point (the feet
        # of the buildings around it). LOD has almost no street pieces, so a road snap lands kilometres away.
        near = []
        for ii, t in tris.items():
            if not len(t):
                continue
            v = t.reshape(-1, 3)
            lo = v.min(0); hi = v.max(0)
            dx = max(lo[0] - ex, 0, ex - hi[0]); dy = max(lo[1] - ey, 0, ey - hi[1])
            near.append((math.hypot(dx, dy), float(lo[2]), names[ii]))
        near.sort()
        zg = float(np.median([n[1] for n in near[:9]]))
        best = (near[0][0], None, zg, near[0][2])
        E = np.array([ex, ey, zg + EYE_H])
        inside = sum(1 for n in near if n[0] == 0.0)
        # ---- skyline mask
        sky = Image.new('L', (AZN, ELN), 0)
        dr = ImageDraw.Draw(sky)
        pbox = {}
        for ii, t in tris.items():
            if not len(t):
                continue
            v = t.reshape(-1, 3) - E
            dist = np.linalg.norm(v, axis=1)
            a = (np.degrees(np.arctan2(v[:, 1], v[:, 0])) % 360.0) / RES
            h = np.hypot(v[:, 0], v[:, 1])
            b = (EL1 - np.degrees(np.arctan2(v[:, 2], np.maximum(h, 1e-6)))) / RES
            a = a.reshape(-1, 3); b = b.reshape(-1, 3)
            near = dist.reshape(-1, 3).min(1) < 32.0
            for k in range(len(a)):
                if near[k]:
                    continue
                xs = a[k]
                if xs.max() - xs.min() > AZN / 2:           # across the seam: draw on both sides
                    xs = np.where(xs < AZN / 2, xs + AZN, xs)
                    dr.polygon([(xs[j] - AZN, b[k][j]) for j in range(3)], fill=255)
                dr.polygon([(xs[j], b[k][j]) for j in range(3)], fill=255)
            # the placement's angular rectangle (a placement across the seam is skipped for the Hi-Z count)
            aa = a.reshape(-1); bb = b.reshape(-1)
            if aa.max() - aa.min() < AZN / 2 and dist.min() > 32.0:
                pbox[ii] = (int(aa.min()), int(aa.max()), int(max(bb.min(), 0)), int(min(bb.max(), ELN - 1)),
                            float(dist.min()))
        skym = np.asarray(sky) > 0
        # ---- occluder depth, exact per pixel
        depth = np.full((ELN, AZN), np.inf, np.float32)
        used = 0
        for c, R, hh in occ:
            lc = R.T @ (E - c)
            if np.all(np.abs(lc) <= hh):
                continue                                    # the eye is inside this box
            if np.linalg.norm(E - c) - np.linalg.norm(hh) > 60000.0:
                continue
            used += 1
            # the box's angular bounds from its corners (plus a pixel)
            cs = np.array([[sx, sy, sz] for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)]) * hh
            wc = c + cs @ R.T - E
            ca = (np.degrees(np.arctan2(wc[:, 1], wc[:, 0])) % 360.0) / RES
            ch = np.hypot(wc[:, 0], wc[:, 1])
            cb = (EL1 - np.degrees(np.arctan2(wc[:, 2], np.maximum(ch, 1e-6)))) / RES
            y0, y1 = int(max(cb.min() - 1, 0)), int(min(cb.max() + 2, ELN))
            if y0 >= y1:
                continue
            if ca.max() - ca.min() > AZN / 2:
                spans = [(0, AZN)]
            else:
                spans = [(int(max(ca.min() - 1, 0)), int(min(ca.max() + 2, AZN)))]
            for x0, x1 in spans:
                d = D[y0:y1, x0:x1] @ R                       # ray directions in the box frame
                o = lc
                with np.errstate(divide='ignore', invalid='ignore'):
                    t1 = (-hh - o) / d; t2 = (hh - o) / d
                tn = np.nanmax(np.minimum(t1, t2), -1); tf = np.nanmin(np.maximum(t1, t2), -1)
                hit = (tf >= tn) & (tf > 0)
                tn = np.where(tn > 0, tn, 0)
                sub = depth[y0:y1, x0:x1]
                depth[y0:y1, x0:x1] = np.where(hit, np.minimum(sub, tn), sub)
        occm = np.isfinite(depth)
        skyn = int(skym.sum()); hidn = int((skym & occm).sum())
        # ---- conservative Hi-Z per placement
        culled = 0
        for ii, (x0, x1, y0, y1, dmin) in pbox.items():
            if y1 < y0:
                continue
            blk = depth[y0:y1 + 1, x0:x1 + 1]
            if blk.size and np.all(np.isfinite(blk)) and dmin > float(blk.max()):
                culled += 1
        rows.append({'eye': [round(float(v)) for v in E], 'nearestPiece': best[3], 'nearestDist': round(best[0]),
                     'piecesWhoseFootprintHoldsTheEye': inside,
                     'skylinePixels': skyn, 'occludedPixels': hidn,
                     'occludedShare': round(hidn / max(1, skyn), 4), 'boxesSeen': used,
                     'placements': len(pbox), 'culledPlacements': culled,
                     'culledShare': round(culled / max(1, len(pbox)), 4)})
        if '--png' in opt:
            img = np.zeros((ELN, AZN, 3), np.uint8)
            img[..., :] = (40, 40, 44)
            img[skym] = (150, 150, 158)
            img[occm & ~skym] = (120, 90, 40)
            img[occm & skym] = (255, 170, 40)
            Image.fromarray(img).save('%s_eye%d.png' % (opt['--png'], e_i))
        print(json.dumps(rows[-1]))
    out = {'file': base, 'region': region, 'eyes': rows,
           'meanOccludedShare': round(float(np.mean([r['occludedShare'] for r in rows])), 4),
           'meanCulledShare': round(float(np.mean([r['culledShare'] for r in rows])), 4)}
    if '--json' in opt:
        open(opt['--json'], 'w').write(json.dumps(out, indent=1))
    print(json.dumps({k: out[k] for k in ('meanOccludedShare', 'meanCulledShare')}))


if __name__ == '__main__':
    main()
