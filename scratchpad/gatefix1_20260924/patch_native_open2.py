"""GATEFIX1: native_open.sh must not hand its own LODL / PORT to lodl_open.sh.

Both spells read an environment variable called LODL and mean different files
(native_open: the one-look-bake landscape; lodl_open: the FO4CS mod's whole
worldspace), so a run that passes LODL= to native_open silently re-points the
lodl_open leg at the wrong file. The sub-gate runs with its own defaults.
"""
P = 'E:/Projects/NifskopeWWE-gatefix1/tests/spells/native_open.sh'
with open(P, 'rb') as fh:
	src = fh.read()
cr0 = src.count(b'\r')
old = b'''	lo=$(bash "$ROOT/tests/spells/lodl_open.sh" 2>&1 | tail -3)
'''
new = b'''	# its OWN fixtures: lodl_open.sh reads LODL and PORT too and means another
	# file, so an LODL= given to this harness must not reach it (EXE and PY may)
	lo=$(env -u LODL -u PORT bash "$ROOT/tests/spells/lodl_open.sh" 2>&1 | tail -3)
'''
assert src.count(old) == 1
src = src.replace(old, new)
assert src.count(b'\r') == cr0
with open(P + '.new', 'wb') as fh:
	fh.write(src)
print('patched')
