# Lane RESTIRSPV1 red helper: copy a SPIR-V header with ONE array word changed (the last word, xor 1).
#   python -I tests/spvsync/flip_word.py <in.spv.h> <out.spv.h>
import re, sys

b = open(sys.argv[1], 'rb').read()
ws = list(re.finditer(rb'0x[0-9a-f]{8}', b))
if len(ws) < 6:
	print('flip_word: no SPIR-V words in', sys.argv[1]); sys.exit(2)
m = ws[-1]
v = int(m.group(0), 16) ^ 1
b = b[:m.start()] + (b'0x%08x' % v) + b[m.end():]
open(sys.argv[2], 'wb').write(b)
print('flip_word: word %d of %d changed' % (len(ws) - 1, len(ws)))
