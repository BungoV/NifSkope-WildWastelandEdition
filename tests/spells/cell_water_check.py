"""lane WATER1: the independent checker for tests/spells/cell_water.sh (stages A B C D).

A  reader   every WATR in the master, parsed here from the record layout (xEdit's DNAM: fog 52 bytes, physical 48,
            specular 28, noise 60, silt 12, ssr 1; a 188-byte DNAM stops before the silt), turned into the 13 shader
            constants by the rule in docs/PRTP_PLAN.md (colours byte/255 ^ 2.2, the w lanes, the clamps), and compared
            with the renderer's own description of the same forms (WW_CELL_WATER_DUMP_FORMS).
              judge reds: WATER_JUDGE_RED=nogamma (colours linear) / noclamp (P1.x, P1.w as stored) / nosilt (a 188-byte
              DNAM read as if it carried silt) -- each must FAIL.
B  rebuild  the final water colour rebuilt here per pixel from the shader's own probe pictures (N, V, rz/rw/dfade, the
            scene texel behind, the offset texel, the frame constants) and the material line from the dump, then
            compared with the picture the renderer wrote. The reds (WW_CELL_WATER_RED=norefl/nofresnel/nosilt/nospec/
            noshore/nonormal) sabotage one term in the renderer; this checker keeps the full formula, so each FAILS.
C  off      with the master off (row off, or WW_CELL_WATER=0) the picture equals the before-lane exe's within the
            run-to-run noise floor measured between two before-lane runs.
D  bake     every dumped probe ray that crossed water, recomputed: the crossing height (XCLW), the Fresnel term, the
            reflected / transmitted weights and the underwater-fog filter, from the WATR record parsed here.

USAGE  python cell_water_check.py A <esm> <forms file>
       python cell_water_check.py B <dir> <tag>          (pictures <tag>.png, <tag>.p1.png ... <tag>.p10.png, <tag>.dump)
       python cell_water_check.py C <after.png> <rung.png> <rung2.png>
       python cell_water_check.py D <esm> <bakedump file>
The last line printed is "PASS ..." or "FAIL ...".
"""
import os
import struct
import sys
import zlib

import numpy as np

JUDGE_RED = os.environ.get('WATER_JUDGE_RED', '')


# ---------------------------------------------------------------- the master, read here
def esm_records(path, want):
    d = open(path, 'rb').read()
    out = {}

    def walk(a, b):
        i = a
        while i < b:
            t = d[i:i + 4]
            if t == b'GRUP':
                sz, lab, gt = struct.unpack_from('<I4si', d, i + 4)
                if gt == 0 and lab != want:
                    i += sz
                    continue
                walk(i + 24, i + sz)
                i += sz
                continue
            sz, fl, fid = struct.unpack_from('<III', d, i + 4)
            if t == want:
                body = d[i + 24:i + 24 + sz]
                if fl & 0x40000:
                    body = zlib.decompress(body[4:])
                out[fid] = body
            i += 24 + sz

    walk(24 + struct.unpack_from('<I', d, 4)[0], len(d))
    return out


def fields(body):
    i, big, out = 0, None, {}
    while i + 6 <= len(body):
        t = body[i:i + 4].decode('latin1')
        n = struct.unpack_from('<H', body, i + 4)[0]
        i += 6
        if t == 'XXXX':
            big = struct.unpack_from('<I', body, i)[0]
            i += n
            continue
        if big is not None:
            n, big = big, None
        out.setdefault(t, body[i:i + n])
        i += n
    return out


