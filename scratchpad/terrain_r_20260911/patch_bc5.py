p = 'tests/spells/lodgen_terrain_model.py'
s = open(p, 'r', encoding='utf-8', newline='').read()


def rep(a, b, n=1):
    global s
    assert s.count(a) == n, (a[:70], s.count(a))
    s = s.replace(a, b)


rep("""		if self.fourcc == b'DXT1':
			self.blockBytes = 8
		elif self.fourcc in (b'DXT3', b'DXT5'):
			self.blockBytes = 16
		else:
			raise ValueError('unsupported fourCC %r in %s' % (self.fourcc, path))""",
    """		if self.fourcc == b'DXT1':
			self.blockBytes = 8
		elif self.fourcc in (b'DXT3', b'DXT5'):
			self.blockBytes = 16
		elif self.fourcc in (b'BC5U', b'ATI2'):
			# MEASURED, and it is the whole reason the gloss lives in GREEN:
			# every Fallout 4 landscape `_s` map is BC5U -- TWO channels, two
			# BC4 blocks of 8 bytes, R then G, with B 0 and A 1. It is not a
			# colour map and it never had a blue channel to lose.
			self.blockBytes = 16
		else:
			raise ValueError('unsupported fourCC %r in %s' % (self.fourcc, path))""")

rep("""		for by in range(bh):
			for bx in range(bw):
				o = off + (by * bw + bx) * self.blockBytes
				a = [1.0] * 16
				co = o""",
    """		if self.fourcc in (b'BC5U', b'ATI2'):
			for by in range(bh):
				for bx in range(bw):
					o = off + (by * bw + bx) * 16
					r = self._bc4(o)
					g = self._bc4(o + 8)
					for j in range(4):
						for i in range(4):
							x, y = bx * 4 + i, by * 4 + j
							if x >= w or y >= h:
								continue
							k = j * 4 + i
							px[y * w + x] = (r[k], g[k], 0.0, 1.0)
			self._cache[m] = (px, w, h)
			return self._cache[m]
		for by in range(bh):
			for bx in range(bw):
				o = off + (by * bw + bx) * self.blockBytes
				a = [1.0] * 16
				co = o""")

rep("""	@staticmethod
	def _565(c):""",
    """	def _bc4(self, o):
		\"\"\"One 8-byte BC4 block as sixteen 0..1 floats, in texel order.\"\"\"
		a0, a1 = self.b[o], self.b[o + 1]
		bits = int.from_bytes(self.b[o + 2:o + 8], 'little')
		tab = [a0, a1]
		if a0 > a1:
			tab += [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)]
		else:
			tab += [((5 - i) * a0 + i * a1) // 5 for i in range(1, 5)]
			tab += [0, 255]
		return [tab[(bits >> (3 * i)) & 7] / 255.0 for i in range(16)]

	@staticmethod
	def _565(c):""")

open(p, 'w', encoding='utf-8', newline='').write(s)
b = open(p, 'rb').read()
print('CR', b.count(b'\r'), 'LF', b.count(b'\n'))
