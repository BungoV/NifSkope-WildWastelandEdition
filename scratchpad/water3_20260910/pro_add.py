# -*- coding: utf-8 -*-
"""Add the two new translation units to NifSkope.pro.

Beside `src/lodtfile.*`, because that is the file they extend. LF-only, and the
byte delta is asserted to be exactly the text added -- a .pro is read by qmake
and a stray CR in it is a filename with a CR in it.
"""
import hashlib
import sys

P = '../../NifSkope.pro'
b = open(P, 'rb').read()
assert b.count(b'\r') == 0, 'NifSkope.pro is LF-only'
before = len(b)
print('before %s %d bytes %d lines' % (hashlib.sha256(b).hexdigest()[:16], before, b.count(b'\n')))
s = b.decode('utf-8')

adds = [
    ('\tsrc/lodtfile.h \\\n',
     '\tsrc/lodtfile.h \\\n\tsrc/watermark.h \\\n\tsrc/watermarkpanel.h \\\n'),
    ('\tsrc/lodgen.cpp \tsrc/lodgenmanager.cpp \tsrc/lodtfile.cpp \\\n',
     '\tsrc/lodgen.cpp \tsrc/lodgenmanager.cpp \tsrc/lodtfile.cpp \\\n'
     '\tsrc/watermark.cpp \\\n\tsrc/watermarkpanel.cpp \\\n'),
]
added = 0
for old, new in adds:
    n = s.count(old)
    if n != 1:
        sys.stderr.write('REFUSED: %d matches for %r\n' % (n, old))
        sys.exit(1)
    s = s.replace(old, new)
    added += len(new) - len(old)

out = s.encode('utf-8')
assert out.count(b'\r') == 0
assert len(out) == before + added, (len(out), before, added)
open(P, 'wb').write(out)
b2 = open(P, 'rb').read()
print('after  %s %d bytes %d lines CR=%d'
      % (hashlib.sha256(b2).hexdigest()[:16], len(b2), b2.count(b'\n'), b2.count(b'\r')))
print('added %d bytes, 4 lines' % added)