def watr(body):
    """The record as named fields (xEdit's names)."""
    f = fields(body)
    w = {'edid': f.get('EDID', b'').rstrip(b'\0').decode('latin1') or '-',
         'anam': f['ANAM'][0] if 'ANAM' in f else 0}
    for k, s in (('noise0', 'NAM2'), ('noise1', 'NAM3'), ('noise2', 'NAM4')):
        w[k] = f.get(s, b'').rstrip(b'\0').decode('latin1') or '-'
    dn = f.get('DNAM', b'')
    w['dnam'] = len(dn)
    pad = dn + bytes(max(0, 201 - len(dn)))
    o = 0

    def F():
        nonlocal o
        v = struct.unpack_from('<f', pad, o)[0]
        o += 4
        return v

    def C():
        nonlocal o
        v = tuple(pad[o:o + 4])
        o += 4
        return v

    # fog (52)
    w['depth'] = F(); w['shallow'] = C(); w['deep'] = C()
    w['csr'] = F(); w['cdr'] = F(); w['sa'] = F(); w['da'] = F(); w['asr'] = F(); w['adr'] = F()
    w['uw'] = C(); w['uwAmount'] = F(); w['uwNear'] = F(); w['uwFar'] = F()
    # physical (48)
    w['normalMag'] = F(); w['shallowNF'] = F(); w['deepNF'] = F(); w['reflectivity'] = F(); w['fresnel'] = F()
    w['surfFalloff'] = F(); w['disp'] = [F() for _ in range(5)]; w['refl'] = C()
    # specular (28)
    w['sunSpecPower'] = F(); w['sunSpecMag'] = F(); w['sparklePower'] = F(); w['sparkleMag'] = F()
    w['intRadius'] = F(); w['intBright'] = F(); w['intPower'] = F()
    # noise (60)
    w['wind'] = [F() for _ in range(3)]; w['speed'] = [F() for _ in range(3)]
    w['amp'] = [F() for _ in range(3)]; w['uv'] = [F() for _ in range(3)]; w['falloff'] = [F() for _ in range(3)]
    # silt (12): absent from a 188-byte DNAM (reads 0)
    if len(dn) >= 200 or JUDGE_RED == 'nosilt':
        src = dn if JUDGE_RED != 'nosilt' else pad
        w['silt'] = struct.unpack_from('<f', src + bytes(201), 188)[0]
        w['lightSilt'] = tuple((src + bytes(201))[192:196]); w['darkSilt'] = tuple((src + bytes(201))[196:200])
        if JUDGE_RED == 'nosilt' and len(dn) < 200:   # the red: read the next record's bytes as silt
            w['silt'] = 1.0; w['lightSilt'] = (255, 255, 255, 0); w['darkSilt'] = (255, 255, 255, 0)
    else:
        w['silt'] = 0.0; w['lightSilt'] = (0, 0, 0, 0); w['darkSilt'] = (0, 0, 0, 0)
    return w


def lin(b):
    return b / 255.0 if JUDGE_RED == 'nogamma' else (b / 255.0) ** 2.2


def material(w):
    """The 13 shader constants (52 floats), in the shader's order."""
    def col(c, a):
        return [lin(c[0]), lin(c[1]), lin(c[2]), a]
    noclamp = JUDGE_RED == 'noclamp'
    m = []
    m += col(w['shallow'], w['sparklePower'])
    m += col(w['deep'], w['sunSpecMag'])
    m += col(w['refl'], 0.0)
    m += col(w['uw'], 0.0)
    m += col(w['lightSilt'], w['silt'])
    m += col(w['darkSilt'], 0.0)
    m += [w['sunSpecPower'], w['reflectivity'], w['anam'] * 0.01, 250.0]
    m += [w['falloff'][0] if noclamp else min(max(w['falloff'][0], 1.0), 8191.0), 0.5, w['sparkleMag'],
          w['depth'] if noclamp else max(w['depth'], 1.0)]
    m += [w['surfFalloff'], w['normalMag'], w['shallowNF'], w['deepNF']]
    m += [w['asr'], w['adr'], w['sa'], w['da']]
    m += [w['csr'], w['cdr'], w['fresnel'], w['intPower']]
    m += [w['amp'][0], w['amp'][1], w['amp'][2], w['disp'][3]]
    m += [w['uv'][0], w['uv'][1], w['uv'][2], 0.0]
    return m


def close(a, b):
    return abs(a - b) <= 1e-5 * max(1.0, abs(a), abs(b))


