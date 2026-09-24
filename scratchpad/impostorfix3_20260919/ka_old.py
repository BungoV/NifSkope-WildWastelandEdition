"""The known-answer block run against WHICHEVER decoder is on disk at
tests/spells/impostor_bc_decode.py -- so it can be run BEFORE the repair (the
red control) and after it, with nothing else changing."""
import sys, numpy as np
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
import impostor_bc_decode as D


def block(a0, a1):
    blk = bytearray(8)
    blk[0] = a0; blk[1] = a1
    bits = 0
    for i in range(15, -1, -1):
        bits = (bits << 3) | (i & 7)
    for i in range(6):
        blk[2 + i] = (bits >> (8 * i)) & 0xFF
    return np.frombuffer(bytes(blk), np.uint8).reshape(1, 8)


def run(name, a0, a1, want):
    got = D._bc3_alpha(block(a0, a1))[0].reshape(16) * 255.0
    seen = {}
    for i in range(16):
        seen[i & 7] = float(got[i])
    nbad = 0
    print('%s  a0=%d a1=%d' % (name, a0, a1))
    for k in sorted(seen):
        g, w = seen[k], want[k]
        bad = abs(g - w) > 0.51
        nbad += bad
        print('   index %d  decoded %7.2f  D3D %7.2f%s' % (k, g, w, '   <-- WRONG' if bad else ''))
    return nbad


a0, a1 = 255.0, 0.0
w8 = [a0, a1] + [((7 - k) * a0 + k * a1) / 7 for k in range(1, 7)]
n8 = run('eight-level (a0 > a1)', 255, 0, w8)
a0, a1 = 40.0, 200.0
w6 = [a0, a1] + [((5 - k) * a0 + k * a1) / 5 for k in range(1, 5)] + [0.0, 255.0]
n6 = run('six-level (a0 <= a1)', 40, 200, w6)
print('known-answer: %d of 16 ramp entries wrong' % (n8 + n6))
