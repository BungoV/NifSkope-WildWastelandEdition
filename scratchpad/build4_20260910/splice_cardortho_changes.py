#!/usr/bin/env python
"""Turn lane CARDORTHO's WW_CHANGES.md entry from pending into measured.

Binary splice; WW_CHANGES.md is MIXED and its CR count is asserted unchanged.
No sed, no tr, no redirect (MISTAKES.md, 2026-09-10).
"""
P = 'E:/Projects/NifskopeWildWastelandEdition/WW_CHANGES.md'
NEW = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/build4_20260910/cardortho_status_new.md'

START = b"STATUS: NOT BUILT. Lanes WATER2 and CLAMP build first;"
END = b"\n## 2026-09-10 - `.lodl` version 3:"

b = open(P, 'rb').read()
cr0, lf0 = b.count(b'\r'), b.count(b'\n')
assert cr0 == 19020, 'CR was %d, expected 19020 -- do not write' % cr0

i = b.find(START)
assert i >= 0 and b.count(START) == 1, 'status anchor count %d' % b.count(START)
j = b.find(END, i)
assert j > i, 'status end anchor'
old = b[i:j]
assert old.count(b'\r') == 0, 'the block was LF-only, is not now'

new = open(NEW, 'rb').read()
assert new.count(b'\r') == 0, 'the replacement must be LF-only'
new = new.rstrip(b'\n')

b = b[:i] + new + b[j:]
open(P, 'wb').write(b)

a = open(P, 'rb').read()
print('WW_CHANGES.md  CR %d -> %d   LF %d -> %d   bytes %d'
      % (cr0, a.count(b'\r'), lf0, a.count(b'\n'), len(a)))
assert a.count(b'\r') == cr0, 'CR MOVED -- a mixed file was normalised'
assert START not in a, 'the pending block survived'
print('ok')
