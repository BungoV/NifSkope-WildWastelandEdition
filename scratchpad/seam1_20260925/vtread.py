"""mmap reader over a .lodt v2 VT container (layout = tests/spells/lodgen_vt_check.py Lodv, which reads the whole
file into memory and cannot open an 11 GB level). Colour/mask BC1/BC3 decoded with numpy.
mosaic(level, x0, y0, x1, y1): content texels of every tile covering cells x0..x1, y0..y1, row 0 = NORTH."""
import mmap, struct, sys, zlib
import numpy as np
sys.path.insert(0, r'E:/Projects/NifskopeWWE-seam1/tests/spells')
import lodgen_vt_check as vc

class Vt(vc.Lodv):
    def __init__(self, path):
        f = open(path, 'rb')
        self.b = mmap.mmap(f.fileno(), 0, access=mmap.ACCESS_READ)
        hdr = self.b[:0x100]
        # reuse the authority's header parse on the header bytes, then the table from the mmap
        b = self.b
        self.path = path
        (self.version, self.headerBytes, self.flags) = struct.unpack_from('<III', b, 4)
        (self.fileBytes, self.tableOffset, self.payloadOffset) = struct.unpack_from('<QQQ', b, 0x10)
        (self.south, self.west, self.north, self.east,
         self.wSouth, self.wWest, self.wNorth, self.wEast) = struct.unpack_from('<8h', b, 0x58)
        (self.levelDim, self.levelIndex, self.levelCount, self.tilesX, self.tilesY,
         self.content, self.border, self.stored) = struct.unpack_from('<8H', b, 0x68)
        (self.mips, self.sheetCount, self.aniso, self.compression) = struct.unpack_from('<4B', b, 0x78)
        self.tileCount = struct.unpack_from('<I', b, 0x7C)[0]
        self.sheets = []
        for i in range(10):
            f0, f1, role, space, skip = struct.unpack_from('<HHBBB', b, 0xA0 + i * 8)
            self.sheets.append({'dxgi': f0, 'dxgiCover': f1, 'role': role, 'space': space, 'skip': skip})
        t = np.frombuffer(b, np.uint8, self.tileCount * 24, self.tableOffset).reshape(-1, 24)
        self.tOff = t[:, 0:8].copy().view('<u8').ravel(); self.tStored = t[:, 8:12].copy().view('<u4').ravel()
        self.tFlags = t[:, 20:22].copy().view('<u2').ravel()
        self.table = None

    def payload(self, i):
        if not (self.tFlags[i] & 1): return None
        d = self.b[int(self.tOff[i]):int(self.tOff[i]) + int(self.tStored[i])]
        return zlib.decompress(d) if self.compression == 1 else d

    def sheet(self, i, role, mip=0):
        """decoded RGBA uint8 (stored x stored x 4) of the sheet with `role` (1 colour, 2 msn, 5 mask)"""
        p = self.payload(i)
        if p is None: return None
        cover = bool(self.tFlags[i] & 2)
        s = [k for k in range(self.sheetCount) if self.sheets[k]['role'] == role][0]
        o = self.sheetOffset(cover, s, mip)
        sd = self.sheets[s]; fmt = sd['dxgiCover'] if (cover and sd['dxgiCover'] != sd['dxgi']) else sd['dxgi']
        side = self.stored >> mip
        return decode(p, o, side, fmt)

    def index(self, cx0, cy0):
        tx = (cx0 - self.west) // self.levelDim
        ty = (self.north - (cy0 + self.levelDim - 1)) // self.levelDim
        return ty * self.tilesX + tx, tx, ty

    def mosaic(self, x0, y0, x1, y1, role=1):
        D = self.levelDim; C = self.content; B = self.border
        tx0 = (x0 - self.west) // D; tx1 = (x1 - self.west) // D
        ty0 = (self.north - y1) // D; ty1 = (self.north - y0) // D
        out = np.zeros(((ty1 - ty0 + 1) * C, (tx1 - tx0 + 1) * C, 4), np.uint8)
        for ty in range(ty0, ty1 + 1):
            for tx in range(tx0, tx1 + 1):
                s = self.sheet(ty * self.tilesX + tx, role)
                if s is None: continue
                out[(ty - ty0) * C:(ty - ty0 + 1) * C, (tx - tx0) * C:(tx - tx0 + 1) * C] = s[B:B + C, B:B + C]
        # world rect of the mosaic: west cell, north cell edge
        wW = self.west + tx0 * D; nN = self.north + 1 - ty0 * D
        return out, wW, nN          # texel (r,c) centre = world (wW*4096 + (c+.5)*upt, nN*4096 - (r+.5)*upt)

