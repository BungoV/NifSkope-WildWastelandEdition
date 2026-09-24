#!/usr/bin/env python
"""Splice CELLVIEW3's entries into the top of the root MISTAKES.md.

MISTAKES.md is CRLF throughout (measured, not assumed); the entry file is
written LF by the editor. The splice converts the entry to CRLF by bytes and
inserts it immediately before the first `## ` header, so "newest at the top"
holds. CR counts are printed before and after: the file's line endings are a
measurement, never an impression (CONSTITUTION rule 8).
"""
import os

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')
TARGET = os.path.normpath(os.path.join(ROOT, 'MISTAKES.md'))
ENTRY = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'mistakes_entry.txt')

b = open(TARGET, 'rb').read()
print('before: %d bytes, CR %d, LF %d, CRLF %d'
      % (len(b), b.count(b'\r'), b.count(b'\n'), b.count(b'\r\n')))

e = open(ENTRY, 'rb').read().replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
assert e.count(b'\r') == e.count(b'\n') == e.count(b'\r\n'), 'entry not pure CRLF'
assert b.count(b'CELLVIEW3') == 0, 'CELLVIEW3 entries are already in the ledger'

i = b.find(b'\r\n## ')
assert i > 0, 'no `## ` header found to insert before'
i += 2                                   # after the CRLF, at the `## `
out = b[:i] + e + b[i:]
open(TARGET, 'wb').write(out)

c = open(TARGET, 'rb').read()
print('after:  %d bytes, CR %d, LF %d, CRLF %d'
      % (len(c), c.count(b'\r'), c.count(b'\n'), c.count(b'\r\n')))
assert c.count(b'\r') == c.count(b'\n') == c.count(b'\r\n'), 'mixed endings after splice'
print('inserted %d bytes at offset %d' % (len(e), i))
