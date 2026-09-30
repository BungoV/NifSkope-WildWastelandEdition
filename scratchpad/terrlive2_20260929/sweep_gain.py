# TERRLIVE2: offline: luma-half choice + per-sample brightness gain (clamped), lum error per slope bin outside.
import numpy as np, tune
rng = np.random.default_rng(2)
idx = np.sort(rng.choice(np.flatnonzero(tune.have), 300000, replace=False))
Y = np.array([0.299, 0.587, 0.114])
for lw in (1.0, 0.5):
    i, A, B, W = tune.run(idx=idx, lumaW=lw)
    col = tune.pc[A] * W[:, None] + tune.pc[B] * (1 - W[:, None]); v = tune.V[i]
    for lo, hi in ((1, 1), (0.5, 2.0), (0.75, 1.5)):
        g = np.clip((v @ Y) / np.maximum(col @ Y, 1e-4), lo, hi)
        e = 255 * (col * g[:, None] - v); l = e @ Y; ch = e - l[:, None]
        rk = tune.rockP[A] * W + tune.rockP[B] * (1 - W); out = ~tune.painted[i]
        s = [f"b{k} rock {rk[out & (tune.SL[i]==k)].mean():.2f} |lum| {np.abs(l[out & (tune.SL[i]==k)]).mean():4.1f} chroma {np.sqrt((ch[out & (tune.SL[i]==k)]**2).sum(1)).mean():4.1f}" for k in range(1, 7)]
        print(f"lumaW {lw} gain [{lo},{hi}] | out |lum| {np.abs(l[out]).mean():4.1f} | " + " | ".join(s))
    gg = np.clip((v @ Y) / np.maximum(col @ Y, 1e-4), 0, 4)[~tune.painted[i]]
    print('  gain pct 1/50/99:', np.round(np.percentile(gg, [1, 50, 99]), 2))
