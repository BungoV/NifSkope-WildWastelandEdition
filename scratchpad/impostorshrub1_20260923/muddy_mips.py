"""IMPOSTORSHRUB1 muddy, stages 2+3 (mip chain and baked AO), offline, from the
SHIPPED card DDS (decoded by tests/spells/impostor_bc_decode.py's block
decoders, never through the writer): per mip, the colour of the texels a card
draws (coverage over the shader's test) and the baked AO the shader multiplies
into the ambient (_gsaos B).

  python muddy_mips.py CARDS_DIR [coverage-test 0..1]"""
import struct, sys
import numpy as np
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
import impostor_bc_decode as D

def mips(path):
    b = open(path, 'rb').read()
    h = struct.unpack('<31I', b[4:128])
    ht, wd, nm, fourcc = h[2], h[3], max(1, h[6]), b[84:88]
    o, out = 128, []
    for k in range(nm):
        w, hh = max(1, wd >> k), max(1, ht >> k)
        bw, bh = (w + 3) // 4, (hh + 3) // 4
        if fourcc == b'DXT5':
            d = np.frombuffer(b[o:o + bw * bh * 16], np.uint8).reshape(-1, 16); o += bw * bh * 16
            img = np.concatenate([D._bc1_colour(d[:, 8:]), D._bc3_alpha(d[:, :8])[..., None]], -1)
        elif fourcc == b'DXT1':
            d = np.frombuffer(b[o:o + bw * bh * 8], np.uint8).reshape(-1, 8); o += bw * bh * 8
            C = D._bc1_colour(d); img = np.concatenate([C, np.ones(C.shape[:3] + (1,))], -1)
        else:
            raise RuntimeError(fourcc)
        img = img.reshape(bh, bw, 4, 4, 4).transpose(0, 2, 1, 3, 4).reshape(bh * 4, bw * 4, 4)[:hh, :w]
        out.append(img)
    return fourcc, out

cards = sys.argv[1]
test = float(sys.argv[2]) if len(sys.argv) > 2 else 0.5
L = np.array([0.2126, 0.7152, 0.0722])
fd, dm = mips(cards + '/000531b3_oct_d.DDS')
fg, gm = mips(cards + '/000531b3_oct_gsaos.DDS')
print('diffuse', fd, len(dm), 'mips; gsaos', fg, len(gm), 'mips; coverage test', test)
for k, img in enumerate(dm):
    a = img[..., 3]
    m = a >= test
    rgb = img[..., :3][m] * 255
    line = '  mip %d %4dx%-4d drawn %7d  RGB %6.1f %6.1f %6.1f  luma %6.1f' % (
        k, img.shape[1], img.shape[0], m.sum(), *(rgb.mean(0) if m.any() else (0, 0, 0)),
        (rgb.mean(0) * L).sum() if m.any() else 0)
    if k < len(gm) and gm[k].shape[:2] == img.shape[:2]:
        ao = gm[k][..., 2][m]
        line += '  AO mean %.3f  p10 %.3f  (<0.5: %.1f%%)' % (ao.mean(), np.percentile(ao, 10), 100 * (ao < 0.5).mean())
    print(line)
