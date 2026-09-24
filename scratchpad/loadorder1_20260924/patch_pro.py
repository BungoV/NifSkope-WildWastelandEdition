"""Lane LOADORDER1: add src/lodgenloadorder.{h,cpp} to NifSkope.pro. LF-only; --check writes nothing."""
import sys

PATH = 'E:/Projects/NifskopeWWE-loadorder1/NifSkope.pro'
check = '--check' in sys.argv
with open(PATH, 'rb') as f:
    src = f.read()
cr0 = src.count(b'\r')
EDITS = [
    (b'\tsrc/lodgenchunkpass.h \\\n', b'\tsrc/lodgenchunkpass.h \\\n\tsrc/lodgenloadorder.h \\\n'),
    (b'\tsrc/lodgenchunkpass.cpp \\\n', b'\tsrc/lodgenchunkpass.cpp \\\n\tsrc/lodgenloadorder.cpp \\\n'),
]
out = src
for i, (a, b) in enumerate(EDITS, 1):
    n = out.count(a)
    if n != 1:
        print('edit %d: anchor count %d, refusing' % (i, n)); sys.exit(1)
    out = out.replace(a, b)
assert out.count(b'\r') == cr0
print('both anchors once; CR %d' % cr0)
if not check:
    with open(PATH, 'wb') as f:
        f.write(out)
    print('written')
