"""VTNORMAL1: a PNG-form msn cache (R east, G north, the old cleaned-cache law)
made from his DDS sheets, into THIS lane's folder only, so the PNG reader of
both exes can be compared. His folder is only read."""
import zlib, struct, os
import numpy as np
SRC = 'E:/Projects/Fallout 4 Mods/mods/Upscaled Terrain Normals/Textures/Terrain/Commonwealth/'
DST = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/vtnormal1_20260923/pngcache/'
os.makedirs(DST, exist_ok=True)


def png(path, a):
	h, w, _ = a.shape
	raw = b''.join(b'\x00' + a[y].tobytes() for y in range(h))
	def chunk(t, d):
		return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
	open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
						   + chunk(b'IDAT', zlib.compress(raw, 1)) + chunk(b'IEND', b''))


k = 0
for cx in (-24, -20, -16):
	for cy in (20, 24, 28):
		p = SRC + 'Commonwealth.4.%d.%d_msn.DDS' % (cx, cy)
		if not os.path.exists(p):
			continue
		b = open(p, 'rb').read()
		h, w = struct.unpack_from('<II', b, 12)
		a = np.frombuffer(b, np.uint8, count=w * h * 4, offset=148).reshape(h, w, 4)
		out = np.stack([a[..., 0], a[..., 2], np.zeros_like(a[..., 0])], -1).copy()
		png(DST + 'Commonwealth.4.%d.%d.png' % (cx, cy), out)
		k += 1
print('png cache sheets', k)
