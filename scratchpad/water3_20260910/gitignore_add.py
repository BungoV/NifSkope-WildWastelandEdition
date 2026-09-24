# -*- coding: utf-8 -*-
"""One allow-list line so lane WATER3's pictures can be committed.

`scratchpad/**/*.png` is excluded wholesale; the four picture folders a handoff
points at are allow-listed one line each. This adds the fifth, beside lane
WATER2's, so the before/after flow pair the resume produces is not silently
dropped.
"""
import hashlib
import sys

P = '../../.gitignore'
b = open(P, 'rb').read()
cr0 = b.count(b'\r')
print('before %s %d bytes %d lines CR=%d'
      % (hashlib.sha256(b).hexdigest()[:16], len(b), b.count(b'\n'), cr0))
s = b.decode('utf-8')
anchor = '!scratchpad/water2_20260909/images/*.png'
add = '\n!scratchpad/water3_20260910/*.png'
if 'water3_20260910' in s:
    print('already present; nothing to do')
    sys.exit(0)
if s.count(anchor) != 1:
    sys.stderr.write('REFUSED: %d matches for the anchor\n' % s.count(anchor))
    sys.exit(1)
s = s.replace(anchor, anchor + add, 1)
out = s.encode('utf-8')
assert out.count(b'\r') == cr0, 'the CR count must not move'
assert len(out) == len(b) + len(add.encode('utf-8'))
open(P, 'wb').write(out)
b2 = open(P, 'rb').read()
print('after  %s %d bytes %d lines CR=%d'
      % (hashlib.sha256(b2).hexdigest()[:16], len(b2), b2.count(b'\n'), b2.count(b'\r')))
