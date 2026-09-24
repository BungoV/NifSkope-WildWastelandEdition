# -*- coding: utf-8 -*-
"""Splice lane WATER3's two entries into MISTAKES.md, newest first.

Append-only in effect: the header block is kept byte for byte, the two entries
go immediately under it, and everything that was there follows unchanged. The
file is read and written in one pass and its length is asserted to grow by
exactly the text added, so a concurrent lane's write cannot be silently lost --
if the assertion fails, nothing was written.
"""
import hashlib
import sys

M = '../../MISTAKES.md'
E = 'MISTAKES_ENTRIES.md'

b = open(M, 'rb').read()
assert b.count(b'\r') == 0, 'MISTAKES.md is LF-only'
print('before %s %d bytes %d lines' % (hashlib.sha256(b).hexdigest()[:16], len(b), b.count(b'\n')))

entries = open(E, 'rb').read().decode('utf-8')
# the file's own preamble ends with the "Newest at the top." line
mark = 'Newest at the top.\n\n'
s = b.decode('utf-8')
if s.count(mark) != 1:
    sys.stderr.write('REFUSED: the preamble marker is not unique\n')
    sys.exit(1)

# keep only the two entries themselves, not the splicing note above them
first = entries.index('## 2026-09-10')
add = entries[first:].rstrip('\n') + '\n\n'
if '## 2026-09-10 — a placeholder check sat where a measurement belonged' in s:
    print('already spliced; nothing to do')
    sys.exit(0)

out = s.replace(mark, mark + add, 1)
nb = out.encode('utf-8')
assert nb.count(b'\r') == 0
assert len(nb) == len(b) + len(add.encode('utf-8')), (len(nb), len(b))
open(M, 'wb').write(nb)
b2 = open(M, 'rb').read()
print('after  %s %d bytes %d lines CR=%d'
      % (hashlib.sha256(b2).hexdigest()[:16], len(b2), b2.count(b'\n'), b2.count(b'\r')))
