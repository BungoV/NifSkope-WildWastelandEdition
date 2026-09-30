# Offline re-run of lodgenRuleBuild's choice on the RDMP dump, with variants.
import numpy as np, rdmp, sys
d = rdmp.load('rule_dump.bin'); F = d['forms']; pc = d['palCol'].astype(np.float64); P = len(pc)
pf = d['palFrom']; NF = len(F)
rockP = np.array([bool(rdmp.ROCK.search(F[f][2])) for f in pf])
s, h, V = d['s'], d['h'], d['V'].astype(np.float64)
painted = d['mw'].sum(1) > 0; have = d['have'] > 0
SL = np.digitize(np.degrees(np.arctan(s)), rdmp.SLOPE); NS, NHB = 7, 6
# painted truth mass per form -> per palette (area-weighted like 'fine')
mid = d['mid'].astype(int); mw = d['mw']
fine_h = np.clip(((h + 40960) // 256).astype(int), 0, 511)
# height edges: equal painted mass
hm = np.bincount(fine_h[painted], weights=mw[painted].sum(1), minlength=512); c = np.cumsum(hm)
hEdge = [int(np.searchsorted(c, c[-1] * (e + 1) / NHB, 'left')) + 1 for e in range(NHB - 1)]
HB = np.searchsorted(np.array(hEdge), fine_h, 'right')
BIN = SL * NHB + HB
form2pal = {}
for p in range(P): form2pal.setdefault(pf[p], p)
cnt = np.zeros((NS * NHB, P))
for k in range(8):
    m = painted & (mid[:, k] != 255)
    for f in np.unique(mid[m, k]):
        # every palette entry pointing at that form gets the mass (code counts fine[..][palFrom[p]] for each p)
        ps = [p for p in range(P) if pf[p] == f]
        mm = m & (mid[:, k] == f)
        add = np.bincount(BIN[mm], weights=mw[mm, k], minlength=NS * NHB)
        for p in ps: cnt[:, p] += add
area = np.array([F[f][1] for f in pf]); gshare = area / area.sum(); kappa = 8.0
def run(lam=2.0, K=8, lumaW=1.0, rockBoost=None, idx=None):
    prior = (cnt + kappa * gshare) / (cnt.sum(1, keepdims=True) + kappa)
    if rockBoost is not None:  # rockBoost[slopebin] = floor on total rock prior mass
        for b in range(NS * NHB):
            t = rockBoost[b // NHB]
            if t <= 0: continue
            r = prior[b, rockP].sum()
            if r < t:
                prior[b, rockP] *= t / max(r, 1e-9); prior[b, ~rockP] *= (1 - t) / max(1 - r, 1e-9)
    idx = np.flatnonzero(have) if idx is None else idx
    A = np.zeros(len(idx), int); B = np.zeros(len(idx), int); W = np.ones(len(idx))
    Y = np.array([0.299, 0.587, 0.114])
    for b in np.unique(BIN[idx]):
        sel = np.flatnonzero(BIN[idx] == b); v = V[idx[sel]]; pr = prior[b]
        top = np.argsort(-pr, kind='stable')[:K]
        best = np.full(len(sel), 1e30)
        for i in range(K):
            for j in range(i, K):
                pa, pb = top[i], top[j]; ca, cb = pc[pa], pc[pb]
                if i == j: w = np.ones(len(sel))
                else:
                    dd = ca - cb; den = dd @ dd
                    w = np.clip(((v - cb) @ dd) / den, 0, 1) if den > 1e-9 else np.ones(len(sel))
                r = 255 * (v - (cb + np.outer(w, ca - cb)))
                if lumaW != 1.0:
                    l = r @ Y; r = r - np.outer(l, [1, 1, 1]) + lumaW * np.outer(l, [1, 1, 1])
                err = np.sqrt((r * r).sum(1))
                sc = err - lam * np.log(np.maximum(1e-6, w * pr[pa] + (1 - w) * pr[pb]))
                better = sc < best; best[better] = sc[better]
                A[sel[better]] = pa; B[sel[better]] = pb; W[sel[better]] = w[better]
    return idx, A, B, W
def report(tag, idx, A, B, W):
    col = pc[A] * W[:, None] + pc[B] * (1 - W[:, None]); v = V[idx]
    rk = rockP[A] * W + rockP[B] * (1 - W); out = ~painted[idx]
    Y = np.array([0.299, 0.587, 0.114]); e = 255 * (col - v); l = e @ Y; ch = e - l[:, None]
    line = []
    for k in range(1, 7):
        m = out & (SL[idx] == k)
        line.append(f"b{k}: rock {rk[m].mean():.2f} rmse {np.sqrt((e[m]**2).sum(1)).mean():4.1f} chroma {np.sqrt((ch[m]**2).sum(1)).mean():4.1f}")
    mp = ~out
    print(f"{tag:22s} | " + " | ".join(line) + f" | painted rmse {np.sqrt((e[mp]**2).sum(1)).mean():4.1f}")
if __name__ == '__main__':
    rng = np.random.default_rng(2)
    idx = np.flatnonzero(have); idx = np.sort(rng.choice(idx, 400000, replace=False))
    i, A, B, W = run(idx=idx)
    wa = d['w'][i] / 255.0
    agree = ((d['a'][i] == A) & (d['b'][i] == B)) | ((d['a'][i] == B) & (d['b'][i] == A)) | ((d['a'][i] == d['b'][i]) & ((A == d['a'][i]) | (B == d['a'][i])) & ((W > .99) | (W < .01)))
    print('reproduce agreement', round(agree.mean(), 3))
    report('dump (C++)', i, d['a'][i], d['b'][i], wa); report('numpy baseline', i, A, B, W)
