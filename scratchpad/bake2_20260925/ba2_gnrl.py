"""BAKE2: read one file out of a GNRL BA2 (read only), print its bytes as hex. usage: ba2_gnrl.py <ba2> <substring>"""
import sys, struct, zlib
f = open(sys.argv[1], 'rb'); magic, ver, kind, n, nto = struct.unpack('<4sI4sIQ', f.read(24))
assert magic == b'BTDX' and kind == b'GNRL', kind
if ver in (2, 3): f.read(8 if ver == 2 else 12)
recs = [struct.unpack('<I4sIIQIII', f.read(36)) for _ in range(n)]
f.seek(nto); names = []
for _ in range(n):
    ln = struct.unpack('<H', f.read(2))[0]; names.append(f.read(ln).decode('latin-1'))
for nm, r in zip(names, recs):
    if sys.argv[2].lower() in nm.lower():
        f.seek(r[4]); d = f.read(r[5] or r[6]); d = zlib.decompress(d) if r[5] else d
        print(nm, len(d), d.hex(' '))
# --extract <manifest>: write each match under the input cache E:/Tools/Fallout 4/DataUnpacked/Data (never the mod)
if '--extract' in sys.argv:
    import os, hashlib
    OUT = 'E:/Tools/Fallout 4/DataUnpacked/Data/'; man = sys.argv[sys.argv.index('--extract') + 1]
    for nm, r in zip(names, recs):
        if sys.argv[2].lower() in nm.lower():
            f.seek(r[4]); d = f.read(r[5] or r[6]); d = zlib.decompress(d) if r[5] else d
            dst = OUT + nm.replace(chr(92), '/'); os.makedirs(os.path.dirname(dst), exist_ok=True)
            if os.path.exists(dst): assert open(dst, 'rb').read() == d, dst
            else: open(dst, 'wb').write(d)
            open(man, 'a').write('%s\t%s\t%d\t%s\n' % (os.path.basename(sys.argv[1]), nm.replace(chr(92), '/'), len(d), hashlib.sha1(d).hexdigest()))
            print('wrote', dst)
