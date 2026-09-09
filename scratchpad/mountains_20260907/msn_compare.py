"""Side-by-side of vanilla's terrain LOD normal map against ours, on the same
out-of-bounds ground, shaded with the same sun.

Six panels, two rows. Top row is the raw normal map as stored, so the channel
convention is visible directly. Bottom row is that same normal shaded by one
fixed sun, which is what the eye actually judges.

  col 1  vanilla's shipped _msn
  col 2  ours, as it is now       (up in GREEN, matching vanilla)
  col 3  ours, as it was          (up in BLUE, the transposition)

Column 3 is synthesised from column 2 by swapping the two channels back, rather
than rebuilt, so the three columns differ in exactly one thing.

Shading is 0.25 ambient + 0.75 * max(0, n . L), sun from the north-west at 45
degrees elevation. No tone mapping, no colour: this is about light, not looks.
"""
import math, os, struct, sys, zlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dds import DDS

VAN = r'E:/Tools/Fallout 4/DataUnpacked/Data/Textures/Terrain/Commonwealth'
OURS = r'C:/Users/bungo/AppData/Local/Temp/claude/laneb/gen/peak/tex'

# sun: azimuth 315 (from the north-west), elevation 45
AZ, EL = math.radians(315.0), math.radians(45.0)
L = (math.cos(EL) * math.cos(AZ), math.cos(EL) * math.sin(AZ), math.sin(EL))


def decode(path, mip):
    d = DDS(path)
    m = min(mip, d.mips - 1)
    return d.decode(m)


def normals(path, mip, up_is_green):
    """(east, north, up) per texel, from the stored channels."""
    w, h, px = decode(path, mip)
    out = []
    for i in range(w * h):
        r, g, b = px[i*4] / 255.0*2-1, px[i*4+1] / 255.0*2-1, px[i*4+2] / 255.0*2-1
        out.append((r, g, b) if up_is_green else (r, b, g))   # -> (east, up, north)
    return w, h, out


def shade(nrm):
    e, u, n = nrm                      # east, up, north
    d = e*L[0] + n*L[1] + u*L[2]
    v = 0.25 + 0.75 * max(0.0, d)
    c = max(0, min(255, int(v * 255)))
    return (c, c, c)


def raw_rgb(path, mip):
    w, h, px = decode(path, mip)
    return w, h, [(px[i*4], px[i*4+1], px[i*4+2]) for i in range(w*h)]


def png(path, w, h, rgb):
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            raw += bytes(rgb[y*w + x])
    def ch(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t+d) & 0xFFFFFFFF)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n'
        + ch(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
        + ch(b'IDAT', zlib.compress(bytes(raw), 6)) + ch(b'IEND', b''))


def build(tile, mip, out, crop=None):
    vp = os.path.join(VAN, '%s_msn.DDS' % tile)
    op = os.path.join(OURS, '%s_msn.DDS' % tile)
    w, h, vN = normals(vp, mip, True)          # vanilla: up in green
    w2, h2, oN = normals(op, mip, True)        # ours now: up in green
    assert (w, h) == (w2, h2), (w, h, w2, h2)
    bN = [(e, n, u) for (e, u, n) in oN]       # ours before: green and blue swapped
    _, _, vRaw = raw_rgb(vp, mip)
    _, _, oRaw = raw_rgb(op, mip)
    bRaw = [(r, b, g) for (r, g, b) in oRaw]

    x0, y0, cw, chh = crop if crop else (0, 0, w, h)
    def sub(src, conv):
        return [conv(src[(y0+y)*w + x0+x]) for y in range(chh) for x in range(cw)]

    cols = [sub(vRaw, lambda c: c), sub(oRaw, lambda c: c),
            sub(vN, shade), sub(oN, shade)]
    G = 6
    W = cw*2 + G*3
    H = chh*2 + G*3
    img = [(24, 24, 26)] * (W * H)
    for k, col in enumerate(cols):
        px_, py_ = G + (k % 2)*(cw+G), G + (k // 2)*(chh+G)
        for y in range(chh):
            for x in range(cw):
                img[(py_+y)*W + px_+x] = col[y*cw + x]
    png(out, W, H, img)
    return W, H


if __name__ == '__main__':
    tile = sys.argv[1] if len(sys.argv) > 1 else 'Commonwealth.4.-12.44'
    print(build(tile, 1, 'msn_wide.png'))
    print(build(tile, 0, 'msn_peak.png', crop=(150, 40, 200, 150)))
    print('msn_wide.png and msn_peak.png written for', tile)