def parse_watr_line(line):
    """The renderer's line: WATR <form> <edid> 52 floats uw a n f wind x y z speed x y z noise0 noise1 noise2."""
    t = line.split()
    if len(t) >= 3 and t[2] == 'NONE':
        return int(t[1], 16), None
    form, edid = int(t[1], 16), t[2]
    m = [float(x) for x in t[3:55]]
    assert t[55] == 'uw' and t[59] == 'wind' and t[63] == 'speed'
    uw = [float(x) for x in t[56:59]]
    wind = [float(x) for x in t[60:63]]
    speed = [float(x) for x in t[64:67]]
    return form, dict(edid=edid, m=m, uw=uw, wind=wind, speed=speed, noise=t[67:70])


def stage_a(esm, forms_file):
    recs = esm_records(esm, b'WATR')
    seen, bad, worst = 0, [], 0.0
    lines = [l for l in open(forms_file, encoding='latin1') if l.startswith('WATR ')]
    for l in lines:
        form, got = parse_watr_line(l)
        if form not in recs:
            if got is not None:
                bad.append('%08x described but no WATR in the master' % form)
            continue
        if got is None:
            bad.append('%08x is a WATR the renderer could not read' % form)
            continue
        w = watr(recs[form])
        want = material(w)
        seen += 1
        if got['edid'] != w['edid']:
            bad.append('%08x edid %s != %s' % (form, got['edid'], w['edid']))
        for i, (a, b) in enumerate(zip(got['m'], want)):
            worst = max(worst, abs(a - b))
            if not close(a, b):
                bad.append('%08x %s lane %d: renderer %g, here %g' % (form, w['edid'], i, a, b))
                break
        for name, a, b in (('uw', got['uw'], [w['uwAmount'], w['uwNear'], w['uwFar']]),
                           ('wind', got['wind'], w['wind']), ('speed', got['speed'], w['speed'])):
            if not all(close(x, y) for x, y in zip(a, b)):
                bad.append('%08x %s %s %s != %s' % (form, w['edid'], name, a, b))
        for k in range(3):
            if got['noise'][k].lower() != w['noise%d' % k].lower():
                bad.append('%08x noise%d %s != %s' % (form, k, got['noise'][k], w['noise%d' % k]))
    missing = sorted(set(recs) - {parse_watr_line(l)[0] for l in lines})
    for f in missing:
        bad.append('%08x in the master, not described' % f)
    short = sum(1 for b in recs.values() if watr(b)['dnam'] < 200)
    for b in bad[:12]:
        print('  ' + b)
    verdict = 'PASS' if seen and not bad else 'FAIL'
    print('%s A reader: %d WATR records of %d compared (%d with the 188-byte DNAM), %d mismatches, worst |d| %.3g%s'
          % (verdict, seen, len(recs), short, len(bad), worst, ('  judge red ' + JUDGE_RED) if JUDGE_RED else ''))
    return verdict == 'PASS'


# ---------------------------------------------------------------- pictures
def img(path):
    from PIL import Image
    return np.asarray(Image.open(path).convert('RGB'), np.float64)


def u16(hi, lo):
    return (np.round(hi) * 256.0 + np.round(lo)) / 65535.0


def tonemap(x, Aa, Da):
    a, b, c, d, e, f = 0.15, 0.50, 0.10, 0.20, 0.02, 0.30
    z = x * x * Da * (Aa * 4.22978723)
    z = (z * (a * z + b * c) + d * e) / (z * (a * z + b) + d * f) - e / f
    return np.sqrt(np.maximum(z / (Aa * 0.93333333), 0.0))


def untonemap(y, Aa, Da):
    a, b, c, d, e, f = 0.15, 0.50, 0.10, 0.20, 0.02, 0.30
    k = np.minimum(y * y * (Aa * 0.93333333) + e / f, 0.99999)
    qa = a * (k - 1.0)
    qb = b * (k - c)
    qc = d * (f * k - e)
    z = (-qb - np.sqrt(np.maximum(qb * qb - 4.0 * qa * qc, 0.0))) / (2.0 * qa)
    return np.maximum(z, 0.0) / (Da * Aa * 4.22978723)


def sat(x):
    return np.clip(x, 0.0, 1.0)


