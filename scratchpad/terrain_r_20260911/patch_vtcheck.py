p = 'tests/spells/lodgen_vt_check.py'
s = open(p, 'r', encoding='utf-8', newline='').read()


def rep(a, b, n=1):
    global s
    assert s.count(a) == n, (a[:70], s.count(a))
    s = s.replace(a, b)


rep("""		self.sheets = []
		for i in range(4):""",
    """		self.sheets = []
		# SIX descriptors since container version 2 (0xA0..0xCF); v1 held four
		for i in range(6):""")

rep("""		fmt = self.sheets[s]['dxgiCover'] if (self.sheets[s]['role'] == 3 and cover) \\
			else self.sheets[s]['dxgi']""",
    """		# the cover carrier is whichever sheet declares TWO formats -- the mask
		# by default, the colour sheet under --vt-cover-in-color
		sd = self.sheets[s]
		fmt = sd['dxgiCover'] if (cover and sd['dxgiCover'] != sd['dxgi']) else sd['dxgi']""")

rep("""		check('V1 %s magic, version and header size' % os.path.basename(p),
			  v.magic == b'LDTX' and v.version == 1 and v.headerBytes == HDR)""",
    """		check('V1 %s magic, version and header size' % os.path.basename(p),
			  v.magic == b'LDTX' and v.version == 2 and v.headerBytes == HDR,
			  'version %d' % v.version)""")

rep("""		check('V1 %s FOUR sheets: colour, msn, data and HEIGHT' % os.path.basename(p),
			  v.sheetCount == 4 and [s['role'] for s in v.sheets[:4]] == [1, 2, 3, 4]
			  and v.sheets[3]['dxgi'] == 56,
			  'roles %s' % [s['role'] for s in v.sheets[:4]])
		check('V1 %s the reserved tail is zero' % os.path.basename(p),
			  v.b[0xC0:0x100] == b'\\0' * 64)""",
    """		roles = [x['role'] for x in v.sheets[:v.sheetCount]]
		# version 2 (docs/LODGEN_TERRAIN_VT.md 2.2): colour, msn, MASK, then
		# height if it was asked for, then emissive if any layer supplies one.
		# Role 3 `data` is RETIRED and must appear nowhere.
		check('V1 %s the object texture family: colour, msn and MASK, in that order'
			  % os.path.basename(p),
			  roles[:3] == [1, 2, 5] and 3 not in roles,
			  'roles %s' % roles)
		check('V1 %s the HEIGHT sheet is role 4 and R16' % os.path.basename(p),
			  4 in roles and v.sheets[roles.index(4)]['dxgi'] == 56,
			  'roles %s' % roles)
		# exactly ONE sheet may declare two formats, and it is the cover carrier
		carriers = [i for i, x in enumerate(v.sheets[:v.sheetCount])
					if x['dxgiCover'] != x['dxgi']]
		check('V1 %s exactly one sheet carries the ground-cover alpha'
			  % os.path.basename(p),
			  len(carriers) == 1 and v.sheets[carriers[0]]['role'] in (1, 5),
			  'carriers %s' % carriers)
		check('V1 %s the reserved tail is zero' % os.path.basename(p),
			  v.b[0xD0:0x100] == b'\\0' * 48)""")

open(p, 'w', encoding='utf-8', newline='').write(s)
b = open(p, 'rb').read()
print('CR', b.count(b'\r'), 'LF', b.count(b'\n'))
