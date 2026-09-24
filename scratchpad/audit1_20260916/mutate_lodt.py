"""AUDIT1 step 3: flip one byte deep inside a .lodt, well past every header and
tile table, so that only a per-tile payload CRC can notice it.
usage: python mutate_lodt.py <file.lodt>
"""
import sys

p = sys.argv[1]
b = bytearray(open(p, 'rb').read())
i = len(b) // 2
b[i] ^= 0xFF
open(p, 'wb').write(bytes(b))
print('  flipped byte %d of %d' % (i, len(b)))