def stage_b(dirpath, tag):
    """Rebuild the final colour from the probe pictures; compare on the water pixels."""
    P = lambda k: img(os.path.join(dirpath, '%s.p%d.png' % (tag, k)))
    final = img(os.path.join(dirpath, tag + '.png')) / 255.0
    p = {k: P(k) for k in range(1, 11)}
    # the water mask: only the water's pixels change from one probe picture to the next (every other pixel is the
    # same scene each time, up to run-to-run noise); N against V, the depth terms' two bytes, the constants against N
    ne = lambda a, b: (np.abs(p[a] - p[b]).max(2) > 0)
    mask = ne(1, 3) & ne(5, 6) & ne(9, 1) & ne(10, 2)
    # the frame constants: per column x % 4
    H, W = mask.shape
    xs = np.arange(W)[None, :].repeat(H, 0)
    cv = u16(p[9], p[10])
    consts = {}
    for col in range(4):
        sel = mask & (xs % 4 == col)
        if sel.sum() == 0:
            print('FAIL B rebuild: no water pixels in column class %d' % col)
            return False
        consts[col] = np.median(cv[sel], axis=0)
    sun = consts[0] * 4.0
    Ldir = consts[1] * 2.0 - 1.0
    Aa, Da = consts[2][0] * 8.0, consts[2][1] * 8.0
    cellOn, cellLinear, fogOn = consts[3] > 0.5
    if fogOn:
        print('FAIL B rebuild: the shots must be taken with the fog off (WW_LOOKDEV_FOG=0)')
        return False
    # the material from the dump's body line (the first WATR line: the default and placed bodies)
    dump = [l for l in open(os.path.join(dirpath, tag + '.dump'), encoding='latin1') if l.startswith('WATR ')]
    sky = None
    for l in open(os.path.join(dirpath, tag + '.dump'), encoding='latin1'):
        if l.startswith('sky '):
            sky = np.array([float(x) for x in l.split()[2:11]]).reshape(3, 3)
    _, rec = parse_watr_line(dump[0])
    M = np.array(rec['m']).reshape(13, 4)
    Var, P1, P2, P3, P4 = M[6], M[7], M[8], M[9], M[10]
    N = u16(p[1], p[2]) * 2.0 - 1.0
    V = u16(p[3], p[4]) * 2.0 - 1.0
    rzw = u16(p[5], p[6])
    rz, rw, dfade = rzw[..., 0], rzw[..., 1], rzw[..., 2]
    texIn = p[7] / 255.0
    texOff = p[8] / 255.0
    under = consts[2][2] > 0.5

    def to_lin(t):
        if cellOn:
            return t if cellLinear else np.power(np.maximum(t, 0.0), 2.2)
        return untonemap(t, Aa, Da)

    refr, refrOff = to_lin(texIn), to_lin(texOff)
    x = sat((rw - P3[1]) / (P3[0] - P3[1]))
    alphaV = np.power(np.maximum(1.0 - (3.0 - 2.0 * x) * x * x, 0.0), 0.33) * (P3[3] - P3[2]) + P3[2]
    sx = sat((rz - P2[0]) / (1.0 - P2[0]))
    shore = sx * sx * (3.0 - 2.0 * sx)
    Ns = -N if under else N
    c5 = 1.0 - sat(np.sum(-V * Ns, 2))
    F = P4[2] + (1.0 - P4[2]) * c5 ** 5
    R = V - 2.0 * np.sum(V * Ns, 2)[..., None] * Ns
    Rz = R[..., 2:3]
    skyc = sky[0] + (sky[1] - sky[0]) * sat(Rz + 0.75)
    skyc = skyc + (sky[2] - skyc) * sat(Rz * 1.9 + 0.35)
    silted = refr + M[4][3] * (1.0 - alphaV)[..., None] * ((M[5][:3] + (M[4][:3] - M[5][:3]) * refr) - refr)
    sunc = sun
    spec = sunc * (sat(np.sum(R * Ldir, 2)) ** Var[0])[..., None] * M[1][3] \
        + sunc * (sat(np.sum(N * np.array([-0.099, -0.099, 0.99]), 2)) ** M[0][3])[..., None] * P1[2]
    if under:
        refl = 0.5 * (skyc + M[3][:3])
        linc = refl + (refr - refl) * (1.0 - F)[..., None]
    else:
        wR = (F * Var[1])[..., None]
        far = M[2][:3] + (skyc - M[2][:3]) * M[2][3]
        linc = silted + (skyc - silted) * wR
        linc = far + (linc - far) * dfade[..., None] + spec
        linc_ns = linc
        linc = linc + (refrOff - linc) * shore[..., None]
    if cellOn and not cellLinear:
        print('FAIL B rebuild: the cell imagespace path is not rebuilt here (exterior shots only)')
        return False

    def finish(l):
        l = np.maximum(l, 0.0)
        return np.clip(l if cellOn else tonemap(np.sqrt(l), Aa, Da), 0.0, 1.0)
    want = finish(linc)
    # the shore term acts only where the water is under ~1% of its fog depth deep, a few hundred pixels at most,
    # too few to move the whole-mask numbers: on the pixels where dropping it changes the rebuild by >= 1.5 levels,
    # the shot must sit closer to the full formula than to the formula without it
    if not under:
        want_ns = finish(linc_ns)
        sel = mask & (np.abs(want - want_ns).max(2) * 255.0 >= 1.5)
        ns = int(sel.sum())
        e_full = np.abs(want - final).max(2)[sel]
        e_ns = np.abs(want_ns - final).max(2)[sel]
        wins = int((e_full < e_ns).sum())
        ok_s = ns >= 30 and wins >= 0.9 * ns
        print('%s B shore %s: %d px where the shore term moves the rebuild >= 1.5 levels; the shot is nearer the full '
              'formula on %d (pass: >= 30 px, >= 90%%)' % ('PASS' if ok_s else 'FAIL', tag, ns, wins))
        if not ok_s:
            return False
    d = np.abs(want - final)[mask].max(1) * 255.0
    n = int(mask.sum())
    p99 = float(np.percentile(d, 99)) if n else 0.0
    mean = float(d.mean()) if n else 0.0
    # 8-bit probes quantise N and V to 1/65535 and the scene texels to 1/255: a rebuilt pixel lands within 2 levels
    ok = n >= 2000 and p99 <= 2.0 and mean <= 0.6
    print('%s B rebuild %s: %d water px, |rebuilt - shot| mean %.3f p99 %.2f max %.1f levels (pass: p99 <= 2, mean <= 0.6)'
          '  under=%d cellOn=%d linear=%d sun %.3f %.3f %.3f'
          % ('PASS' if ok else 'FAIL', tag, n, mean, p99, float(d.max()) if n else 0.0, under, cellOn, cellLinear,
             sun[0], sun[1], sun[2]))
    return ok


