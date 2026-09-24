#!/usr/bin/env python
"""Turn lane CLAMP's WW_CHANGES.md entry from pending into measured.

WW_CHANGES.md is MIXED (CONSTITUTION 8): the 2026-09 entries at the top are
LF-only, the body below is CRLF. It is spliced in BINARY and the CR count is
asserted unchanged on both sides. No sed, no tr, no redirect -- lane CARDORTHO
lost all 19,020 CRs to one `sed -i` earlier tonight (MISTAKES.md).
"""
P = 'E:/Projects/NifskopeWildWastelandEdition/WW_CHANGES.md'
NEW = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/build4_20260910/clamp_status_new.md'

OLD_HEAD = (b"`docs/LODGEN_TERRAIN_VT.md`. **WRITTEN, NOT BUILT** -- see the status block at\n"
            b"the end of this entry.")
NEW_HEAD = (b"`docs/LODGEN_TERRAIN_VT.md`. **BUILT AND MEASURED** (lane BUILD4) -- see the\n"
            b"status block at the end of this entry.")

OLD_STATUS_START = b"**STATUS: BUILD PENDING.** Nothing here has been compiled."
STATUS_END = b"\n## 2026-09-09"

b = open(P, 'rb').read()
cr0, lf0 = b.count(b'\r'), b.count(b'\n')
assert cr0 == 19020, 'CR was %d, expected 19020 -- do not write' % cr0

assert b.count(OLD_HEAD) == 1, 'head anchor count %d' % b.count(OLD_HEAD)
b = b.replace(OLD_HEAD, NEW_HEAD)

i = b.find(OLD_STATUS_START)
assert i >= 0 and b.count(OLD_STATUS_START) == 1, 'status anchor'
j = b.find(STATUS_END, i)
assert j > i, 'status end'
old_block = b[i:j]
assert old_block.count(b'\r') == 0, 'the block was LF-only, is not now'

new_block = open(NEW, 'rb').read()
assert new_block.count(b'\r') == 0, 'the replacement must be LF-only'
new_block = new_block.rstrip(b'\n')

b = b[:i] + new_block + b[j:]
open(P, 'wb').write(b)

a = open(P, 'rb').read()
print('WW_CHANGES.md  CR %d -> %d   LF %d -> %d   bytes %d'
      % (cr0, a.count(b'\r'), lf0, a.count(b'\n'), len(a)))
assert a.count(b'\r') == cr0, 'CR MOVED -- a mixed file was normalised'
assert b'BUILD PENDING' not in a[:20000], 'the pending block survived'
print('ok')
