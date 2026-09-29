# MERGE1 copy of MAPS1 offline_objects.py: input paths come from env MAPS_CW (and MAPS_V2); nothing else changed.
"""offline_objects.py -- MAPS1 offline top-down decodes of the object channels no renderer view isolates.

Same population as the viewer (tests/spells/lodl_channels_table.drawn: region -5 -10 2 -3, slot 0, level 0 =
11,416 placements), same world transform as src/lodinative.cpp:1043-1044 (pos + M * (local * scale), M row-major,
normal = M * n), oct12 normals per src/lodofile.cpp:143. Painted top-down, north up, 1600x1600 over cells
x -5..2, y -10..-3 (20.48 world units a pixel), triangles painter-sorted by their highest vertex so the roof wins.

Writes pics/O_*.png and offline_objects.json (numbers for the refuters)."""
import sys, os, json, math, struct
import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, 'E:/Projects/NifskopeWWE-night/tests/spells')
import lodgen_native_decode as ND
import lodl_channels_table as CT

HERE = os.path.dirname(os.path.abspath(__file__))
OBJ = os.environ['MAPS_CW'] + '/Commonwealth'
X0, Y0, X1, Y1 = -5, -10, 2, -3
WX0, WY1 = X0 * 4096.0, (Y1 + 1) * 4096.0
N = 1600
UPP = (X1 - X0 + 1) * 4096.0 / N
BG = (40, 40, 44)


def px(x, y):
    return ((x - WX0) / UPP, (WY1 - y) / UPP)


def oct12(n0, n1, n2):
    p = n0 | (n1 << 8) | (n2 << 16)
    u = (p & 0xFFF) / 4095.0 * 2 - 1
    v = ((p >> 12) & 0xFFF) / 4095.0 * 2 - 1
    z = 1 - abs(u) - abs(v)
    if z < 0:
        u, v = (1 - abs(v)) * (1 if u >= 0 else -1), (1 - abs(u)) * (1 if v >= 0 else -1)
    l = math.sqrt(u * u + v * v + z * z)
    return (u / l, v / l, z / l)


def deq(q, lo, ext):
    return lo + q / 65535.0 * ext