def stage_c(after, rung, rung2):
    a, r, r2 = img(after), img(rung), img(rung2)
    if a.shape != r.shape:
        print('FAIL C off: size %s != %s' % (a.shape, r.shape))
        return False
    noise = int((np.abs(r - r2).max(2) > 0).sum())
    diff = int((np.abs(a - r).max(2) > 0).sum())
    ok = diff <= max(noise * 2, 16)
    print('%s C off %s: %d px differ from the before-lane exe; the before-lane exe differs from itself by %d px'
          % ('PASS' if ok else 'FAIL', os.path.basename(after), diff, noise))
    return ok


# ---------------------------------------------------------------- the bake
def world_water(esm, wrld=0x3C):
    """Per cell (x, y) its water height (XCLW; absent = the worldspace's DNAM default; 3.4e38 = none), Commonwealth."""
    d = open(esm, 'rb').read()
    cells, deflt = {}, None

    def walk(a, b, inside):
        nonlocal deflt
        i = a
        while i < b:
            t = d[i:i + 4]
            if t == b'GRUP':
                sz, lab, gt = struct.unpack_from('<I4si', d, i + 4)
                lv = struct.unpack_from('<I', lab)[0]
                if gt == 0 and lab != b'WRLD':
                    i += sz
                    continue
                if gt == 1 and lv != wrld:
                    i += sz
                    continue
                walk(i + 24, i + sz, inside or (gt == 1 and lv == wrld))
                i += sz
                continue
            sz, fl, fid = struct.unpack_from('<III', d, i + 4)
            if t in (b'WRLD', b'CELL') and (t == b'CELL' and inside or t == b'WRLD' and fid == wrld):
                body = d[i + 24:i + 24 + sz]
                if fl & 0x40000:
                    body = zlib.decompress(body[4:])
                f = fields(body)
                if t == b'WRLD' and 'DNAM' in f:
                    deflt = struct.unpack_from('<f', f['DNAM'], 4)[0]   # default land, default water
                elif t == b'CELL' and 'XCLC' in f:
                    x, y = struct.unpack_from('<ii', f['XCLC'])
                    h = struct.unpack_from('<f', f['XCLW'])[0] if 'XCLW' in f else None
                    cells.setdefault((x, y), h)
            i += 24 + sz

    walk(24 + struct.unpack_from('<I', d, 4)[0], len(d), False)
    return cells, deflt


