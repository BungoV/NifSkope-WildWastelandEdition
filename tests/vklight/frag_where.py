"""Lane VKLIGHT1 round 2: where a fragmatch job's GL and Vulkan targets differ (rows, channels, worst texels).

    python -I tests/vklight/frag_where.py <job dir> [rt index] [<red dir>]
"""
import os, sys
import numpy as np

d = sys.argv[1]
k = int(sys.argv[2]) if len(sys.argv) > 2 else 0
vdir = sys.argv[3] if len(sys.argv) > 3 else d
gl = np.load(os.path.join(d, 'gl_rt%d.npy' % k))
H, W = gl.shape[:2]
vk = np.fromfile(os.path.join(vdir, 'vk_rt%d.bin' % k), np.float32, offset=8).reshape(H, W, 4)


def ordv(a):
    i = a.view(np.int32).astype(np.int64)
    return np.where(i < 0, -(i & 0x7FFFFFFF), i)


u = np.abs(ordv(gl) - ordv(vk))
rel = np.abs(gl.astype(np.float64) - vk) / np.maximum(np.maximum(np.abs(gl), np.abs(vk)), 1e-30)
print('per channel: differ', [int((u[..., c] > 0).sum()) for c in range(4)], 'max ulp', [int(u[..., c].max()) for c in range(4)],
      'max rel', ['%.3g' % rel[..., c].max() for c in range(4)])
rows = (u > 0).any(-1).sum(1)
print('rows with differences (row: count), bottom = 0:')
print(' '.join('%d:%d' % (r, rows[r]) for r in range(H) if rows[r]))
print('ulp histogram:', {b: int(((u > lo) & (u <= b)).sum()) for lo, b in ((0, 1), (1, 4), (4, 16), (16, 64), (64, 256), (256, 1 << 30))})
idx = np.argsort(u.reshape(-1))[::-1][:10]
for i in idx:
    y, x, c = np.unravel_index(i, u.shape)
    print('px (%d, %d) ch %d: gl %.9g vk %.9g ulp %d' % (x, y, c, gl[y, x, c], vk[y, x, c], u[y, x, c]))
