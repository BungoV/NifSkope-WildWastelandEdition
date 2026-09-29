"""TILING5 -- is the macro field correlated with the hex patches?

Re-implements, term by term, lodgenWarpHash / lodgenLandHexCell /
lodgenLandHexOffset / lodgenMacroNoise / lodgenMacroField (src/lodgen.cpp) in
numpy uint32 arithmetic, then measures Pearson r between each macro channel and
the hex patch offsets over the Commonwealth:

  (a) at every hex lattice vertex in a window: field(vertex pos) vs offset keys 0/1
  (b) at 400k random world points: field vs the dominant vertex's offsets and
      vs the barycentric-weighted offsets (what the join actually shows)
  (c) the three channels against each other

A |r| bound for "uncorrelated" is set from the sample size: 4/sqrt(N) (4 sigma
of the null).  Prints one verdict line per test and writes logs/hexcorr.json.

    usage: python t5_hexcorr.py
"""
import json, os, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
HEX = 256.0
SKEW = 0.57735026918962576
SCALE = 1.15470053837925152
LAT = (4096.0, 16384.0, 65536.0)
WT = (0.25, 0.5, 1.0)
M32 = np.uint64(0xFFFFFFFF)


def whash(i, j, k):
    i = np.asarray(i, dtype=np.int64).astype(np.uint64) & M32
    j = np.asarray(j, dtype=np.int64).astype(np.uint64) & M32
    k = np.uint64(k)
    h = (i * np.uint64(374761393) + j * np.uint64(668265263) + k * np.uint64(2246822519)) & M32
    h ^= h >> np.uint64(13)
    h = (h * np.uint64(1274126177)) & M32
    h ^= h >> np.uint64(16)
    return h


def noise(wx, wy, lat, key):
    gx = wx / lat; gy = wy / lat
    fi = np.floor(gx); fj = np.floor(gy)
    i = fi.astype(np.int64); j = fj.astype(np.int64)
    fx = gx - fi; fy = gy - fj
    sx = fx * fx * fx * (fx * (fx * 6.0 - 15.0) + 10.0)
    sy = fy * fy * fy * (fy * (fy * 6.0 - 15.0) + 10.0)
    a = whash(i, j, key) / 4294967296.0
    b = whash(i + 1, j, key) / 4294967296.0
    c = whash(i, j + 1, key) / 4294967296.0
    d = whash(i + 1, j + 1, key) / 4294967296.0
    v = (a * (1 - sx) + b * sx) * (1 - sy) + (c * (1 - sx) + d * sx) * sy
    return v * 2.0 - 1.0


def field(wx, wy, ch):
    s = 0.0
    for o in range(3):
        s = s + WT[o] * noise(wx, wy, LAT[o], 0x7A5E0000 + 16 * ch + o)
    return s / sum(WT)


def hexcell(wx, wy):
    px = wx / HEX; py = wy / HEX
    sx = px - SKEW * py; sy = SCALE * py
    bi = np.floor(sx); bj = np.floor(sy)
    tx = sx - bi; ty = sy - bj; tz = 1.0 - tx - ty
    up = tz > 0
    o = np.where(up, 0.0, 1.0)
    w = np.stack([np.where(up, tz, -tz), np.where(up, ty, 1 - ty), np.where(up, tx, 1 - tx)], 1)
    vi = np.stack([bi + o, bi + o, bi + 1 - o], 1).astype(np.int64)
    vj = np.stack([bj + o, bj + 1 - o, bj + o], 1).astype(np.int64)
    return vi, vj, w


def r(a, b):
    return float(np.corrcoef(a.ravel(), b.ravel())[0, 1])


