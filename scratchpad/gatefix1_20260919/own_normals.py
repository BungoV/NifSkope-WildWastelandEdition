"""What the OWN normal sheet in today's container actually says, and what
own-minus-flat luma signal it can produce -- the discriminator for gate (b)'s
two remaining reds.  Read-only.  Lane GATEFIX1, 2026-09-19."""
import glob
import math
import struct
import sys

import numpy as np

CACHE = r"E:/Projects/NifskopeWildWastelandEdition/scratchpad/nativeview1_20260912/work/sheetcache/Textures/LODLSheets"

# the checker's own light, verbatim (native_lighting_check.py)
rx, rz = math.radians(-63.5593), math.radians(133.3081)
sx, cx, sz, cz = math.sin(rx), math.cos(rx), math.sin(rz), math.cos(rz)
Lw = np.array([sx * sz, sx * cz, cx])          # world (east, north, up)
FLAT = np.array([0.0353, 0.0353, 0.9988])      # the flat fixture, read back
NL_FLAT = float(FLAT @ Lw)


def bc1_rgb(buf, w, h):
    """Decode a BC1 mip 0 to an (h, w, 3) uint8 array."""
    bw, bh = (w + 3) // 4, (h + 3) // 4
    out = np.zeros((h, w, 3), np.uint8)
    for by in range(bh):
        for bx in range(bw):
            o = (by * bw + bx) * 8
            c0, c1, bits = struct.unpack_from("<HHI", buf, o)
            def rgb(c):
                return (((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63,
                        (c & 31) * 255 // 31)
            p = [rgb(c0), rgb(c1)]
            if c0 > c1:
                p.append(tuple((2 * p[0][k] + p[1][k]) // 3 for k in range(3)))
                p.append(tuple((p[0][k] + 2 * p[1][k]) // 3 for k in range(3)))
            else:
                p.append(tuple((p[0][k] + p[1][k]) // 2 for k in range(3)))
                p.append((0, 0, 0))
            for t in range(16):
                y, x = by * 4 + t // 4, bx * 4 + t % 4
                if y < h and x < w:
                    out[y, x] = p[(bits >> (2 * t)) & 3]
    return out


allN = []
for p in sorted(glob.glob(CACHE + "/*.n.DDS")):
    b = open(p, "rb").read()
    hdr = struct.unpack_from("<7I", b, 4)
    h, w = hdr[2], hdr[3]
    off = 148 if b[84:88] == b"DX10" else 128
    a = bc1_rgb(b[off:], w, h).astype(np.float64) / 255.0 * 2.0 - 1.0
    # the sheet's channel order is R east, G up, B north (NATIVEVIEW2,
    # correlation against real cell heights); the light Lw is world
    # (east, north, up) -- so reorder, do NOT dot them as stored.
    rgb = np.stack([a[..., 0], a[..., 2], a[..., 1]], axis=2)
    n = rgb / np.maximum(1e-9, np.linalg.norm(rgb, axis=2, keepdims=True))
    allN.append(n.reshape(-1, 3))
    nl = n @ Lw
    print("%-34s %dx%d  up mean %.4f sd %.4f | east sd %.4f north sd %.4f | "
          "N.L mean %.4f sd %.4f" % (p.split("/")[-1], w, h,
                                     n[..., 2].mean(), n[..., 2].std(),
                                     n[..., 0].std(), n[..., 1].std(),
                                     nl.mean(), nl.std()))

N = np.concatenate(allN)
nl = N @ Lw
print()
print("the FLAT fixture's N.L                 %.4f" % NL_FLAT)
print("the OWN sheet's N.L over %d texels  mean %.4f  sd %.4f" % (len(nl), nl.mean(), nl.std()))
print("own MINUS flat, in N.L                 mean %+.4f  sd %.4f  |mean| %.4f"
      % ((nl - NL_FLAT).mean(), (nl - NL_FLAT).std(), np.abs(nl - NL_FLAT).mean()))
print()
print("what that is worth in 8-bit luma, if luma tracked N.L linearly over 0..255:")
print("  sd  %.2f levels;  mean|d|  %.2f levels"
      % (255.0 * (nl - NL_FLAT).std(), 255.0 * np.abs(nl - NL_FLAT).mean()))
print("gate (b) measures blockSD of own-minus-flat AFTER 32x32 block averaging;")
print("this run read 2.28 against a floor of 3.50 measured 2026-09-16 11:54:47.")
