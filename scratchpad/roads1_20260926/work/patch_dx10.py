s = open('nlc.py', encoding='utf-8').read()
old_idx = """            if magic != b'BTDX' or kind.to_bytes(4, 'little') != b'GNRL':
                return
"""
new_idx = """            if magic != b'BTDX':
                return
            if kind.to_bytes(4, 'little') == b'DX10':        # ROADS1: texture archives too
                recs = []
                for _ in range(n):
                    h = struct.unpack('<IIIBBHHHBBBB', f.read(24))
                    ch = [struct.unpack('<QIIHHI', f.read(24)) for _ in range(h[4])]
                    recs.append((h, ch))
                f.seek(nto)
                for h, ch in recs:
                    ln = struct.unpack('<H', f.read(2))[0]
                    name = f.read(ln).decode('latin-1').lower().replace('/', '\\')
                    self.arch[name] = ('DX10', path, h, ch)
                return
            if kind.to_bytes(4, 'little') != b'GNRL':
                return
"""
assert s.count(old_idx) == 1
s = s.replace(old_idx, new_idx)
old_rd = """        path, off, packed, unpacked = hit
"""
new_rd = """        if hit[0] == 'DX10':
            return dx10_dds(*hit[1:])
        path, off, packed, unpacked = hit
"""
assert s.count(old_rd) == 1
s = s.replace(old_rd, new_rd)
old_fn = "# ------------------------------------------------------------------------------------------ ESM\n"
new_fn = '''def dx10_dds(path, h, chunks):
    """ROADS1: a DX10 BA2 entry rebuilt as a .dds (DX10 extended header), every chunk inflated in order."""
    _nh, _ext, _dh, _u, _nc, _chs, height, width, mips, fmt, cube, _tile = h
    body = b''
    with open(path, 'rb') as f:
        for off, packed, unpacked, _m0, _m1, _al in chunks:
            f.seek(off)
            b = f.read(packed if packed else unpacked)
            body += zlib.decompress(b) if packed else b
    ddsd = 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000 | 0x80000
    hdr = struct.pack('<4sIIIIIII', b'DDS ', 124, ddsd, height, width, len(body), 0, mips)
    hdr += b'\0' * 44
    hdr += struct.pack('<II4sIIIII', 32, 0x4, b'DX10', 0, 0, 0, 0, 0)
    hdr += struct.pack('<IIIII', 0x1000 | 0x400000 | 0x8, 0, 0, 0, 0)
    hdr += struct.pack('<IIIII', fmt, 3, 4 if cube else 0, 1, 0)
    return hdr + body


# ------------------------------------------------------------------------------------------ ESM
'''
assert s.count(old_fn) == 1
s = s.replace(old_fn, new_fn)
open('nlc.py', 'w', encoding='utf-8', newline='').write(s)
print('patched')
