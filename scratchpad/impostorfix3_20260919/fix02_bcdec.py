"""IMPOSTORFIX3 fix 02 -- the BC3 alpha ramp in the gate's own decoder.

tests/spells/impostor_bc_decode.py:26-28. BOTH interpolation modes are one step
short, not just the eight-level one IMPOSTORFIX2 reported:

  8-level (a0 >  a1): D3D is pal[k+1] = ((7-k)*a0 + k*a1)/7, k = 1..6.
                      The file writes (6-k) for k = 1..5 and never assigns
                      pal[7], so pal[7] decodes as 0.
  6-level (a0 <= a1): D3D is pal[k+1] = ((5-k)*a0 + k*a1)/5, k = 1..4,
                      pal[6] = 0, pal[7] = 255.
                      The file writes (4-k) for k = 1..3 and leaves pal[5] at
                      the 0 the eight-level branch put there.

Both errors read LOW, which is the direction that makes the gate SKIP texels it
was meant to test (impostor_sheet_check.py selects covered texels by this alpha
against covBase = 160).
"""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/impostor_bc_decode.py'
b = open(P, 'rb').read()
cr0 = b.count(b'\r')
s = b.decode('utf-8')

A = """    for k in range(1,6): pal[:,k+1]=np.where(big,((6-k)*a0+k*a1)/7,0)
    for k in range(1,4): pal[:,k+1]=np.where(big,pal[:,k+1],((4-k)*a0+k*a1)/5)
    pal[:,6]=np.where(big,pal[:,6],0.0); pal[:,7]=np.where(big,pal[:,7],255.0)
"""
B = """    # D3D10 BC4/BC3 alpha ramps. BOTH were one step short until 2026-09-19
    # (lane IMPOSTORFIX3): the eight-level branch ran k=1..5 with (6-k) and
    # never assigned index 7; the six-level branch ran k=1..3 with (4-k) and
    # never assigned index 5. Both read LOW, so the gate that selects covered
    # texels by this alpha silently skipped the texels it was meant to test.
    for k in range(1,7): pal[:,k+1]=np.where(big,((7-k)*a0+k*a1)/7,0)
    for k in range(1,5): pal[:,k+1]=np.where(big,pal[:,k+1],((5-k)*a0+k*a1)/5)
    pal[:,6]=np.where(big,pal[:,6],0.0); pal[:,7]=np.where(big,pal[:,7],255.0)
"""
n = s.count(A)
if n != 1:
    print('ANCHOR COUNT %d (want 1)' % n); sys.exit(2)
s = s.replace(A, B)

# The known-answer block the gate row runs. Kept in the decoder's own file so
# the ruler carries its own proof and nothing can run the decoder without it.
TAIL = '''

def known_answer():
	"""A hand-built BC3 alpha block carrying every index 0..7, decoded against
	the D3D10 ramp written out by hand. Returns (rows, worst) where rows is
	[(index, got, want)] -- the gate prints them and fails on any disagreement.

	The block: a0 = 255, a1 = 0 (a0 > a1, so the EIGHT-level ramp), then six
	bytes of 3-bit indices, texel i taking index i & 7, so all eight appear
	twice. 16 bytes total, the same shape a .DDS carries."""
	blk = bytearray(8)
	blk[0] = 255; blk[1] = 0
	bits = 0
	for i in range(15, -1, -1):
		bits = (bits << 3) | (i & 7)
	for i in range(6):
		blk[2+i] = (bits >> (8*i)) & 0xFF
	a0, a1 = 255.0, 0.0
	want = [a0, a1] + [((7-k)*a0 + k*a1)/7 for k in range(1, 7)]
	got = _bc3_alpha(np.frombuffer(bytes(blk), np.uint8).reshape(1, 8))[0].reshape(16)*255.0
	rows, worst = [], 0.0
	for i in range(16):
		k = i & 7
		rows.append((k, float(got[i]), want[k]))
		worst = max(worst, abs(float(got[i]) - want[k]))
	return rows, worst


def known_answer_six():
	"""The same for the SIX-level ramp. a0 = 40, a1 = 200 -- NOT a0 = 0,
	because with a0 = 0 the wrong coefficient (4-k) and the right one (5-k)
	multiply zero and agree, and the row would only catch the missing index 5.
	Indices 6 and 7 are the hard 0 and 255 the format defines."""
	blk = bytearray(8)
	blk[0] = 40; blk[1] = 200
	bits = 0
	for i in range(15, -1, -1):
		bits = (bits << 3) | (i & 7)
	for i in range(6):
		blk[2+i] = (bits >> (8*i)) & 0xFF
	a0, a1 = 40.0, 200.0
	want = [a0, a1] + [((5-k)*a0 + k*a1)/5 for k in range(1, 5)] + [0.0, 255.0]
	got = _bc3_alpha(np.frombuffer(bytes(blk), np.uint8).reshape(1, 8))[0].reshape(16)*255.0
	rows, worst = [], 0.0
	for i in range(16):
		k = i & 7
		rows.append((k, float(got[i]), want[k]))
		worst = max(worst, abs(float(got[i]) - want[k]))
	return rows, worst


if __name__ == '__main__':
	nbad = 0
	for name, fn in (('eight-level (a0 > a1)', known_answer), ('six-level (a0 <= a1)', known_answer_six)):
		rows, worst = fn()
		seen = {}
		for k, g, w in rows:
			seen[k] = (g, w)
		print('%s: worst error %.2f levels of 255' % (name, worst))
		for k in sorted(seen):
			g, w = seen[k]
			flag = '' if abs(g - w) <= 0.51 else '   <-- WRONG'
			print('   index %d  decoded %7.2f  D3D %7.2f%s' % (k, g, w, flag))
			if abs(g - w) > 0.51:
				nbad += 1
	print('known-answer: %d of 16 ramp entries wrong' % nbad)
	raise SystemExit(1 if nbad else 0)
'''
if 'def known_answer' in s:
    print('known_answer already present -- refusing'); sys.exit(2)
s = s + TAIL.replace('\t', '    ')

out = s.encode('utf-8')
if out.count(b'\r') != cr0:
    print('CR COUNT MOVED %d -> %d' % (cr0, out.count(b'\r'))); sys.exit(2)
open(P, 'wb').write(out)
print('fix02 applied: %d -> %d bytes, CR %d -> %d' % (len(b), len(out), cr0, out.count(b'\r')))
