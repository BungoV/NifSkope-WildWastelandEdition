"""GATEFIX1: wipe the scratch scope before EVERY shot, not only around the run.

Measured: with the scope wiped once, the first window saves its layout into the
scope and the next window opens with a different viewport (991 vs 989 rows), and
leg (c) refuses to compare frames of two sizes. Each shot now starts from the
same empty settings tree.
"""
P = 'E:/Projects/NifskopeWWE-gatefix1/tests/spells/native_open.sh'
with open(P, 'rb') as fh:
	src = fh.read()
cr0 = src.count(b'\r')
old = b'''	local ctr="${SHOT_CENTER:-$CENTER}"
	rm -f "$out"
	env "$@" WW_SETTINGS_SCOPE="$SCOPE" \\'''
new = b'''	local ctr="${SHOT_CENTER:-$CENTER}"
	rm -f "$out"
	# EVERY window from an empty scope: a window saves its layout on close, and
	# the next one then opens a different viewport (991 against 989 rows,
	# measured), which (c) rightly refuses to compare
	wipe_scope
	env "$@" WW_SETTINGS_SCOPE="$SCOPE" \\'''
assert src.count(old) == 1
src = src.replace(old, new)
assert src.count(b'\r') == cr0
with open(P + '.new', 'wb') as fh:
	fh.write(src)
print('patched')
