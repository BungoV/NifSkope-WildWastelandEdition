# RDMP v1 reader (WW_RULE_DUMP from lodgenRuleBuild) + rock share by slope bin.
import numpy as np, struct, sys, re
ROCK = re.compile(r'rock|cliff|stone|boulder', re.I)
SLOPE = [4, 8, 14, 22, 32, 45]
def load(p):
    b = open(p, 'rb').read(); o = 0
    def u32():
        nonlocal o; v = struct.unpack_from('<I', b, o)[0]; o += 4; return v
    def arr(dt, n):
        nonlocal o; a = np.frombuffer(b, dt, n, o); o += a.nbytes; return a
    assert u32() == 0x504D4452 and u32() == 1
    NX, NY, NF, P = u32(), u32(), u32(), u32(); N = NX * NY
    d = dict(NX=NX, NY=NY)
    d['V'] = arr('<f4', N * 3).reshape(N, 3); d['s'] = arr('<f4', N); d['h'] = arr('<f4', N)
    d['have'] = arr('u1', N); d['mid'] = arr('u1', N * 8).reshape(N, 8); d['mw'] = arr('<f4', N * 8).reshape(N, 8)
    forms = []
    for _ in range(NF):
        fid = u32(); area = struct.unpack_from('<d', b, o)[0]; o += 8; L = u32()
        forms.append((fid, area, b[o:o + L].decode('utf8', 'replace'))); o += L
    pf, pc = [], []
    for _ in range(P):
        pf.append(u32()); pc.append(struct.unpack_from('<3f', b, o)); o += 12
    d['forms'] = forms; d['palFrom'] = np.array(pf); d['palCol'] = np.array(pc, np.float32)
    d['a'] = arr('u1', N); d['b'] = arr('u1', N); d['w'] = arr('u1', N)
    assert o == len(b), (o, len(b))
    return d
def rockshares(d, a=None, b=None, w=None):
    a = d['a'] if a is None else a; b = d['b'] if b is None else b; w = d['w'] if w is None else w
    formRock = np.array([bool(ROCK.search(f[2])) for f in d['forms']] + [False] * 256)
    palRock = formRock[d['palFrom']]
    mid = d['mid'].astype(int); mid[mid == 255] = len(d['forms'])
    truth = (formRock[mid] * d['mw']).sum(1) / np.maximum(d['mw'].sum(1), 1e-9)
    painted = d['mw'].sum(1) > 0
    wa = w / 255.0; rule = palRock[a] * wa + palRock[b] * (1 - wa)
    sb = np.digitize(np.degrees(np.arctan(d['s'])), SLOPE)
    rows = []
    for k in range(len(SLOPE) + 1):
        m = sb == k; mp = m & painted; mo = m & ~painted & (d['have'] > 0)
        rows.append((k, int(mp.sum()), int(mo.sum()),
                     truth[mp].mean() if mp.any() else np.nan, rule[mp].mean() if mp.any() else np.nan,
                     rule[mo].mean() if mo.any() else np.nan))
    return rows
if __name__ == '__main__':
    d = load(sys.argv[1])
    print(f"N={d['NX']}x{d['NY']} forms={len(d['forms'])} rockforms={sum(bool(ROCK.search(f[2])) for f in d['forms'])} pal={len(d['palFrom'])}")
    print('bin  deg<   nPaint  nOut   truth  rule@paint  rule@out')
    for k, np_, no, t, rp, ro in rockshares(d):
        print(f"{k:3d} {([*SLOPE,90])[k]:5d} {np_:8d} {no:7d}  {t:6.3f}  {rp:9.3f}  {ro:8.3f}")
