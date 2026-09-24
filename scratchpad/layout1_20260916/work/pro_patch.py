import sys
BS = chr(92).encode()
p = 'E:/Projects/NifskopeWildWastelandEdition/NifSkope.pro'
b = open(p, 'rb').read()
if b.find(b'lodgenlayout') >= 0:
    print('already there'); sys.exit(0)
h = b'\tsrc/lodgenchunkpass.h ' + BS + b'\n'
c = b'\tsrc/lodgenchunkpass.cpp ' + BS + b'\n'
assert b.count(h) == 1, ('h', b.count(h))
assert b.count(c) == 1, ('c', b.count(c))
b = b.replace(h, h + b'\tsrc/lodgenlayout.h ' + BS + b'\n')
b = b.replace(c, c + b'\tsrc/lodgenlayout.cpp ' + BS + b'\n')
open(p, 'wb').write(b)
n = open(p, 'rb').read()
print('ok', n.count(b'lodgenlayout'), 'CR', n.count(b'\r'), 'LF', n.count(b'\n'))
