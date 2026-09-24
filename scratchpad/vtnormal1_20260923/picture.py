"""VTNORMAL1 job 4: Sanctuary L02 normal, three panels, north up:
  heights (old) | your upscaled, downsampled | vanilla _msn
row 1 = the normals as colour (R east, G north, B up), row 2 = the same lit by
one sun (from the south-west, 35 degrees up), Lambert only. Titles are drawn
with a 5x7 bitmap font so no font package is needed.
usage: picture.py <rung VT.2.lodt> <new VT.2.lodt> <out.png>"""
import sys, zlib, struct
import numpy as np
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/vtnormal1_20260923')
import measure as M

F = {  # 5x7 glyphs, rows top-down, bit 4 = left
	'a': [0, 0, 14, 1, 15, 17, 15], 'b': [16, 16, 30, 17, 17, 17, 30], 'd': [1, 1, 15, 17, 17, 17, 15],
	'e': [0, 0, 14, 17, 31, 16, 14], 'g': [0, 15, 17, 17, 15, 1, 14], 'h': [16, 16, 22, 25, 17, 17, 17],
	'i': [4, 0, 12, 4, 4, 4, 14], 'l': [12, 4, 4, 4, 4, 4, 14], 'm': [0, 0, 26, 21, 21, 17, 17],
	'n': [0, 0, 22, 25, 17, 17, 17], 'o': [0, 0, 14, 17, 17, 17, 14], 'p': [0, 0, 30, 17, 30, 16, 16],
	'r': [0, 0, 22, 25, 16, 16, 16], 's': [0, 0, 15, 16, 14, 1, 30], 't': [8, 8, 28, 8, 8, 9, 6],
	'u': [0, 0, 17, 17, 17, 19, 13], 'v': [0, 0, 17, 17, 17, 10, 4], 'w': [0, 0, 17, 17, 21, 21, 10],
	'c': [0, 0, 14, 16, 16, 17, 14], 'y': [0, 0, 17, 17, 15, 1, 14], 'f': [6, 9, 8, 28, 8, 8, 8],
	'k': [16, 16, 18, 20, 24, 20, 18], 'z': [0, 0, 31, 2, 4, 8, 31], '_': [0, 0, 0, 0, 0, 0, 31],
	'(': [2, 4, 8, 8, 8, 4, 2], ')': [8, 4, 2, 2, 2, 4, 8], ',': [0, 0, 0, 0, 12, 4, 8], ' ': [0] * 7,
	'0': [14, 17, 19, 21, 25, 17, 14], '2': [14, 17, 1, 2, 4, 8, 31], '3': [30, 1, 1, 14, 1, 1, 30],
	'L': [16, 16, 16, 16, 16, 16, 31], 'N': [17, 25, 21, 19, 17, 17, 17], '.': [0, 0, 0, 0, 0, 12, 12],
	'-': [0, 0, 0, 31, 0, 0, 0], '=': [0, 0, 31, 0, 31, 0, 0], 'x': [0, 0, 17, 10, 4, 10, 17],
	'1': [4, 12, 4, 4, 4, 4, 14], '5': [31, 16, 30, 1, 1, 17, 14], '6': [6, 8, 16, 30, 17, 17, 14],
	'8': [14, 17, 17, 14, 17, 17, 14], '4': [2, 6, 10, 18, 31, 2, 2], '9': [14, 17, 17, 15, 1, 2, 12],
	'7': [31, 1, 2, 4, 8, 8, 8], 'S': [15, 16, 16, 14, 1, 1, 30], 'r_': [0] * 7, '/': [1, 1, 2, 4, 8, 16, 16],
}


def text(img, x, y, s, scale=2, col=(255, 255, 255)):
	for ch in s:
		g = F.get(ch, F.get(ch.lower(), F[' ']))
		for r, bits in enumerate(g):
			for c in range(5):
				if bits & (16 >> c):
					img[y + r * scale:y + (r + 1) * scale, x + c * scale:x + (c + 1) * scale] = col
		x += 6 * scale


def png(path, a):
	h, w, _ = a.shape
	raw = b''.join(b'\x00' + a[y].astype(np.uint8).tobytes() for y in range(h))
	def chunk(t, d):
		return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
	open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
						   + chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))


old, _ = M.lodt_mosaic(sys.argv[1])
new, _ = M.lodt_mosaic(sys.argv[2])
van, _ = M.load_vanilla()
N = old.shape[0]
if van.shape[0] > N:
	van = M.box(van, van.shape[0] // N)
sun = np.array([-0.5, -0.5, 0.0])
sun[:2] = sun[:2] / np.linalg.norm(sun[:2]) * np.cos(np.radians(35))
sun[2] = np.sin(np.radians(35))
panels = [('heights (old)', old), ('your upscaled, downsampled', new), ('vanilla _msn', van)]
gap, head = 8, 30
W = 3 * N + 4 * gap
H = 2 * N + 2 * head + 3 * gap
img = np.full((H, W, 3), 24, np.uint8)
for i, (t, v) in enumerate(panels):
	x = gap + i * (N + gap)
	col = np.clip((v * 0.5 + 0.5) * 255, 0, 255)
	lit = np.clip((v * sun).sum(-1), 0, 1)
	lit = np.clip(lit * 255 * 1.1, 0, 255)[..., None].repeat(3, -1)
	img[head + gap:head + gap + N, x:x + N] = col
	img[2 * head + 2 * gap + N:2 * head + 2 * gap + 2 * N, x:x + N] = lit
	text(img, x, gap, t)
	text(img, x, head + gap + N + gap, t + ', lit')
png(sys.argv[3], img)
print('picture %s %dx%d' % (sys.argv[3], W, H))