def stage_d(esm, dumpfile):
    """Each dumped ray of a probe near the water, recomputed from the master: where it meets the water (the cell's
    XCLW), the reflected weight (Fresnel x reflectivity from above, Fresnel from below) and the underwater-fog filter
    of the transmitted leg (and of the mirror leg from below)."""
    recs = esm_records(esm, b'WATR')
    cells, deflt = world_water(esm)
    kinds, raymax = {}, 131072.0
    rays = []
    for l in open(dumpfile, encoding='latin1'):
        t = l.split()
        if t and t[0] == 'kind':
            kinds[int(t[1])] = int(t[3], 16)
            raymax = float(t[5])
        elif t and t[0] == 'ray':
            rays.append(t)
    cache = {}

    def rec(kind):
        f = kinds[kind]
        if f not in cache:
            cache[f] = watr(recs[f])
        return cache[f]

    def fog(w, L):
        span = max(w['uwFar'] - w['uwNear'], 1e-3)
        a = min(max(w['uwAmount'] * min(max((L - w['uwNear']) / span, 0.0), 1.0), 0.0), 1.0)
        if JUDGE_RED == 'nofog':
            a = 0.0
        return np.array([1.0 - a * (1.0 - lin(c)) for c in w['uw'][:3]])

    n = bad = crossed = under_n = mirrored = 0
    worst = 0.0
    for t in rays:
        o = np.array([float(v) for v in t[3:6]])
        dv = np.array([float(v) for v in t[6:9]])
        thit = float(t[9])
        kv = {t[i]: t[i + 1] for i in range(10, 22, 2)}
        T0 = np.array([float(v) for v in t[23:26]])
        under, cross, kind = int(kv['under']), int(kv['cross']), int(kv['kind'])
        e = 0.0
        if cross:
            crossed += 1
            s_got = float(kv['s'])
            p = o + dv * s_got
            cx, cy = int(np.floor(p[0] / 4096.0)), int(np.floor(p[1] / 4096.0))
            h = cells.get((cx, cy), None)
            h = deflt if h is None else h
            s = (h - o[2]) / dv[2] if abs(dv[2]) > 1e-12 else float('inf')
            e = max(e, abs(s - s_got) / max(1.0, abs(s)))
            above = dv[2] < 0
            e = max(e, 0.0 if int(kv['above']) == int(above) else 1.0)
            w = rec(kind)
            c = min(abs(dv[2]), 1.0)
            Fr = w['fresnel'] if JUDGE_RED == 'nofresnel' else w['fresnel'] + (1 - w['fresnel']) * (1 - c) ** 5
            wR = min(max(Fr * w['reflectivity'] if above else Fr, 0.0), 1.0)
            e = max(e, abs(wR - float(kv['R'])))
            L = (thit - s if thit >= 0 else raymax) if above else s
            e = max(e, float(np.abs(fog(w, L) - T0).max()))
            if 'mirror' in t:
                mirrored += 1
                k = t.index('mirror')
                tm = float(t[k + 1])
                Tm = np.array([float(v) for v in t[k + 2:k + 5]])
                want = np.ones(3) if above else fog(w, s + (tm if tm >= 0 else raymax))
                e = max(e, float(np.abs(want - Tm).max()))
            elif wR > 0:
                e = max(e, 1.0)
        elif under:
            under_n += 1
            w = rec(kind)
            e = max(e, float(np.abs(fog(w, thit if thit >= 0 else raymax) - T0).max()))
        else:
            e = max(e, float(np.abs(T0 - 1.0).max()))
        worst = max(worst, e)
        n += 1
        if e > 2e-4:
            bad += 1
            if bad <= 6:
                print('  probe %s ray %s off by %.3g' % (t[1], t[2], e))
    ok = crossed >= 50 and bad == 0
    print('%s D bake: %d rays recomputed (%d cross the water, %d with a mirror leg, %d from under it), %d off, worst %.2e%s'
          % ('PASS' if ok else 'FAIL', n, crossed, mirrored, under_n, bad, worst,
             ('  judge red ' + JUDGE_RED) if JUDGE_RED else ''))
    return ok


