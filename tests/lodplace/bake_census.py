"""LODPLACE1 bake census: every far-LOD placement in a baked .lodi against (i) the terrain height at its XY
(LAND VHGT, last plugin of the bake's .lodb wins) and (ii) its ESM reference (DATA pos/rot, XSCL).

python -I tests/lodplace/bake_census.py <dir with ws.lodo/.lodi/.lodb> <ws> [--region=x0,y0,x1,y1]
Independent of the C++ reader: the .lodi is decoded by tests/spells/lodgen_native_decode.py, the plugins by
tools/lod_emission_probe.py's walker.
"""
import argparse, math, os, struct, sys, time, collections

TREE = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..'))
sys.path.insert(0, os.path.join(TREE, 'tests', 'spells'))
sys.path.insert(0, os.path.join(TREE, 'tools'))
import lodgen_native_decode as D
from lod_emission_probe import walk, subrecords, recordData

COMMONWEALTH = 0x3C
SCOLS = {}      # SCOL low24 -> [(part base low24, (px, py, pz, rx, ry, rz, s))] in ONAM/DATA order


def mat_vec(m, v):
    return [m[0] * v[0] + m[1] * v[1] + m[2] * v[2], m[3] * v[0] + m[4] * v[1] + m[5] * v[2],
            m[6] * v[0] + m[7] * v[1] + m[8] * v[2]]


def scol_part(e, part):
    """lodgen.cpp's composition of collection part `part` of reference e: pos rp + rm (p s), rot rm pm, scale s ps."""
    parts = SCOLS.get(e[1])
    if parts is None or not 0 <= part < len(parts):
        return None
    pb, (px, py, pz, rx, ry, rz, ps) = parts[part]
    rm = D.euler_neg_matrix(e[2][3], e[2][4], e[2][5])
    off = mat_vec(rm, [px * e[3], py * e[3], pz * e[3]])
    return pb, (e[2][0] + off[0], e[2][1] + off[1], e[2][2] + off[2]), D.mat_mul(rm, D.euler_neg_matrix(rx, ry, rz)), e[3] * ps


def plugins_of(lodb):
    out = []
    for line in open(lodb, encoding='utf-8'):
        t = line.rstrip('\r\n').split('\t')
        if t[0] == 'plugin' and len(t) >= 6:
            out.append(t[5])
    return out


def masters_of(buf):
    # TES4 header record: MAST subrecords
    size = struct.unpack_from('<I', buf, 4)[0]
    flags = struct.unpack_from('<I', buf, 8)[0]
    ms = []
    for st, p in subrecords(recordData(buf, 24, size, flags)):
        if st == b'MAST':
            ms.append(p.split(b'\0', 1)[0].decode('latin1').lower())
    return ms, flags


