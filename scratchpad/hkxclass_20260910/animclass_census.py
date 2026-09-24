"""Tally every hka*Animation class in every .hkx of Fallout4 - Animations.ba2.

Two independent tallies per file:
  OBJECTS -- class of every object listed in the packfile's virtual-fixup table
  NAMED   -- every string in __classnames__ (a class can be NAMED without an
             instance; that is the refuter for "nothing but spline")
The packfile walker is HKX1's (section headers at 0x40 + u16@0x3e).
"""
import struct, sys, zlib, collections, re

ARCHIVE = sys.argv[1]
OUT = sys.argv[2] if len(sys.argv) > 2 else None
RX = re.compile(r'^hka.*Animation$')

def packfile(blob):
    if struct.unpack_from('<II', blob, 0) != (0x57E0E057, 0x10C0C010):
        return None
    numSections, = struct.unpack_from('<i', blob, 20)
    sechdr = 0x40 + struct.unpack_from('<H', blob, 0x3e)[0]
    secs = []
    for s in range(numSections):
        off = sechdr + s * 0x40
        tag = blob[off:off + 19].split(b'\0')[0].decode('latin-1')
        vals = struct.unpack_from('<7i', blob, off + 20)
        secs.append((tag,) + vals)
    cn = [s for s in secs if s[0] == '__classnames__'][0]
    dt = [s for s in secs if s[0] == '__data__'][0]
    cnames = {}
    p = cn[1]; endp = cn[1] + cn[2]
    while p + 5 < endp:
        if blob[p + 4] != 0x09:
            break
        e = blob.index(b'\0', p + 5)
        cnames[p + 5 - cn[1]] = blob[p + 5:e].decode('latin-1')
        p = e + 1
    base = dt[1]
    objs = []
    for p in range(base + dt[4], min(base + dt[5], len(blob) - 11), 12):
        src, sec, cno = struct.unpack_from('<iii', blob, p)
        if src != -1:
            objs.append(cnames.get(cno, '?'))
    return cnames, objs

objs_t = collections.Counter()
named_t = collections.Counter()
files_with = collections.defaultdict(list)
n_hkx = n_pf = n_bad = 0
with open(ARCHIVE, 'rb') as f:
    magic, version, kind, numFiles, nameTableOffset = struct.unpack('<4sII I Q', f.read(24))
    recs = [struct.unpack('<IIIIQIII', f.read(36)) for _ in range(numFiles)]
    f.seek(nameTableOffset)
    names = []
    for _ in range(numFiles):
        n = struct.unpack('<H', f.read(2))[0]
        names.append(f.read(n).decode('latin-1'))
    for r, nm in zip(recs, names):
        if not nm.lower().endswith('.hkx'):
            continue
        n_hkx += 1
        _, _, _, _, offset, packed, unpacked, _ = r
        f.seek(offset)
        data = f.read(packed if packed else unpacked)
        if packed:
            try: data = zlib.decompress(data)
            except zlib.error: n_bad += 1; continue
        try: pf = packfile(data)
        except Exception: n_bad += 1; continue
        if pf is None: n_bad += 1; continue
        n_pf += 1
        cnames, objs = pf
        for c in set(v for v in cnames.values() if RX.match(v)):
            named_t[c] += 1
        seen = set()
        for c in objs:
            if RX.match(c):
                objs_t[c] += 1
                if c not in seen and len(files_with[c]) < 5:
                    files_with[c].append(nm)
                seen.add(c)

print("hkx files scanned      : %d" % n_hkx)
print("parsed as packfiles    : %d" % n_pf)
print("refused / not packfile : %d" % n_bad)
print("\n-- OBJECT instances by class (hka*Animation) --")
for c, n in objs_t.most_common():
    print("  %-42s %6d   e.g. %s" % (c, n, files_with[c][0] if files_with[c] else ''))
print("\n-- NAMED in __classnames__ (files) --")
for c, n in named_t.most_common():
    print("  %-42s %6d" % (c, n))
if OUT:
    with open(OUT, 'w') as fh:
        for c, n in objs_t.most_common(): fh.write("OBJ\t%s\t%d\n" % (c, n))
        for c, n in named_t.most_common(): fh.write("NAMED\t%s\t%d\n" % (c, n))
