"""List the FO4 animation files that carry hkaLosslessCompressedAnimation, and the
character clips whose first block uses THREECOMP48 rotations. Uses census.py's
packfile walker (imported by exec of its top half, so there is one walker)."""
import struct, zlib, sys, collections
sys.argv = ['x', '', '']
src = open(__file__.replace('find_clips.py', 'census.py')).read().split("tally = collections.Counter()")[0]
exec(src)
A = "X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4 - Animations.ba2"
with open(A, 'rb') as f:
    magic, version, kind, numFiles, nto = struct.unpack('<4sII I Q', f.read(24))
    recs = [struct.unpack('<IIIIQIII', f.read(36)) for _ in range(numFiles)]
    f.seek(nto); names = []
    for _ in range(numFiles):
        n = struct.unpack('<H', f.read(2))[0]; names.append(f.read(n).decode('latin-1'))
    lossless = []; hits = []
    for r, nm in zip(recs, names):
        if not nm.lower().endswith('.hkx'):
            continue
        f.seek(r[4]); d = f.read(r[5] if r[5] else r[6])
        if r[5]:
            d = zlib.decompress(d)
        if b'hkaLosslessCompressedAnimation' in d:
            lossless.append(nm)
        if 'Character\\Animations' not in nm:
            continue
        try:
            pf = packfile(d)
        except Exception:
            continue
        if not pf:
            continue
        cn, base, local, glob, objs = pf
        for ao, c in objs:
            if c != 'hkaSplineCompressedAnimation':
                continue
            nT, = struct.unpack_from('<i', d, ao + 0x18)
            dP, dN = arr(d, local, ao + 0x98); boP, boN = arr(d, local, ao + 0x58)
            bo, = struct.unpack_from('<I', d, boP)
            qs = set((d[dP + bo + t * 4] >> 2) & 0xF for t in range(nT))
            if 2 in qs:
                hits.append((r[6], nm, sorted(qs)))
print("lossless files:", len(lossless))
print(collections.Counter(n.split('\\')[2] if n.count('\\') > 2 else n for n in lossless).most_common(8))
print(lossless[:5])
hits.sort()
print("THREECOMP48 character clips:", len(hits))
for h in hits[:8]:
    print("  ", h)
