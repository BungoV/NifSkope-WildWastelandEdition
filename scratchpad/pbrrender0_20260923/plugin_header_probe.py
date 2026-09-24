"""Header verdict for a mod plugin: flags, masters, WTHR count, compressed count, override vs new."""
import struct, sys
P = sys.argv[1]
d = open(P, "rb").read()
typ, size, flags = struct.unpack_from("<4sII", d, 0)
p = 24; end = 24 + size; masters = []
while p < end:
    t, n = struct.unpack_from("<4sH", d, p); p += 6
    if t == b"MAST": masters.append(d[p:p+n].rstrip(bytes(1)).decode("latin1"))
    p += n
nm = len(masters)
pos = end; w = comp = over = new = 0
while pos < len(d):
    _, gsize, label = struct.unpack_from("<4sI4s", d, pos)
    if label == b"WTHR":
        q = pos + 24
        while q < pos + gsize:
            _, rs, rf, fid = struct.unpack_from("<4sIII", d, q)
            w += 1; comp += bool(rf & 0x40000)
            if (fid >> 24) < nm: over += 1
            else: new += 1
            q += 24 + rs
    pos += gsize
print(f"{P.split('/')[-1]}: flags=0x{flags:X} ESM={bool(flags&1)} ESL={bool(flags&0x200)} localized={bool(flags&0x80)} masters={masters} WTHR={w} compressed={comp} overrides={over} new={new}")
