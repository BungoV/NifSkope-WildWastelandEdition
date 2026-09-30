# TERRLIVE2: offline luma-weight sweep on the RDMP dump: rock share, |lum| and signed lum per slope bin outside.
import numpy as np, tune
rng = np.random.default_rng(2)
idx = np.sort(rng.choice(np.flatnonzero(tune.have), 300000, replace=False))
Y = np.array([0.299, 0.587, 0.114])
for lw in (1.0, 0.75, 0.5, 0.25):
    i, A, B, W = tune.run(idx=idx, lumaW=lw)
    col = tune.pc[A] * W[:, None] + tune.pc[B] * (1 - W[:, None]); e = 255 * (col - tune.V[i]); l = e @ Y
    rk = tune.rockP[A] * W + tune.rockP[B] * (1 - W); out = ~tune.painted[i]
    s = []
    for k in range(1, 7):
        m = out & (tune.SL[i] == k)
        s.append(f"b{k} rock {rk[m].mean():.2f} lum {l[m].mean():+5.1f}/{np.abs(l[m]).mean():4.1f}")
    m = out
    print(f"lumaW {lw:4.2f} (K {1-lw:.2f}) | all-out |lum| {np.abs(l[m]).mean():4.1f} signed {l[m].mean():+5.1f} | " + " | ".join(s))
