"""VTNORMAL1: the chunk _msn bytes that differ rung vs new -- which pixel, what
his sheet holds there, and the float32 arithmetic of the encode, to see if the
value sits on a rounding edge (an FMA-contraction effect) or not."""
import numpy as np
O = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/vtnormal1_20260923/out/'
a = open(O + 'rung_cache/tex/Commonwealth.4.-20.24_msn.DDS', 'rb').read()
b = open(O + 'new_cache/tex/Commonwealth.4.-20.24_msn.DDS', 'rb').read()
his = open('E:/Projects/Fallout 4 Mods/mods/Upscaled Terrain Normals/Textures/Terrain/Commonwealth/Commonwealth.4.-20.24_msn.DDS', 'rb').read()
A = np.frombuffer(a, np.uint8)
B = np.frombuffer(b, np.uint8)
d = np.nonzero(A != B)[0]
print('header dx10', a[84:88], 'differing offsets', d.tolist())
f = np.float32
for off in d:
	if off - 148 >= 2048 * 2048 * 4:
		print(off, 'in a lower mip, rung', A[off], 'new', B[off])
		continue
	p = (off - 148) // 4
	ch = (off - 148) % 4
	row, col = divmod(p, 2048)
	src = his[148 + p * 4:148 + p * 4 + 4]
	e = f(src[0]) / f(255) * f(2) - f(1)
	up = f(src[1]) / f(255) * f(2) - f(1)
	n = f(src[2]) / f(255) * f(2) - f(1)
	s = np.sqrt(f(e * e + n * n + up * up))
	inv = f(1) / s
	vals = [(e * inv * f(0.5) + f(0.5)) * f(255) + f(0.5), (up * inv * f(0.5) + f(0.5)) * f(255) + f(0.5), (n * inv * f(0.5) + f(0.5)) * f(255) + f(0.5)]
	# double-precision view of how close to an integer edge the value sits
	E, U, N = [float(x) / 255 * 2 - 1 for x in src[:3]]
	L = (E * E + N * N + U * U) ** 0.5
	exact = [((E / L) * 0.5 + 0.5) * 255 + 0.5, ((U / L) * 0.5 + 0.5) * 255 + 0.5, ((N / L) * 0.5 + 0.5) * 255 + 0.5]
	print('off %d pixel row %d col %d byte %d: rung %d new %d | his rgba %s | f32 pre-trunc %s | f64 %s'
		  % (off, row, col, ch, A[off], B[off], list(src), ['%.6f' % v for v in vals], ['%.6f' % v for v in exact]))
