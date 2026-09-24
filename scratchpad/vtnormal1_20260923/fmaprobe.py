"""VTNORMAL1: which float contraction does each exe's chunk-msn encode use?
Re-encode his whole sheet under each fused/unfused variant of
  len = sqrt(e*e + n*n + up*up); x = c*inv; byte = int((x*0.5+0.5)*255+0.5)
and count texels equal to the rung's and the new exe's chunk sheet mip 0.
fma(a,b,c) is emulated as round32(a*b + c) in float64 (the f32 product is exact)."""
import numpy as np
f32 = np.float32
O = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/vtnormal1_20260923/out/'
H = 'E:/Projects/Fallout 4 Mods/mods/Upscaled Terrain Normals/Textures/Terrain/Commonwealth/Commonwealth.4.-20.24_msn.DDS'
W = 2048
his = np.frombuffer(open(H, 'rb').read(), np.uint8, count=W * W * 4, offset=148).reshape(-1, 4)
def mip0(p):
	return np.frombuffer(open(p, 'rb').read(), np.uint8, count=W * W * 4, offset=148).reshape(-1, 4)
rung = mip0(O + 'rung_cache/tex/Commonwealth.4.-20.24_msn.DDS')
new = mip0(O + 'new_cache/tex/Commonwealth.4.-20.24_msn.DDS')

def fma(a, b, c):
	return (a.astype(np.float64) * b + c).astype(f32)

e = (his[:, 0].astype(f32) / f32(255)) * f32(2) - f32(1)
up = (his[:, 1].astype(f32) / f32(255)) * f32(2) - f32(1)
n = (his[:, 2].astype(f32) / f32(255)) * f32(2) - f32(1)
sums = {
	'S0 unfused': (e * e + n * n) + up * up,
	'S1 fma(u,u,fma(n,n,e*e))': fma(up, up, fma(n, n, e * e)),
	'S2 fma(u,u,fma(e,e,n*n))': fma(up, up, fma(e, e, n * n)),
	'S3 fma(u,u,e*e+n*n)': fma(up, up, e * e + n * n),
}
def pack(x, fused):
	t = x * f32(0.5) + f32(0.5)
	v = fma(t, f32(255), f32(0.5)) if fused else (t * f32(255)) + f32(0.5)
	return np.clip(v.astype(np.int64), 0, 255)
for sk, s in sums.items():
	inv = f32(1) / np.maximum(np.sqrt(s.astype(f32)), f32(1e-6))
	for fused in (False, True):
		E, U, N = pack(e * inv, fused), pack(up * inv, fused), pack(n * inv, fused)
		# stored BGRA: byte0 = B = north? measured: the diff sat at byte 0 with north's value
		out = np.stack([N, U, E], -1)
		mr = (out == rung[:, :3]).all(-1).sum()
		mn = (out == new[:, :3]).all(-1).sum()
		print('%-28s pack %-8s  == rung %d  == new %d  (of %d)' % (sk, 'fma' if fused else 'unfused', mr, mn, W * W))
