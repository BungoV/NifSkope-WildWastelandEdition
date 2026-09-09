"""Second probe of Shaders011.fxp: dump the bytes around the first blobs so the
record layout can be read off, then, once the technique-id field is located,
enumerate every BSLightingShader technique id in the package. Read-only."""
import struct, sys, collections

PATH = r'C:\Users\bungo\AppData\Local\Temp\claude\laneb\Shaders011.fxp'
b = open(PATH, 'rb').read()

offs = []
i = 0
while True:
    j = b.find(b'DXBC', i)
    if j < 0:
        break
    offs.append(j)
    i = j + 4

mode = sys.argv[1] if len(sys.argv) > 1 else 'dump'

if mode == 'dump':
    print('header:', ' '.join('%02x' % c for c in b[:76]))
    for j in offs[:6]:
        total = struct.unpack_from('<I', b, j + 24)[0]
        print('--- blob at %d, DXBC total=%d, ends %d' % (j, total, j + total))
        s = max(0, j - 40)
        print('    before:', ' '.join('%02x' % c for c in b[s:j]))
        print('    dwords:', ['%08x' % v for v in struct.unpack_from('<10I', b, s)])
        e = j + total
        print('    after :', ' '.join('%02x' % c for c in b[e:e + 40]))

elif mode == 'walk':
    # hypothesis: records are laid out back to back as
    #   <fields...> DXBC blob <fields...> DXBC blob ...
    # so the gap between the END of one blob and the START of the next holds the
    # next record's header. Print the gap dwords for the first N records.
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 8
    prev_end = 0
    for k, j in enumerate(offs[:n]):
        total = struct.unpack_from('<I', b, j + 24)[0]
        gap = b[prev_end:j]
        gw = len(gap) // 4
        print('rec %d: gap %d bytes = %s' % (
            k, len(gap), ['%08x' % v for v in struct.unpack_from('<%dI' % gw, gap, 0)][:12]))
        prev_end = j + total

elif mode == 'ids':
    # take the dword at a fixed negative offset from each blob start and
    # histogram it, for a range of candidate offsets
    for back in range(4, 64, 4):
        c = collections.Counter()
        for j in offs:
            if j - back >= 0:
                c[struct.unpack_from('<I', b, j - back)[0]] += 1
        distinct = len(c)
        small = sum(n for v, n in c.items() if v < 0x10000)
        print('back=%2d distinct=%5d  values<0x10000: %d  top=%s'
              % (back, distinct, small,
                 [('%x' % v, n) for v, n in c.most_common(4)]))
