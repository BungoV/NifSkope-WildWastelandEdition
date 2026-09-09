"""Probe of Fallout 4's ShadersFX\\Shaders011.fxp, to answer one question:
does the package contain a compiled BSLightingShader permutation whose
technique ID has the Vc bit (bit 0) set together with the LODObj type
(bits 8-13 == 13), i.e. id 0x0D01? Read-only. Lane IDENTITY.

Layout is discovered, not assumed: the file is a sequence of per-shader-type
tables, each entry a technique id followed by a DXBC blob. We locate every
DXBC blob, read the dword that precedes its length field, and treat that as
the technique id -- then sanity-check the recovered id set against the
technique-name decoder recovered from the exe.
"""
import struct, sys, collections

PATH = sys.argv[1] if len(sys.argv) > 1 else \
    r'C:\Users\bungo\AppData\Local\Temp\claude\laneb\Shaders011.fxp'

TYPES = {0: '', 1: 'Envmap', 2: 'Glowmap', 3: 'Parallax', 4: 'Face',
         5: 'SkinTint', 6: 'Hair', 7: 'ParallaxOcc', 8: 'MTLand',
         9: 'LODLand', 10: '?', 11: 'MultiLayerParallax', 12: 'Tree',
         13: 'LODObj', 14: 'MultiIndexTriShapeSnow', 15: 'LODObjHD',
         16: 'Eye', 17: '?', 18: 'LODLandNoise', 19: 'MTLandLODBlend'}
MODS = {0: 'Vc', 1: 'Sk', 2: 'Msn', 3: 'Projuv', 4: 'b4', 5: 'Pipboy',
        6: 'b6', 7: 'Menu'}

def name(tid):
    s = 'BSLighting'
    for b in range(8):
        if tid & (1 << b):
            s += ' ' + MODS[b]
    t = (tid >> 8) & 0x3F
    s += ' ' + TYPES.get(t, 'type%d' % t)
    return s.strip()

def main():
    b = open(PATH, 'rb').read()
    print('file %d bytes' % len(b))
    print('head', ' '.join('%02x' % c for c in b[:64]))

    offs = []
    i = 0
    while True:
        j = b.find(b'DXBC', i)
        if j < 0:
            break
        offs.append(j)
        i = j + 4
    print('DXBC blobs: %d, first five at %s' % (len(offs), offs[:5]))

    # DXBC header: 'DXBC' + 16-byte checksum + u32 one + u32 totalSize
    good = 0
    ids = collections.Counter()
    pre = collections.Counter()
    for j in offs:
        total = struct.unpack_from('<I', b, j + 24)[0]
        if not (0 < total < len(b) - j):
            continue
        good += 1
        # walk backwards: the bytes just before a blob carry its id/length
        for back in (4, 8, 12, 16):
            if j - back >= 0:
                v = struct.unpack_from('<I', b, j - back)[0]
                pre[(back, v)] += 1
        if j - 8 >= 0:
            ids[struct.unpack_from('<I', b, j - 8)[0]] += 1
    print('well-formed DXBC blobs: %d' % good)
    print('most common dword at blob-8:')
    for v, n in ids.most_common(12):
        print('   0x%08x  x%d' % (v, n))
    return b, offs

if __name__ == '__main__':
    main()
