"""Mean colour of a BC3 terrain LOD texture, taken from its 4x4 mip.

One block is the whole 4x4 level, so its two RGB565 endpoints averaged are the
texture's own average to within the encoder's error. Far cheaper than decoding
512x512, and the number that matters here is a mean.
"""
import struct, sys, os, colorsys

def mip_offsets(w, h, mips, blockBytes):
    off = 128
    out = []
    for i in range(mips):
        mw, mh = max(1, w >> i), max(1, h >> i)
        sz = max(1, (mw + 3) // 4) * max(1, (mh + 3) // 4) * blockBytes
        out.append((mw, mh, off, sz))
        off += sz
    return out

def mean565(path):
    b = open(path, 'rb').read()
    h, w = struct.unpack_from('<II', b, 12)
    mips = struct.unpack_from('<I', b, 28)[0]
    fcc = b[84:88]
    bb = 16 if fcc in (b'DXT5', b'DXT3') else 8
    colOff = 8 if bb == 16 else 0        # BC3: alpha block first
    levels = mip_offsets(w, h, mips, bb)
    pick = None
    for (mw, mh, off, sz) in levels:
        if mw <= 4 and mh <= 4:
            pick = (mw, mh, off, sz); break
    if pick is None:
        pick = levels[-1]
    mw, mh, off, sz = pick
    o = off + colOff
    c0, c1 = struct.unpack_from('<HH', b, o)
    def rgb(c):
        return (((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31)
    a, c = rgb(c0), rgb(c1)
    m = tuple((a[i] + c[i]) / 2.0 for i in range(3))
    lum = 0.2126 * m[0] + 0.7152 * m[1] + 0.0722 * m[2]
    hsv = colorsys.rgb_to_hsv(m[0] / 255, m[1] / 255, m[2] / 255)
    return w, h, m, lum, hsv[1]

if __name__ == '__main__':
    for p in sys.argv[1:]:
        try:
            w, h, m, lum, sat = mean565(p)
            print('%-28s %dx%d  rgb %5.1f %5.1f %5.1f  lum %5.1f  sat %.3f'
                  % (os.path.basename(p), w, h, m[0], m[1], m[2], lum, sat))
        except Exception as e:
            print('%-28s ERROR %s' % (os.path.basename(p), e))
