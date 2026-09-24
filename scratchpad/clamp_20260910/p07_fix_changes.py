# LANE CLAMP: correct one sentence of the WW_CHANGES entry -- the ceiling run
# fails the band bar on all four chunks AND trips the msn floor, and saying
# "four bars" hid the second half.  Binary splice; the file is mixed.
import sys

PATH = 'WW_CHANGES.md'
b = open(PATH, 'rb').read()
cr = b.count(b'\r')
if cr != 19020:
    print('CR %d, refusing' % cr); sys.exit(1)

old = (b'416,280 of them beyond the 4-texel band and failed four bars (rc 1). Its\n'
       b'ceiling count reproduces')
new = (b'416,280 of them beyond the 4-texel band, failing the band bar on all four\n'
       b'chunks (rc 1). That pair also trips the msn floor, because the ground-cover\n'
       b'tint does not touch the msn and those four sheets are identical on both\n'
       b'sides of it -- which is the floor doing exactly what it is for. Its\n'
       b'ceiling count reproduces')
if b.count(old) != 1:
    print('anchor count %d' % b.count(old)); sys.exit(1)
out = b.replace(old, new)
if out.count(b'\r') != cr:
    print('CR moved'); sys.exit(1)
open(PATH, 'wb').write(out)
print('ok  CR %d' % cr)