def main():
    out = {}
    fails = 0
    # (a) lattice vertices in a window around Boston, 600x600 vertices
    ii, jj = np.meshgrid(np.arange(-300, 300), np.arange(-300, 300))
    py = jj / SCALE; px = ii + SKEW * py
    vx = px * HEX; vy = py * HEX - 24576.0
    o0 = whash(ii, jj, 0) / 4294967296.0
    o1 = whash(ii, jj, 1) / 4294967296.0
    n = ii.size
    bound = 4.0 / np.sqrt(n)
    for ch, name in enumerate(("bright", "hue", "sat")):
        f = field(vx, vy, ch)
        for k, o in ((0, o0), (1, o1)):
            v = r(f, o)
            ok = abs(v) < bound
            fails += (not ok)
            out["vertex_%s_key%d" % (name, k)] = v
            print("(a) vertex  %-6s vs offset key %d: r=%+.5f  |r|<%.5f  %s" % (name, k, v, bound, "PASS" if ok else "FAIL"))
    # control: a "macro" that reused the hex's key 0 on the hex's own lattice
    # (the mistake this test exists to catch) must FAIL (a).
    ctrl = r(noise(ii.astype(float), jj.astype(float), 1.0, 0), o0)
    ctrl_fails = abs(ctrl) >= bound
    out["control_key0_on_hex_lattice"] = ctrl
    print("(a) CONTROL key-0 noise on the hex lattice vs offset key 0: r=%+.5f  -> %s (must FAIL)" % (ctrl, "FAIL" if ctrl_fails else "PASS"))
    if not ctrl_fails:
        fails += 1
    # (b) random points over the Commonwealth
    rng = np.random.default_rng(20260927)
    N = 400000
    wx = rng.uniform(-96 * 4096, 96 * 4096, N)
    wy = rng.uniform(-96 * 4096, 96 * 4096, N)
    vi, vj, w = hexcell(wx, wy)
    dom = np.argmax(w, 1)
    oxd = (whash(vi, vj, 0) / 4294967296.0)[np.arange(N), dom]
    oyd = (whash(vi, vj, 1) / 4294967296.0)[np.arange(N), dom]
    oxw = ((whash(vi, vj, 0) / 4294967296.0) * w).sum(1)
    wmax = w.max(1)
    bound = 4.0 / np.sqrt(N)
    F = [field(wx, wy, ch) for ch in range(3)]
    for ch, name in enumerate(("bright", "hue", "sat")):
        for lab, o in (("dominant ox", oxd), ("dominant oy", oyd), ("weighted ox", oxw), ("max weight", wmax)):
            v = r(F[ch], o)
            ok = abs(v) < bound
            fails += (not ok)
            out["point_%s_%s" % (name, lab.replace(" ", "_"))] = v
            print("(b) point   %-6s vs %-11s: r=%+.5f  |r|<%.5f  %s" % (name, lab, v, bound, "PASS" if ok else "FAIL"))
    # (c) channels against each other: expected independent, but the sample is
    # spatially correlated at ~km scale, so the bound is set by the number of
    # independent 65536-unit cells (~ (2*96*4096/65536)^2 = 144): 4/sqrt(144)
    bound_c = 4.0 / np.sqrt((2 * 96 * 4096 / 65536.0) ** 2)
    for a, b in ((0, 1), (0, 2), (1, 2)):
        v = r(F[a], F[b])
        ok = abs(v) < bound_c
        fails += (not ok)
        out["chan_%d_%d" % (a, b)] = v
        print("(c) channel %d vs %d: r=%+.4f  |r|<%.3f  %s" % (a, b, v, bound_c, "PASS" if ok else "FAIL"))
    out["field_sd"] = [float(f.std()) for f in F]
    out["field_range"] = [[float(f.min()), float(f.max())] for f in F]
    print("field SD per channel: %s  range %s" % (["%.3f" % s for s in out["field_sd"]], [["%.2f" % x for x in rr] for rr in out["field_range"]]))
    os.makedirs(os.path.join(HERE, "logs"), exist_ok=True)
    json.dump(out, open(os.path.join(HERE, "logs", "hexcorr.json"), "w"), indent=1)
    print("HEXCORR %s (%d fails)" % ("PASS" if fails == 0 else "FAIL", fails))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
