"""Add the three native-emitter translation units to NifSkope.pro (LANE NATIVE0b).
Anchors asserted unique; CR count asserted unchanged (the file is LF-only)."""
import sys
P = 'NifSkope.pro'
b = open(P, 'rb').read()
cr0 = b.count(b'\r')
s = b.decode('utf-8')
hdr_anchor = '\tsrc/lodtfile.h \\\n'
src_anchor = '\tsrc/lodgen.cpp \tsrc/lodgenmanager.cpp \tsrc/lodtfile.cpp \\\n'
hdr_new = hdr_anchor + '\tsrc/lodofile.h \\\n\tsrc/lodifile.h \\\n\tsrc/nativeemit.h \\\n'
src_new = src_anchor + '\tsrc/lodofile.cpp \\\n\tsrc/lodifile.cpp \\\n\tsrc/nativeemit.cpp \\\n'
if 'src/lodofile.h' in s:
    print('already applied'); sys.exit(0)
assert s.count(hdr_anchor) == 1, s.count(hdr_anchor)
assert s.count(src_anchor) == 1, s.count(src_anchor)
s = s.replace(hdr_anchor, hdr_new).replace(src_anchor, src_new)
out = s.encode('utf-8')
assert out.count(b'\r') == cr0, (out.count(b'\r'), cr0)
open(P, 'wb').write(out)
print('NifSkope.pro: +3 headers, +3 sources; CR %d -> %d; lines %d -> %d' % (cr0, out.count(b'\r'), b.count(b'\n'), out.count(b'\n')))
