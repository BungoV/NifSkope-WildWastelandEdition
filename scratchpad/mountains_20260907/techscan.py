"""Does the Fallout 4 exe contain a static list of valid BSLightingShader
technique ids, and is 0x0D01 (Vc + LODObj) in it? Read-only. Lane IDENTITY.
Usage: python techscan.py <path to Fallout4.exe>"""
import struct, sys

b = open(sys.argv[1], 'rb').read()

def scan(target):
    pat = struct.pack('<I', target)
    hits = []
    i = 0
    while True:
        j = b.find(pat, i)
        if j < 0:
            break
        if j % 4 == 0 and j >= 32:
            w = struct.unpack_from('<12I', b, j - 32)
            if all(v < 0x4000 for v in w):
                hits.append((j, ['%x' % x for x in w]))
        i = j + 4
    return hits

for t in (0x0d01, 0x0d00, 0x0f01, 0x0f00):
    h = scan(t)
    print('0x%04x : %d aligned hits inside a small-u32 neighbourhood' % (t, len(h)))
    for j, w in h[:6]:
        print('    at 0x%08x : %s' % (j, w))