def main():
    L = ND.read_lodo(OBJ + '.lodo')
    T = ND.read_lodi(OBJ + '.lodi')
    sel = CT.drawn(L, T, X0, Y0, X1, Y1, 0, 0)
    print('placements drawn', len(sel))
    tris = []   # (zmax, pts, dict of per-triangle values)
    nverts = 0
    cosines = []
    roof = wall = 0
    colour_meshes = 0
    for ii, inst, mesh in sel:
        m = inst['m']; s = inst['scaleF']
        pos = (inst['x'], inst['y'], inst['z'])
        me = L['meshes'][mesh]
        has_col = bool(me['flags'] & 8)
        has_alpha = bool(me['flags'] & 16)
        colour_meshes += has_col
        first, span = CT.mesh_range(L, mesh)
        vf = T['vertexAoFirst']; sf = T['vertexSkyFirst']
        vao_ok = vf and ii + 1 < len(vf) and vf[ii + 1] - vf[ii] == span and span > 0
        sky_ok = sf and ii + 1 < len(sf) and sf[ii + 1] - sf[ii] == span and span > 0
        pa = T['placementAo'][ii] if ii < len(T['placementAo']) else 255
        for c in range(me['clusterFirst'], me['clusterFirst'] + me['clusterCount']):
            cll = L['clusterLods'][c]
            if not (cll['level'] == 0 or (cll['level'] < 0 and cll['parentCount'] == 0)):
                continue
            cl = L['clusters'][c]
            W = {}
            for v in range(cl['vertexCount']):
                vi = cl['vertexBase'] + v
                lv = L['vertices'][vi]
                lp = [deq(lv['px'], me['aabbMin'][0], me['aabbExtent'][0]) * s,
                      deq(lv['py'], me['aabbMin'][1], me['aabbExtent'][1]) * s,
                      deq(lv['pz'], me['aabbMin'][2], me['aabbExtent'][2]) * s]
                wp = [pos[r] + m[r * 3] * lp[0] + m[r * 3 + 1] * lp[1] + m[r * 3 + 2] * lp[2] for r in range(3)]
                n = oct12(lv['n0'], lv['n1'], lv['n2'])
                wn = [m[r * 3] * n[0] + m[r * 3 + 1] * n[1] + m[r * 3 + 2] * n[2] for r in range(3)]
                col = L['colours'][vi] if has_col else None
                W[v] = (wp, wn, col,
                        T['vertexAo'][vf[ii] + vi - first] if vao_ok else None,
                        T['vertexSky'][sf[ii] + vi - first] if sky_ok else None)
                nverts += 1
            li = c * 48
            for t in range(cl['triangleCount']):
                a, b, cc = L['localIndices'][li + 3 * t:li + 3 * t + 3]
                if a not in W or b not in W or cc not in W:
                    continue
                A, B, C = W[a], W[b], W[cc]
                # the geometric face normal from the dequantised positions -- independent of the stored normals
                e1 = [B[0][k] - A[0][k] for k in range(3)]
                e2 = [C[0][k] - A[0][k] for k in range(3)]
                fn = [e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]]
                fl = math.sqrt(sum(q * q for q in fn))
                vn = [(A[1][k] + B[1][k] + C[1][k]) / 3.0 for k in range(3)]
                vl = math.sqrt(sum(q * q for q in vn)) or 1.0
                vn = [q / vl for q in vn]
                if fl > 1e-3:
                    fn = [q / fl for q in fn]
                    cosines.append(sum(fn[k] * vn[k] for k in range(3)))
                if vn[2] > 0.95:
                    roof += 1
                elif abs(vn[2]) < 0.2:
                    wall += 1
                vals = {'n': vn, 'flags': inst['flags'], 'pao': pa, 'psky': inst['sky']}
                if A[2] is not None:
                    vals['rgb'] = tuple(int(sum(X[2][k] for X in (A, B, C)) / 3) for k in range(3))
                    vals['a'] = int(sum(X[2][3] for X in (A, B, C)) / 3) if has_alpha else None
                if A[3] is not None:
                    vals['vao'] = (A[3] + B[3] + C[3]) / 3.0
                if A[4] is not None:
                    vals['vsky'] = (A[4] + B[4] + C[4]) / 3.0
                pts = [px(X[0][0], X[0][1]) for X in (A, B, C)]
                tris.append((max(A[0][2], B[0][2], C[0][2]), pts, vals))
    tris.sort(key=lambda t: t[0])
    print('vertices', nverts, 'triangles', len(tris))

    panels = {k: Image.new('RGB', (N, N), BG) for k in
              ('O_normal_world', 'O_vcolour_rgb', 'O_vcolour_a', 'O_vao_stream', 'O_placement_ao',
               'O_sky_placement', 'O_sky_stream', 'O_flags')}
    dr = {k: ImageDraw.Draw(v) for k, v in panels.items()}
    FLAGCOL = {0: (150, 150, 150), 1: (80, 200, 255), 2: (255, 80, 200), 4: (120, 230, 90), 8: (255, 230, 60),
               16: (255, 140, 40), 32: (200, 60, 60)}
    flagcount = {}
    for z, pts, v in tris:
        n = v['n']
        dr['O_normal_world'].polygon(pts, fill=tuple(int(round((q * 0.5 + 0.5) * 255)) for q in n))
        if 'rgb' in v:
            dr['O_vcolour_rgb'].polygon(pts, fill=v['rgb'])
            if v.get('a') is not None:
                dr['O_vcolour_a'].polygon(pts, fill=(v['a'],) * 3)
            else:
                dr['O_vcolour_a'].polygon(pts, fill=(70, 70, 110))
        else:
            dr['O_vcolour_rgb'].polygon(pts, fill=(70, 70, 110))
            dr['O_vcolour_a'].polygon(pts, fill=(70, 70, 110))
        if 'vao' in v:
            g = int(round(v['vao'])); dr['O_vao_stream'].polygon(pts, fill=(g, g, g))
        g = v['pao']; dr['O_placement_ao'].polygon(pts, fill=(g, g, g) if g != 255 else (255, 255, 255))
        g = v['psky']; dr['O_sky_placement'].polygon(pts, fill=(g, g, g))
        if 'vsky' in v:
            g = int(round(v['vsky'])); dr['O_sky_stream'].polygon(pts, fill=(g, g, g))
        fb = v['flags'] & 0x3F
        # the class a triangle shows: the highest set bit of 0..5, so a rare bit is never hidden under a common one
        top = 0
        for bit in (32, 16, 8, 4, 2, 1):
            if fb & bit:
                top = bit; break
        flagcount[top] = flagcount.get(top, 0) + 1
        dr['O_flags'].polygon(pts, fill=FLAGCOL[top])
    # occluder boxes over a faint silhouette
    occ = Image.new('RGB', (N, N), BG)
    do = ImageDraw.Draw(occ)
    for z, pts, v in tris:
        do.polygon(pts, fill=(78, 78, 84))
    nocc = 0
    for o in T['occluders']:
        if not (X0 * 4096 <= o['x'] < (X1 + 1) * 4096 and Y0 * 4096 <= o['y'] < (Y1 + 1) * 4096):
            continue
        mm = ND.unpack_rotation(o['r0'], o['r1'], o['r2'])
        cs = []
        for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            lx, ly = sx * o['hx'], sy * o['hy']
            cs.append(px(o['x'] + mm[0] * lx + mm[1] * ly, o['y'] + mm[3] * lx + mm[4] * ly))
        do.polygon(cs, outline=(255, 170, 40), fill=None)
        do.line(cs + [cs[0]], fill=(255, 170, 40), width=2)
        nocc += 1
    panels['O_occluders'] = occ
    for k, im in panels.items():
        im.save(os.path.join(HERE, 'pics', k + '.png'))
    arr = {k: np.asarray(v).astype(np.int32) for k, v in panels.items()}
    cos = np.array(cosines)
    out = {'placements': len(sel), 'vertices': nverts, 'triangles': len(tris), 'colourMeshPlacements': colour_meshes,
           'occludersInRegion': nocc, 'occludersTotal': len(T['occluders']),
           'normal_face_cos_mean': float(cos.mean()), 'normal_face_cos_share_gt_0.9': float((cos > 0.9).mean()),
           'normal_face_cos_share_lt_0': float((cos < 0).mean()),
           'roofTriangles_nz_gt_0.95': roof, 'wallTriangles_abs_nz_lt_0.2': wall,
           'flagClassTriangles': {str(k): v for k, v in sorted(flagcount.items())}}
    # colour of roof and wall pixels in the world-normal panel (the known-facing refuter)
    nw = arr['O_normal_world']
    bg = np.all(nw == np.array(BG), axis=2)
    out['normal_panel_pixels'] = int((~bg).sum())
    blue = (~bg) & (nw[:, :, 2] > 240) & (np.abs(nw[:, :, 0] - 128) < 20) & (np.abs(nw[:, :, 1] - 128) < 20)
    out['normal_panel_share_flat_roof_128_128_255'] = float(blue.sum() / max(1, (~bg).sum()))
    # ---- the sky refuter: an up-facing surface with something ABOVE it (under a catwalk / overpass / canopy)
    # must read a lower sky stream than an up-facing surface nothing covers (an open roof).
    zimg = Image.new('F', (N, N), -1e9)
    dz = ImageDraw.Draw(zimg)
    for z, pts, v in tris:
        dz.polygon(pts, fill=float(z))
    zt = np.asarray(zimg)
    open_s, under_s = [], []
    for z, pts, v in tris:
        if v['n'][2] < 0.9 or 'vsky' not in v:
            continue
        cx = int(sum(p[0] for p in pts) / 3); cy = int(sum(p[1] for p in pts) / 3)
        if not (0 <= cx < N and 0 <= cy < N):
            continue
        top = zt[cy, cx]
        if top > z + 300:
            under_s.append(v['vsky'])
        elif top <= z + 1:
            open_s.append(v['vsky'])
    out['sky_upfacing_open'] = {'triangles': len(open_s), 'mean': float(np.mean(open_s)) if open_s else None}
    out['sky_upfacing_covered_300plus'] = {'triangles': len(under_s), 'mean': float(np.mean(under_s)) if under_s else None}
    # the same split on the per-PLACEMENT byte, which cannot see a catwalk over one part of a building
    json.dump(out, open(os.path.join(HERE, 'offline_objects.json'), 'w'), indent=1)
    print(json.dumps(out, indent=1))


if __name__ == '__main__':
    main()
