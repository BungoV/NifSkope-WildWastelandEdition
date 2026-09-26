"""Lane NEAR2 (2026-09-26): a resource stack that ALSO opens DX10 (texture) BA2s, for the near-library texture gate.

Independent of NifSkope: loose folders and BA2 archives (GNRL and DX10), read in Python. The stack order is the
caller's (lodgen_loadorder_check.expected: Data first, then the enabled MO2 mods bottom-up, then overwrite);
LAST entry wins, a loose file beats every archive -- the same rule near_library_check.Stack uses for GNRL.

A DX10 record is rebuilt into a DDS (148-byte DX10 header + the chunks in order), per the fo4-ba2-texture-miss
skill section 3. Only what the gate needs is decoded: the header facts, and BC1/BC3/BC7/BGRA pixels for colour
statistics.
"""
import os
import struct
import zlib

DDS_MAGIC = b'DDS '


class TexStack:
    def __init__(self, entries, label='', plugins=None):
        """`plugins`: the active plugins in load order. Inside ONE entry an archive "<stem> - *.ba2" takes its
        plugin's index (the fo4-ba2-texture-miss skill section 4), so DLCUltraHighResolution beats
        "Fallout4 - Textures1"; archives no active plugin names (the ini's own "Fallout4 - *" list) go first.
        Without `plugins` the order inside an entry is by name."""
        self.label = label
        self.entries = list(entries)
        self.loose = list(reversed(self.entries))      # first hit wins in this order
        self.arch = {}                                  # lowercase backslash name -> record tuple
        rank = {}
        for i, p in enumerate(plugins or []):
            rank[os.path.splitext(os.path.basename(p.replace(chr(92), '/')))[0].lower()] = i
        for e in self.entries:                          # later entries overwrite earlier
            if not os.path.isdir(e):
                continue
            names = [n for n in os.listdir(e) if n.lower().endswith('.ba2')]
            names.sort(key=lambda n: (rank.get(n.lower()[:-4].split(' - ')[0], -1), n.lower()))
            for n in names:
                self._index(os.path.join(e, n))

    def _index(self, path):
        with open(path, 'rb') as f:
            head = f.read(24)
            if len(head) < 24:
                return
            magic, _ver, kind, n, nto = struct.unpack('<4sI4sIQ', head)
            if magic != b'BTDX':
                return
            if kind == b'GNRL':
                recs = [('G',) + struct.unpack('<IIIIQIII', f.read(36)) for _ in range(n)]
            elif kind == b'DX10':
                recs = []
                for _ in range(n):
                    r = f.read(24)
                    _nh, _ext, _dh, _u8, nchunks, _chl, h, w, mips, dxgi, cube = struct.unpack('<I4sIBBHHHBBH', r)
                    chunks = [struct.unpack('<QIIHHI', f.read(24)) for _ in range(nchunks)]
                    recs.append(('D', h, w, mips, dxgi, cube, chunks))
            else:
                return
            f.seek(nto)
            for r in recs:
                ln = struct.unpack('<H', f.read(2))[0]
                name = f.read(ln).decode('latin-1').lower().replace('/', '\\')
                self.arch[name] = (path, r)

    @staticmethod
    def canon(rel, prefix):
        """lower case, back slashes, `<prefix>\\` in front exactly once (the game's own resolution)."""
        p = rel.replace('/', '\\').lower().lstrip('\\')
        if not p.startswith(prefix + '\\'):
            p = prefix + '\\' + p
        return p

    def where(self, rel):
        """'loose:<dir>' / 'ba2:<archive>' / None -- no bytes read."""
        rel = rel.replace('/', '\\').lower()
        for e in self.loose:
            p = os.path.join(e, rel)
            if os.path.isfile(p) and os.path.getsize(p) > 0:
                return 'loose:' + e
        hit = self.arch.get(rel)
        return ('ba2:' + os.path.basename(hit[0])) if hit else None

    def read(self, rel):
        rel = rel.replace('/', '\\').lower()
        for e in self.loose:
            p = os.path.join(e, rel)
            if os.path.isfile(p):
                with open(p, 'rb') as f:
                    b = f.read()
                if b:
                    return b
        hit = self.arch.get(rel)
        if not hit:
            return None
        path, r = hit
        with open(path, 'rb') as f:
            if r[0] == 'G':
                _g, _nh, _ext, _dh, _fl, off, packed, unpacked, _bad = r
                f.seek(off)
                b = f.read(packed if packed else unpacked)
                return zlib.decompress(b) if packed else b
            _d, h, w, mips, dxgi, cube, chunks = r
            body = b''
            for off, packed, unpacked, _m0, _m1, _baad in chunks:
                f.seek(off)
                c = f.read(packed if packed else unpacked)
                body += zlib.decompress(c) if packed else c
            return dds_header(w, h, mips, dxgi, cube & 1) + body

    def header(self, rel):
        """(dxgi, w, h, mips, cube) of a texture, from the archive record or a loose DDS header; None = absent."""
        rel = rel.replace('/', '\\').lower()
        for e in self.loose:
            p = os.path.join(e, rel)
            if os.path.isfile(p) and os.path.getsize(p) > 0:
                with open(p, 'rb') as f:
                    return dds_facts(f.read(148))
        hit = self.arch.get(rel)
        if not hit:
            return None
        r = hit[1]
        if r[0] != 'D':
            return None
        return (r[4], r[2], r[1], r[3], bool(r[5] & 1))