def _565(c):
    c = c.astype(np.int32)
    return np.stack([((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31], -1)

def decode(p, off, side, fmt):
    nb = (side // 4) ** 2
    bc3 = fmt in (77, 78)
    bb = 16 if bc3 else 8
    raw = np.frombuffer(p, np.uint8, nb * bb, off).reshape(nb, bb)
    col = raw[:, 8:16] if bc3 else raw
    c0 = col[:, 0:2].copy().view('<u2').ravel(); c1 = col[:, 2:4].copy().view('<u2').ravel()
    bits = col[:, 4:8].copy().view('<u4').ravel()
    e0, e1 = _565(c0), _565(c1)
    four = (c0 > c1) | bc3
    p2 = np.where(four[:, None], (2 * e0 + e1) // 3, (e0 + e1) // 2)
    p3 = np.where(four[:, None], (e0 + 2 * e1) // 3, 0)
    pal = np.stack([e0, e1, p2, p3], 1)                      # nb x 4 x 3
    idx = (bits[:, None] >> (2 * np.arange(16))) & 3          # nb x 16
    rgb = pal[np.arange(nb)[:, None], idx]                   # nb x 16 x 3
    a = np.full((nb, 16), 255, np.int32)
    if bc3:
        a0 = raw[:, 0].astype(np.int32); a1 = raw[:, 1].astype(np.int32)
        ab = raw[:, 2:8].copy(); ab = np.concatenate([ab, np.zeros((nb, 2), np.uint8)], 1).view('<u8').ravel()
        ai = (ab[:, None] >> (3 * np.arange(16)).astype(np.uint64)) & 7
        ai = ai.astype(np.int32)
        k = np.arange(8)
        pal8 = np.where(k[None] == 0, a0[:, None], np.where(k[None] == 1, a1[:, None],
               ((8 - k[None]) * a0[:, None] + (k[None] - 1) * a1[:, None]) // 7))
        pal6 = np.where(k[None] == 0, a0[:, None], np.where(k[None] == 1, a1[:, None],
               np.where(k[None] == 6, 0, np.where(k[None] == 7, 255, ((6 - k[None]) * a0[:, None] + (k[None] - 1) * a1[:, None]) // 5))))
        pa = np.where((a0 > a1)[:, None], pal8, pal6)
        a = pa[np.arange(nb)[:, None], ai]
    px = np.concatenate([rgb, a[..., None]], -1).astype(np.uint8)  # nb x 16 x 4
    bw = side // 4
    return px.reshape(bw, bw, 4, 4, 4).transpose(0, 2, 1, 3, 4).reshape(side, side, 4)

if __name__ == '__main__':
    v = Vt(sys.argv[1])
    print(v.levelDim, v.tilesX, v.tilesY, v.west, v.south, v.north, v.east, v.content, v.border, v.stored, v.sheets[:5])
    # self-check vs the authority's scalar decoder on one tile
    i = v.index(-20, 24)[0]
    p = v.payload(i); o = v.sheetOffset(bool(v.tFlags[i] & 2), 0, 0)
    ref = np.array(vc.decode_bc1(p, o, 64, 64), np.uint8)  # 64x64 corner is enough? decode_bc1 walks w x h from off
    mine = v.sheet(i, 1)
    print('bc1 self-check (first 64 rows of 16 blocks wide? no -- decode_bc1 takes w)', )
