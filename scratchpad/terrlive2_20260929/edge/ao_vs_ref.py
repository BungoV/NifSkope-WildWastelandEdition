"""TERRLIVE2: the .loda (32 u) against the old FULL bake's baked AO (mask sheet B, 16 u) over the same cells.
usage: python ao_vs_ref.py <file.loda> <FULL VT.2.lodt> x0 y0 x1 y1
Prints mean / percentiles of both over the painted quadrants in the rect, and their correlation."""
import struct, sys, zlib
import numpy as np
sys.path.insert(0, __file__.rsplit('\\', 1)[0].rsplit('/', 1)[0])
import vtread

raw = open(sys.argv[1], 'rb').read()
assert raw[:4] == b'LODA'
ver, hb, q, cnt = struct.unpack_from('<4I', raw, 4)
idx = np.frombuffer(raw, '<i2', cnt * 2, hb).reshape(cnt, 2)
ao = np.frombuffer(zlib.decompress(raw[hb + cnt * 4:]), np.uint8).reshape(cnt, q, q)
x0, y0, x1, y1 = map(int, sys.argv[3:7])
v = vtread.Vt(sys.argv[2])
m, wW, nN = v.mosaic(x0, y0, x1, y1, role=5)
upt = 4096.0 * v.levelDim / v.content
k = int(round(32.0 / upt))
A, R = [], []
for n in range(cnt):
    qx, qy = int(idx[n, 0]), int(idx[n, 1])
    if not (2 * x0 <= qx <= 2 * x1 + 1 and 2 * y0 <= qy <= 2 * y1 + 1):
        continue
    blk = ao[n][::-1]                                   # row 0 = north
    c0 = int(round((qx * 2048.0 - wW * 4096.0) / upt))
    r0 = int(round((nN * 4096.0 - (qy + 1) * 2048.0) / upt))
    if upt <= 32.0:                                     # finer reference: box it down to 32 u
        ref = m[r0:r0 + q * k, c0:c0 + q * k, 2].astype(np.float32)
        if ref.shape != (q * k, q * k):
            continue
        a = blk.astype(np.float32); ref = ref.reshape(q, k, q, k).mean((1, 3))
    else:                                               # coarser reference: box the .loda up to it
        j = int(round(upt / 32.0)); n2 = q // j
        ref = m[r0:r0 + n2, c0:c0 + n2, 2].astype(np.float32)
        if ref.shape != (n2, n2):
            continue
        a = blk.astype(np.float32).reshape(n2, j, n2, j).mean((1, 3))
    A.append(a.ravel()); R.append(ref.ravel())
A = np.concatenate(A); R = np.concatenate(R)
p = lambda x: ' '.join(f'{v:5.1f}' for v in np.percentile(x, [5, 25, 50, 75, 95]))
print(f'texels compared {len(A)} at {max(upt, 32.0):.0f} u (reference level {upt:.0f} u)')
print(f'  .loda 32 u  mean {A.mean():6.1f}  p5/25/50/75/95 {p(A)}')
print(f'  ref FULL B  mean {R.mean():6.1f}  p5/25/50/75/95 {p(R)}')
print(f'  corr {np.corrcoef(A, R)[0, 1]:.3f}  mean |diff| {np.abs(A - R).mean():.1f}')