def read_plugins(paths, ws):
    refs = collections.defaultdict(list)    # low24 -> [(plugin, base24, pos6, scale)]
    land = {}                               # (cx, cy) -> 33x33 heights
    t0 = time.time()
    for p in paths:
        if not os.path.exists(p):
            print('  plugin missing: %s' % p)
            continue
        buf = open(p, 'rb').read()
        name = os.path.basename(p).lower()
        cellWorld, cellXY = {}, {}
        nref = nland = 0
        for typ, form, flags, doff, dsize, stack in walk(buf):
            if typ == b'CELL':
                w = None
                for label, gtype in reversed(stack):
                    if gtype == 1:
                        w = struct.unpack_from('<I', label, 0)[0]
                        break
                cellWorld[form] = w
                if (w or 0) & 0xFFFFFF == ws:
                    for st, payload in subrecords(recordData(buf, doff, dsize, flags)):
                        if st == b'XCLC' and len(payload) >= 8:
                            cellXY[form] = struct.unpack_from('<ii', payload, 0)
                continue
            if typ == b'SCOL' and not flags & 0x20:
                parts, cur = [], None
                for st, payload in subrecords(recordData(buf, doff, dsize, flags)):
                    if st == b'ONAM' and len(payload) >= 4:
                        cur = struct.unpack_from('<I', payload, 0)[0] & 0xFFFFFF
                    elif st == b'DATA' and cur is not None:
                        for k in range(len(payload) // 28):
                            parts.append((cur, struct.unpack_from('<7f', payload, k * 28)))
                        cur = None
                SCOLS[form & 0xFFFFFF] = parts
                continue
            if typ not in (b'REFR', b'LAND'):
                continue
            c = None
            for label, gtype in reversed(stack):
                if gtype in (6, 8, 9, 10):
                    c = struct.unpack_from('<I', label, 0)[0]
                    if gtype == 6:
                        break
            if c is None or ((cellWorld.get(c) or 0) & 0xFFFFFF) != ws:
                continue
            if flags & 0x20:
                continue
            data = recordData(buf, doff, dsize, flags)
            if typ == b'LAND':
                xy = cellXY.get(c)
                if xy is None:
                    continue
                for st, d in subrecords(data):
                    if st == b'VHGT' and len(d) >= 4 + 33 * 33:
                        rs = struct.unpack_from('<f', d, 0)[0]
                        g = struct.unpack_from('<%db' % (33 * 33), d, 4)
                        hs = [0.0] * (33 * 33)
                        for r in range(33):
                            rs += g[r * 33]
                            v = rs
                            hs[r * 33] = v * 8.0
                            for cc in range(1, 33):
                                v += g[r * 33 + cc]
                                hs[r * 33 + cc] = v * 8.0
                        land[xy] = hs
                        nland += 1
                continue
            base = pos = None
            scale = 1.0
            for st, payload in subrecords(data):
                if st == b'NAME' and len(payload) >= 4:
                    base = struct.unpack_from('<I', payload, 0)[0]
                elif st == b'DATA' and len(payload) >= 24:
                    pos = struct.unpack_from('<6f', payload, 0)
                elif st == b'XSCL' and len(payload) >= 4:
                    scale = struct.unpack_from('<f', payload, 0)[0]
            if base is not None and pos is not None:
                refs[form & 0xFFFFFF].append((name, base & 0xFFFFFF, pos, scale, bool(flags & 0x800)))
                nref += 1
        print('  %-40s refs %7d land %5d  (%.0f s)' % (name, nref, nland, time.time() - t0))
    return refs, land


def height_at(land, x, y):
    cx, cy = math.floor(x / 4096.0), math.floor(y / 4096.0)
    hs = land.get((cx, cy))
    if hs is None:
        return None
    fx = (x - cx * 4096.0) / 128.0
    fy = (y - cy * 4096.0) / 128.0
    c0, r0 = min(31, int(fx)), min(31, int(fy))
    tx, ty = fx - c0, fy - r0
    h00, h01 = hs[r0 * 33 + c0], hs[r0 * 33 + c0 + 1]
    h10, h11 = hs[(r0 + 1) * 33 + c0], hs[(r0 + 1) * 33 + c0 + 1]
    return (h00 * (1 - tx) + h01 * tx) * (1 - ty) + (h10 * (1 - tx) + h11 * tx) * ty


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('bakedir')
    ap.add_argument('ws')
    ap.add_argument('--region', default='')
    ap.add_argument('--out', default='')
    ap.add_argument('--tol', type=float, default=50.0)
    # the census's own reds: every placement 6000 u lower (a z offset applied twice) or upside down
    ap.add_argument('--red', default='', choices=('', 'under', 'flip'))
    a = ap.parse_args()
    B = os.path.join(a.bakedir, a.ws + '.')
    L = D.read_lodo(B + 'lodo')
    T = D.read_lodi(B + 'lodi')
    print('lodi v%d %d instances; lodo v%d %d bases' % (T['header']['version'], len(T['instances']),
                                                       L['header']['version'], len(L['bases'])))
    reg = [int(v) for v in a.region.split(',')] if a.region else None
    refs, land = read_plugins(plugins_of(B + 'lodb'), COMMONWEALTH)
    print('ESM: %d ref ids, %d LAND cells' % (len(refs), len(land)))
    hi = collections.Counter(c['refFormId'] >> 24 for c in T['cold'])
    print('cold refFormId high bytes: %s' % dict(sorted(hi.items())))
    tally = collections.Counter()
    rows = []
    worst = collections.defaultdict(float)
    buried = collections.defaultdict(lambda: [0, None, []])
    meshlow = {}
    for i, (r, c) in enumerate(zip(T['instances'], T['cold'])):
        if a.red == 'under':
            r['z'] -= 6000.0
        elif a.red == 'flip':
            mm = r['m']     # m x Rx(180): a half turn about the model's own x axis, up z negated
            r['m'] = [mm[0], -mm[1], -mm[2], mm[3], -mm[4], -mm[5], mm[6], -mm[7], -mm[8]]
        x, y, z = r['x'], r['y'], r['z']
        cx, cy = math.floor(x / 4096.0), math.floor(y / 4096.0)
        if reg and not (reg[0] <= cx <= reg[2] and reg[1] <= cy <= reg[3]):
            continue
        tally['placements'] += 1
        m = r['m']
        s = r['scaleF']
        base = L['bases'][r['baseId']]
        mesh = None
        for k in ('rep0', 'rep1', 'rep2', 'rep3'):
            if base[k] != 0xFFFF and base[k] < len(L['meshes']):
                mesh = L['meshes'][base[k]]
                break
        # the rotated model box's world z range (a superset of the true range: corners)
        zlo = zhi = None
        if mesh:
            mn, ex = mesh['aabbMin'], mesh['aabbExtent']
            for kx in (0, 1):
                for ky in (0, 1):
                    for kz in (0, 1):
                        p = (mn[0] + kx * ex[0], mn[1] + ky * ex[1], mn[2] + kz * ex[2])
                        wz = z + s * (m[6] * p[0] + m[7] * p[1] + m[8] * p[2])
                        zlo = wz if zlo is None else min(zlo, wz)
                        zhi = wz if zhi is None else max(zhi, wz)
        h = height_at(land, x, y)
        part0 = c['scolPart'] != -1
        if h is None:
            tally['no LAND under'] += 1
        else:
            if z < h - a.tol:
                tally['origin under terrain-%g' % a.tol] += 1
            if zhi is not None and zhi < h - a.tol:
                tally['WHOLE box under terrain-%g' % a.tol] += 1
                tally['WHOLE box under, %s' % ('scol part' if part0 else 'plain ref')] += 1
                if z >= h - a.tol:
                    tally['WHOLE box under while ORIGIN on/above ground'] += 1
                key = L['string_at'](mesh['modelStringOffset']) if mesh else '?'
                buried[key][0] += 1
                buried[key][1] = (mesh['aabbMin'], mesh['aabbExtent']) if mesh else None
                buried[key][2].append('%s %08x part %d z %.0f ground %.0f top %.0f up.z %.2f at %.0f,%.0f'
                                      % ('P' if part0 else 'R', c['refFormId'], c['scolPart'], z, h, zhi, m[8], x, y))
        if mesh:
            meshlow[L['string_at'](mesh['modelStringOffset'])] = (mesh['aabbMin'], mesh['aabbExtent'])
        flipped = m[8] < -0.1
        if flipped:
            tally['up axis z<0 (lodi)'] += 1
        # the ESM reference
        cand = refs.get(c['refFormId'] & 0xFFFFFF, [])
        # the record of this id that places it here: an override can move a REFR and another plugin's NEW form can
        # share the low 24 bits, so "moved by the writer" means NO record of the id puts it there. A plain ref by its
        # own position; a collection part by its COMPOSED position (the ref's own sits elsewhere)
        part = c['scolPart'] != -1
        best = None
        for e in cand:
            if part:
                sp = scol_part(e, c['scolPart'])
                if sp is None:
                    continue
                d = max(abs(sp[1][0] - x), abs(sp[1][1] - y)) + abs(sp[1][2] - z) * 0.001
            else:
                d = max(abs(e[2][0] - x), abs(e[2][1] - y))
            if best is None or d < best[0]:
                best = (d, e)
        if best is None and cand:
            best = (1e9, cand[-1])
        if part:
            tally['scol parts (ESM compare skipped)'] += 1
        if best is None:
            tally['no ESM ref by id'] += 1
            continue
        d, e = best
        if part:
            sp = scol_part(e, c['scolPart'])
            if sp is None:
                tally['scol part: no SCOL record / index past it'] += 1
                continue
            pb, pp, pm, ps = sp
            tally['scol part composed'] += 1
            if pb != L['bases'][r['baseId']]['formId'] & 0xFFFFFF:
                tally['scol part: base differs'] += 1
            dd = max(abs(pp[0] - x), abs(pp[1] - y))
            dz = z - pp[2]
            zstep = T['chunks'][r['chunk']]['zExtent'] / 65535.0
            if dd > 1.0:
                tally['scol part: XY off > 1u'] += 1
                if len(rows) < 120:
                    rows.append('SXY ref %08x part %d off %.1f u, dz %.1f' % (c['refFormId'], c['scolPart'], dd, dz))
            if dd > 64.0 or abs(dz) > 1024.0:
                tally['scol part: MOVED by the writer (XY > 64 u or Z > 1024 u)'] += 1
            if abs(dz) > max(1.0, zstep):
                tally['scol part: Z off > max(1u, z step)'] += 1
            ang = D.mat_angle_deg(pm, m)
            if base['flags'] & 1:
                th = D.tree_hash(pp[0], pp[1])
                ang = min(ang, D.mat_angle_deg(D.mat_mul(pm, D.euler_neg_matrix(0.0, 0.0, -((th % 360) * 0.01745329))), m))
            if ang > 0.5:
                tally['scol part: rotation off > 0.5 deg'] += 1
                if len(rows) < 100:
                    rows.append('SR ref %08x part %d angle %.2f lodi up.z %.3f composed up.z %.3f'
                                % (c['refFormId'], c['scolPart'], ang, m[8], pm[8]))
            if m[8] < -0.1 and not pm[8] < -0.1:
                tally['scol part: FLIPPED vs composed'] += 1
            if pm[8] < 0.5:
                tally['scol part: composed up.z < 0.5 (lying/tilted in the ESM itself)'] += 1
            continue
        tally['ESM matched'] += 1
        if d > 1.0:
            tally['ESM XY off > 1u'] += 1
        dz = z - e[2][2]
        worst['dz'] = max(worst['dz'], abs(dz))
        zstep = T['chunks'][r['chunk']]['zExtent'] / 65535.0
        if abs(dz) > 1024.0:
            tally['ESM Z off > 1024 u (MOVED by the writer)'] += 1
        if abs(dz) > max(1.0, zstep):
            tally['ESM Z off > max(1u, z step)'] += 1
            if len(rows) < 40:
                rows.append('Z  ref %08x base %06x lodi z %.1f esm z %.1f terrain %s at %.0f,%.0f'
                            % (c['refFormId'], e[1], z, e[2][2], '%.0f' % h if h is not None else '-', x, y))
        me = D.euler_neg_matrix(e[2][3], e[2][4], e[2][5])
        if base['flags'] & 1:
            th = D.tree_hash(e[2][0], e[2][1])
            me = D.mat_mul(me, D.euler_neg_matrix(0.0, 0.0, -((th % 360) * 0.01745329)))
        ang = D.mat_angle_deg(me, m)
        worst['rot'] = max(worst['rot'], ang)
        if ang > 0.5:
            tally['ESM rotation off > 0.5 deg'] += 1
            if len(rows) < 80:
                rows.append('R  ref %08x base %06x angle %.2f lodi up.z %.3f esm up.z %.3f'
                            % (c['refFormId'], e[1], ang, m[8], me[8]))
        if flipped and not me[8] < -0.1:
            tally['FLIPPED vs ESM (lodi up z<0, ESM not)'] += 1
        if abs(s - e[3]) > 1.0 / 4096 + 1e-4 * e[3]:
            tally['ESM scale off'] += 1
        if e[4]:
            tally['ESM initially disabled'] += 1
        if h is not None and e[2][2] < h - a.tol:
            tally['ESM origin itself under terrain-%g' % a.tol] += 1
    print('region %s' % (a.region or 'all'))
    for k in sorted(tally):
        print('  %-48s %8d' % (k, tally[k]))
    print('  worst |dz| %.2f  worst rotation %.3f deg' % (worst['dz'], worst['rot']))
    for line in rows:
        print('  ' + line)
    # the card manifests: every row (cards included) against the .lodi and the ESM, Z too
    import glob
    byref = {}
    for r, c in zip(T['instances'], T['cold']):
        byref[(c['refFormId'], c['scolPart'])] = r
    mt = collections.Counter()
    for f in sorted(glob.glob(B + '*.BTO.manifest.txt')):
        lines = open(f, encoding='utf-8').read().splitlines()
        rowsOf = {}
        for line in lines:
            t = line.split()
            if not t or not t[0].isdigit() or len(t) < 11:
                continue
            rowsOf[t[0]] = t
        for line in lines:
            t = line.split()
            if not t or t[0] != 'C' or t[1] not in rowsOf:
                continue
            o = rowsOf[t[1]]
            x, y, z = float(o[3]), float(o[4]), float(o[5])
            cx, cy = math.floor(x / 4096.0), math.floor(y / 4096.0)
            if reg and not (reg[0] <= cx <= reg[2] and reg[1] <= cy <= reg[3]):
                continue
            mt['card rows'] += 1
            ref, part = int(o[9], 16), int(o[10])
            h = height_at(land, x, y)
            if h is not None and z < h - a.tol:
                mt['card origin under terrain-%g' % a.tol] += 1
            # card box: centre (C col 2..4) x scale, half extents
            cz, hz = float(t[4]), float(t[6]) if len(t) > 6 else 0.0
            s = float(o[6])
            if h is not None and z + s * (cz + hz) < h - a.tol:
                mt['card WHOLE under terrain-%g' % a.tol] += 1
            li = byref.get((ref, part))
            if li is None:
                mt['card not in the .lodi'] += 1
            elif abs(li['z'] - z) > 2.0:
                mt['card Z differs from the .lodi > 2u'] += 1
            cand = refs.get(ref & 0xFFFFFF, [])
            if part == -1 and cand:
                e = min(cand, key=lambda e: max(abs(e[2][0] - x), abs(e[2][1] - y)))
                if abs(e[2][2] - z) > 2.0:
                    mt['card Z differs from the ESM > 2u'] += 1
    print('card manifests (C lines):')
    for k in sorted(mt):
        print('  %-48s %8d' % (k, mt[k]))
    print('buried (whole box under terrain-%g) by LOD mesh, top 25:' % a.tol)
    for k, v in sorted(buried.items(), key=lambda kv: -kv[1][0])[:25]:
        bx = v[1]
        print('  %5d  %s  aabb min %s ext %s' % (v[0], k, tuple(round(t) for t in bx[0]) if bx else '-',
                                                tuple(round(t) for t in bx[1]) if bx else '-'))
        for s in v[2][:3]:
            print('         ' + s)
    hang = [(k, v) for k, v in meshlow.items() if v[0][2] + v[1][2] < 0.0]
    print('LOD meshes used here whose whole model box is BELOW their own origin (top z < 0): %d' % len(hang))
    for k, v in sorted(hang, key=lambda kv: kv[1][0][2] + kv[1][1][2])[:20]:
        print('  top %.0f  %s' % (v[0][2] + v[1][2], k))
    # the gate: nothing the WRITER moved away from its reference (the game's own buried objects are not counted)
    bad = (tally['ESM Z off > 1024 u (MOVED by the writer)'] + tally['ESM XY off > 1u']
           + tally['scol part: MOVED by the writer (XY > 64 u or Z > 1024 u)']
           + tally['FLIPPED vs ESM (lodi up z<0, ESM not)'] + tally['scol part: FLIPPED vs composed'])
    ok = bad == 0 and tally['ESM matched'] > 0
    print('bake census: moved/flipped by the writer %d (Z > 1024 u %d, XY %d, parts moved %d, flipped %d+%d), matched %d VERDICT %s'
          % (bad, tally['ESM Z off > 1024 u (MOVED by the writer)'], tally['ESM XY off > 1u'],
             tally['scol part: MOVED by the writer (XY > 64 u or Z > 1024 u)'],
             tally['FLIPPED vs ESM (lodi up z<0, ESM not)'], tally['scol part: FLIPPED vs composed'], tally['ESM matched'],
             'PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
