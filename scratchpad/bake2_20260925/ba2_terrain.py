"""BAKE2, director's ruling 2026-09-25 (fill ON for Far Harbor and Nuka-World): list, and with --extract write,
ONLY the vanilla terrain LOD colour textures the fill reads -- Textures\\Terrain\\<WS>\\<WS>.4.<x>.<y>.dds -- from a
DX10 BA2 in his game Data, READ ONLY, into the input cache E:\\Tools\\Fallout 4\\DataUnpacked\\Data (never the mod,
never git, never the game's Data). Reader after skill fo4-ba2-texture-miss s3 (records before the name table, zlib
chunks, a 148-byte DX10 DDS header rebuilt). Writes a manifest line per file: archive, path, bytes, sha1.
usage: python ba2_terrain.py <archive.ba2> <world folder name> [--all-dims] [--extract <manifest>]"""
import sys, struct, zlib, hashlib, os, re

OUT = 'E:/Tools/Fallout 4/DataUnpacked/Data/'

def records(path):
    f = open(path, 'rb')
    magic, ver, kind, n, nto = struct.unpack('<4sI4sIQ', f.read(24))
    assert magic == b'BTDX' and kind == b'DX10', (magic, kind)
    if ver in (2, 3): f.read(8 if ver == 2 else 12)
    recs = []
    for _ in range(n):
        r = f.read(24)
        numChunks = r[13]; h, w = struct.unpack_from('<HH', r, 16); mips, fmt = r[20], r[21]
        ch = [struct.unpack('<QIIHHI', f.read(24)) for _ in range(numChunks)]
        recs.append(dict(h=h, w=w, mips=mips, fmt=fmt, cube=r[22], chunks=ch))
    f.seek(nto)
    for rc in recs:
        ln = struct.unpack('<H', f.read(2))[0]; rc['name'] = f.read(ln).decode('latin-1')
    return f, ver, recs

def dds(f, rc):
    body = b''
    for off, packed, unpacked, m0, m1, _ in rc['chunks']:
        f.seek(off); d = f.read(packed or unpacked)
        if packed:
            d = zlib.decompress(d); assert len(d) == unpacked, 'inflate size'
        body += d
    # DDS_HEADER (124) + DX10 (20); linear size for BC formats
    bc = rc['fmt'] in (70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 94, 95, 96, 97, 98, 99)
    bpb = 8 if rc['fmt'] in (70, 71, 72, 79, 80, 81) else 16
    lin = max(1, (rc['w'] + 3) // 4) * max(1, (rc['h'] + 3) // 4) * bpb if bc else rc['w'] * rc['h'] * 4
    flags = 0x1 | 0x2 | 0x4 | 0x1000 | 0x20000 | (0x80000 if bc else 0x8)
    hdr = struct.pack('<4s7I44x', b'DDS ', 124, flags, rc['h'], rc['w'], lin, 0, rc['mips'])
    hdr += struct.pack('<II4s20x', 32, 0x4, b'DX10')
    hdr += struct.pack('<5I', 0x1000 | (0x400000 if rc['mips'] > 1 else 0) | 0x8, 0, 0, 0, 0)
    hdr += struct.pack('<IIIII', rc['fmt'], 3, 0, 1, 0)
    assert len(hdr) == 148, len(hdr)
    return hdr + body

def main():
    arc, world = sys.argv[1], sys.argv[2]
    alld = '--all-dims' in sys.argv
    man = sys.argv[sys.argv.index('--extract') + 1] if '--extract' in sys.argv else None
    f, ver, recs = records(arc)
    pre = ('textures\\terrain\\%s\\' % world).lower()
    pat = re.compile(re.escape(pre) + re.escape(world.lower()) + r'\.(\d+)\.(-?\d+)\.(-?\d+)\.dds$')
    hits = [r for r in recs if r['name'].lower().startswith(pre)]
    kinds = {}
    for r in hits:
        m = pat.match(r['name'].lower())
        k = ('dim%s' % m.group(1)) if m else re.sub(r'[-\d]+', '#', r['name'].lower()[len(pre):])
        kinds[k] = kinds.get(k, 0) + 1
    print('%s v%d: %d records, %d under %s; kinds %s' % (os.path.basename(arc), ver, len(recs), len(hits), pre, kinds))
    fmts = {}
    for r in hits:
        m = pat.match(r['name'].lower())
        if m and m.group(1) == '4': fmts[(r['w'], r['h'], r['fmt'], r['mips'])] = fmts.get((r['w'], r['h'], r['fmt'], r['mips']), 0) + 1
    print('dim-4 colour tiles (w, h, dxgi, mips): %s' % fmts)
    if not man:
        return
    n = 0; tot = 0
    with open(man, 'a', encoding='utf-8') as mf:
        for r in hits:
            m = pat.match(r['name'].lower())
            if not m or (m.group(1) != '4' and not alld) or r['cube']:
                continue
            data = dds(f, r)
            rel = r['name'].replace('\\', '/')
            dst = OUT + rel
            assert os.path.abspath(dst).lower().startswith(os.path.abspath(OUT).lower())
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            if os.path.exists(dst):
                assert open(dst, 'rb').read() == data, ('exists and differs', dst)
            else:
                open(dst, 'wb').write(data)
            mf.write('%s\t%s\t%d\t%s\n' % (os.path.basename(arc), rel, len(data), hashlib.sha1(data).hexdigest()))
            n += 1; tot += len(data)
    print('extracted %d files, %d bytes, into %s' % (n, tot, OUT))

if __name__ == '__main__':
    main()
