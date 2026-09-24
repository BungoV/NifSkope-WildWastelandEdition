"""VTNORMAL1: compare two .lodt files sheet by sheet, tile by tile.
usage: cmp_tiles.py A.lodt B.lodt [--half]
Without --half: prints, per role, how many present tiles have byte-identical
sheets (all mips). With --half: B is a half-aux bake of the same ground; every
B sheet with mipSkip 1 must equal A's same sheet's mips 1.. exactly, and every
mipSkip-0 sheet must equal A's whole sheet. One verdict line per role."""
import sys, struct, mmap

HDR, STRIDE = 256, 24
ROLE = {1: 'color', 2: 'msn', 4: 'height', 5: 'mask', 6: 'emissive', 7: 'horizon'}


class L(object):
	def __init__(self, p):
		self.f = open(p, 'rb')
		self.m = mmap.mmap(self.f.fileno(), 0, access=mmap.ACCESS_READ)
		b = self.m[:HDR]
		self.table_off = struct.unpack_from('<Q', b, 0x18)[0]
		(self.dim, _, _, self.tx, self.ty, self.content, self.border, self.stored) = struct.unpack_from('<8H', b, 0x68)
		(self.mips, self.nsheets) = struct.unpack_from('<2B', b, 0x78)
		self.ntiles = struct.unpack_from('<I', b, 0x7C)[0]
		self.sheets = []
		for i in range(self.nsheets):
			f0, f1, role, sp, skip = struct.unpack_from('<HHBBB', b, 0xA0 + i * 8)
			self.sheets.append((f0, f1, role, skip))

	def mipbytes(self, s, m, cover):
		f0, f1, role, skip = self.sheets[s]
		if m + skip >= self.mips:
			return 0
		side = self.stored >> (m + skip)
		if role == 4:
			return side * side * 2
		fmt = f1 if (cover and f1 != f0) else f0
		return (side // 4) ** 2 * (16 if fmt in (77, 78) else 8)

	def entry(self, i):
		return struct.unpack_from('<QIIIHH', self.m, self.table_off + i * STRIDE)

	def sheet(self, i, s, from_mip=0):
		off, st, raw, crc, flags, res = self.entry(i)
		if not flags & 1:
			return None
		cover = bool(flags & 2)
		o = off
		for k in range(s):
			for m in range(self.mips):
				o += self.mipbytes(k, m, cover)
		for m in range(from_mip):
			o += self.mipbytes(s, m, cover)
		n = sum(self.mipbytes(s, m, cover) for m in range(from_mip, self.mips))
		return self.m[o:o + n]


def main():
	a, b = L(sys.argv[1]), L(sys.argv[2])
	half = '--half' in sys.argv
	assert a.ntiles == b.ntiles and a.nsheets == b.nsheets, 'grids differ'
	for s in range(a.nsheets):
		role = ROLE.get(a.sheets[s][2], str(a.sheets[s][2]))
		same = present = 0
		skip = b.sheets[s][3]
		for i in range(a.ntiles):
			sa = a.sheet(i, s, from_mip=(skip if half else 0))
			sb = b.sheet(i, s)
			if sa is None or sb is None:
				continue
			present += 1
			same += (sa == sb)
		print('%s sheet %d %-8s skipA %d skipB %d: %d of %d tiles identical%s'
			  % ('half' if half else 'cmp', s, role, a.sheets[s][3], skip, same, present,
				 ' (B vs A mips %d..)' % skip if half else ''))


if __name__ == '__main__':
	main()