def stage_p(esm, probes_tsv, pin_tsv=None):
    """The placer: every level-0 first-hit probe in a cell with water stands an eye above the water line (the split
    column's probe) or under it (the split's underwater probe, never deeper than the column's floor); the counts are
    the ones the placer's census line says. pin_tsv (the bake pinned without water): a column whose DRY floor already
    stood its probe exactly an eye over the line is no split column, and is counted as dry ground."""
    cells, deflt = world_water(esm)
    coincident = set()
    if pin_tsv:
        for l in open(pin_tsv, encoding='latin1'):
            t = l.rstrip('\r\n').split('\t')
            if l.startswith('#') or t[0] == 'id' or t[1] != 'first-hit' or t[2] != '0':
                continue
            hp = cells.get((int(t[6]), int(t[7])), deflt)
            if hp is not None and hp < 1e30 and abs(float(t[5]) - (hp + 120.0)) < 0.5:
                coincident.add((round(float(t[3])), round(float(t[4]))))
    eye, cols, under_c = 120.0, None, None
    above = drowned = dry = 0
    for l in open(probes_tsv, encoding='latin1'):
        if l.startswith('#'):
            if ' eye ' in l:
                eye = float(l.split(' eye ')[1].split()[0])
            if 'split at the water line:' in l:
                tail = l.split('split at the water line:')[1]
                cols = int(tail.split()[0])
                under_c = int(tail.split('under the water in')[1].split()[0].rstrip(')'))
            continue
        t = l.rstrip('\r\n').split('\t')
        if t[0] == 'id' or t[1] != 'first-hit' or t[2] != '0':
            continue
        x, y, z, cx, cy = float(t[3]), float(t[4]), float(t[5]), int(t[6]), int(t[7])
        h = cells.get((cx, cy), None)
        h = deflt if h is None else h
        if h is None or h > 1e30:
            continue
        if abs(z - (h + eye)) < 0.5 and (round(x), round(y)) not in coincident:
            above += 1
        elif z < h:
            drowned += 1
        else:
            dry += 1
    ok = cols is not None and above >= 20 and above == cols and drowned == under_c
    print('%s P placer: %d probes an eye above the water line, %d under it, %d on dry ground in water cells; census %s'
          ' split columns, %s under' % ('PASS' if ok else 'FAIL', above, drowned, dry, cols, under_c))
    return ok


if __name__ == '__main__':
    st = sys.argv[1]
    if st == 'forms':   # the harness's list for WW_CELL_WATER_DUMP_FORMS
        print(','.join('%08x' % f for f in sorted(esm_records(sys.argv[2], b'WATR'))))
        sys.exit(0)
    if st == 'A':
        ok = stage_a(sys.argv[2], sys.argv[3])
    elif st == 'B':
        ok = stage_b(sys.argv[2], sys.argv[3])
    elif st == 'C':
        ok = stage_c(sys.argv[2], sys.argv[3], sys.argv[4])
    elif st == 'P':
        ok = stage_p(sys.argv[2], sys.argv[3], sys.argv[4] if len(sys.argv) > 4 else None)
    elif st == 'D':
        ok = stage_d(sys.argv[2], sys.argv[3])
    else:
        print('FAIL unknown stage ' + st)
        ok = False
    sys.exit(0 if ok else 1)
