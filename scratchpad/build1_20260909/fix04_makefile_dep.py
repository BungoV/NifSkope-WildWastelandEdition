#!/usr/bin/env python
"""BUILD1: Makefile.Release does not know btdterrain.o depends on lodtfile.h.

`src/btdterrain.cpp` includes `lodtfile.h` (line 13) and puts a `LodtFile` on
the stack in `lodtReadWorldInfo`. Lane TERRAINFIX added three members to that
class (ver, defWaterH, defWaterType), so the object grew -- but qmake's
generated dependency list for btdterrain.o was written before that include
existed and names `src/lodtfile.h` for nifcli.o, lodgenmanager.o and
lodtfile.o only. make therefore left btdterrain.o at 15:30 while everything
else rebuilt at 17:09, and the 17:09 exe carried one translation unit that
believed in the old, smaller LodtFile: `LodtFile::open()` wrote past the end
of the caller's stack object and every `-no-gui lodt` run segfaulted (rc 139),
on version 1 and version 2 files alike.

This adds the dependency so the next edit of lodtfile.h rebuilds btdterrain.o
too. It is a stopgap: Makefile.Release is generated, and a qmake re-run is the
real fix -- recorded in MISTAKES.md.

Makefile.Release is CRLF throughout; the inserted line must be CRLF too.
"""
import os

P = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                 '..', '..', 'Makefile.Release')
P = os.path.normpath(P)

b = open(P, 'rb').read()
cr, lf = b.count(b'\r'), b.count(b'\n')
assert cr == lf, (cr, lf)

i = b.find(b'GeneratedFiles/.obj/btdterrain.o:')
assert i > 0, 'no btdterrain.o rule'
j = b.find(b'\r\n\t$(CXX)', i)
assert j > i, 'no recipe line after the btdterrain.o dependency list'
head = b[i:j]
assert b'src/lodtfile.h' not in head, 'the dependency is already there'
# The list ends without a continuation backslash; add one, then the new line.
assert head.endswith(b'.h') or head.endswith(b'.hpp'), head[-40:]
b = b[:j] + b' \\\r\n\t\tsrc/lodtfile.h' + b[j:]

assert b.count(b'\r') == b.count(b'\n')
assert b.count(b'\r') == cr + 1, (b.count(b'\r'), cr)
open(P, 'wb').write(b)
print('patched %s, CR %d LF %d' % (P, b.count(b'\r'), b.count(b'\n')))
