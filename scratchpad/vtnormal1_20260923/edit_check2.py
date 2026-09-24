"""VTNORMAL1: lodgen_vt_check filter on a half-aux file says why it does not
apply instead of dying on an index (its fine heights have no mip 0)."""
import sys
P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/lodgen_vt_check.py'
s = open(P, 'rb').read().decode('utf-8')
assert s.count('\r') == 0
old = """	fine = Lodv(finePath)
	coarse = Lodv(coarsePath)
	check('V8 the two levels share one origin (a coarse tile covers exactly four fine)',"""
new = """	fine = Lodv(finePath)
	coarse = Lodv(coarsePath)
	# a half-resolution height sheet (--vt-half-aux, descriptor byte 6) stores
	# no mip 0, so the exact per-texel law has nothing to read at the fine side
	if any(x['role'] == 4 and x['skip'] for x in fine.sheets + coarse.sheets):
		print('  skip V8/V10: half-resolution height sheet (mipSkip 1); the law is checked on the full bake')
		return
	check('V8 the two levels share one origin (a coarse tile covers exactly four fine)',"""
if s.count(old) != 1:
	sys.exit('anchor count %d' % s.count(old))
s = s.replace(old, new)
open(P, 'wb').write(s.encode('utf-8'))
print('check2 ok')