def dds_header(w, h, mips, dxgi, cube):
    flags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000
    caps = 0x1000 | 0x8 | 0x400000
    caps2 = 0xFE00 if cube else 0
    hdr = struct.pack('<7I', 124, flags, h, w, 0, 0, mips)
    hdr += b'\0' * 44
    hdr += struct.pack('<2I4s5I', 32, 0x4, b'DX10', 0, 0, 0, 0, 0)
    hdr += struct.pack('<5I', caps, caps2, 0, 0, 0)
    dx10 = struct.pack('<5I', dxgi, 3, 0x4 if cube else 0, 1, 0)
    return DDS_MAGIC + hdr + dx10


# legacy FourCC -> DXGI, for loose DDS files without a DX10 header
FOURCC_DXGI = {b'DXT1': 71, b'DXT3': 74, b'DXT5': 77, b'ATI1': 80, b'BC4U': 80, b'ATI2': 83, b'BC5U': 83}


def dds_facts(b):
    """(dxgi, w, h, mips, cube) of a DDS header, or None."""
    if len(b) < 128 or b[:4] != DDS_MAGIC:
        return None
    h, w = struct.unpack_from('<II', b, 12)
    mips = max(1, struct.unpack_from('<I', b, 28)[0])
    pf_flags = struct.unpack_from('<I', b, 80)[0]
    fourcc = b[84:88]
    caps2 = struct.unpack_from('<I', b, 112)[0]
    cube = bool(caps2 & 0x200)
    if fourcc == b'DX10' and len(b) >= 148:
        return (struct.unpack_from('<I', b, 128)[0], w, h, mips, cube)
    if pf_flags & 0x4:
        return (FOURCC_DXGI.get(fourcc, 0), w, h, mips, cube)
    bits = struct.unpack_from('<I', b, 88)[0]
    return (87 if bits == 32 else 0, w, h, mips, cube)


def mean_rgb(dds, max_side=256):
    """The mean RGB (0..1) of the smallest mip whose side is <= max_side (BC1/BC3/BC7/BGRA); None = not decoded."""
    import numpy as np
    f = dds_facts(dds)
    if not f:
        return None
    dxgi, w, h, mips, cube = f
    off = 148 if dds[84:88] == b'DX10' else 128
    bpb = {71: 8, 72: 8, 77: 16, 78: 16, 98: 16, 99: 16}.get(dxgi)
    level = 0
    lw, lh = w, h
    while level + 1 < mips and max(lw, lh) > max_side:
        if bpb:
            off += max(1, (lw + 3) // 4) * max(1, (lh + 3) // 4) * bpb
        elif dxgi in (87, 28, 91):
            off += lw * lh * 4
        else:
            return None
        lw, lh = max(1, lw // 2), max(1, lh // 2)
        level += 1
    body = dds[off:]
    if dxgi in (87, 28, 91):
        a = np.frombuffer(body[:lw * lh * 4], np.uint8).reshape(-1, 4).astype(np.float64) / 255.0
        return tuple(a[:, [2, 1, 0]].mean(0)) if dxgi == 87 else tuple(a[:, :3].mean(0))
    import impostor_bc_decode as bc
    bw, bh = max(1, (lw + 3) // 4), max(1, (lh + 3) // 4)
    if dxgi in (71, 72):
        d = np.frombuffer(body[:bw * bh * 8], np.uint8).reshape(-1, 8)
        return tuple(bc._bc1_colour(d).reshape(-1, 3).mean(0))
    if dxgi in (77, 78):
        d = np.frombuffer(body[:bw * bh * 16], np.uint8).reshape(-1, 16)
        return tuple(bc._bc1_colour(d[:, 8:]).reshape(-1, 3).mean(0))
    return None
