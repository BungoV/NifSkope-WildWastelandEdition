import io
p = 'NifSkope.pro'
s = io.open(p, encoding='utf-8', newline='').read()
b = s
h = '\tsrc/hkxanim.h \\\n'
c = '\tsrc/hkxanim.cpp \\\n'
assert h in s, 'header line not found'
assert c in s, 'source line not found'
s = s.replace(h, h + '\tsrc/hkxplayback.h \\\n', 1)
s = s.replace(c, c + '\tsrc/hkxplayback.cpp \\\n\tsrc/hkxplaybacktest.cpp \\\n', 1)
assert s != b
io.open(p, 'w', encoding='utf-8', newline='').write(s)
print('cr=', open(p, 'rb').read().count(b'\r'))
