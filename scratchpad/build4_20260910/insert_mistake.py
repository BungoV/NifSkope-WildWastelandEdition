#!/usr/bin/env python
"""Insert lane BUILD4's entry at the TOP of MISTAKES.md (newest first).

LF-only file; CR asserted 0 on both sides. The header block is kept above the
first entry, so the insert point is the first `## ` heading.
"""
P = 'E:/Projects/NifskopeWildWastelandEdition/MISTAKES.md'
E = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/build4_20260910/mistakes_entry.md'

b = open(P, 'rb').read()
e = open(E, 'rb').read()
assert b.count(b'\r') == 0 and e.count(b'\r') == 0, 'both must be LF-only'
assert b'qmake\'s dependency scan does not follow into' not in b, 'already inserted'

i = b.find(b'\n## ')
assert i > 0, 'no entry heading found'
i += 1                                     # keep the newline that ends the header

out = b[:i] + e.rstrip(b'\n') + b'\n\n' + b[i:]
open(P, 'wb').write(out)

a = open(P, 'rb').read()
print('MISTAKES.md  %d -> %d bytes, CR %d, LF %d -> %d'
      % (len(b), len(a), a.count(b'\r'), b.count(b'\n'), a.count(b'\n')))
assert a.count(b'\r') == 0
assert a[:i] == b[:i] and a.endswith(b[i:]), 'existing bytes moved'
print('ok')
