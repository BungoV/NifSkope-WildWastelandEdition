import sys
P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/lodgen_vt_check.py'
s = open(P, 'rb').read().decode('utf-8')


def sub(old, new):
	global s
	if s.count(old) != 1:
		sys.exit('anchor count %d:\n%s' % (s.count(old), old))
	s = s.replace(old, new)


sub("""			f0, f1, role, space = struct.unpack_from('<HHBB', b, 0xA0 + i * 8)
			self.sheets.append({'dxgi': f0, 'dxgiCover': f1, 'role': role, 'space': space})""",
"""			f0, f1, role, space, skip = struct.unpack_from('<HHBBB', b, 0xA0 + i * 8)
			# byte 6 is mipSkip (lane VTNORMAL1): 1 on a half-resolution sheet,
			# which stores the full sheet's mips 1.. and no mip 0
			self.sheets.append({'dxgi': f0, 'dxgiCover': f1, 'role': role, 'space': space,
								'skip': skip})""")
sub("""	def sheetMipBytes(self, s, mip, cover):
		side = self.stored >> mip""", """	def sheetMipBytes(self, s, mip, cover):
		skip = self.sheets[s]['skip']
		if mip + skip >= self.mips:
			return 0
		side = self.stored >> (mip + skip)""")
sub("""		o = self.sheetOffset(bool(e['flags'] & 2), hs, 0)
		n = self.stored
""", """		o = self.sheetOffset(bool(e['flags'] & 2), hs, 0)
		n = self.stored >> self.sheets[hs]['skip']
""")
open(P, 'wb').write(s.encode('utf-8'))
print('check ok')
