import struct, sys

path = sys.argv[1]
data = open(path, 'rb').read()

nl = data.index(b'\x0a'); pos = nl+1
ver, = struct.unpack_from('<I', data, pos); pos += 4
pos += 1  # endian
uver, = struct.unpack_from('<I', data, pos); pos += 4
nb, = struct.unpack_from('<I', data, pos); pos += 4
bsver, = struct.unpack_from('<I', data, pos); pos += 4
nexp = 4 if bsver == 130 else 3
for i in range(nexp):
    slen = data[pos]; pos += 1+slen
ntypes = struct.unpack_from('<H', data, pos)[0]; pos += 2
btypes = []
for i in range(ntypes):
    slen = struct.unpack_from('<I', data, pos)[0]; pos += 4
    btypes.append(data[pos:pos+slen].decode()); pos += slen
tidx  = list(struct.unpack_from(f'<{nb}H', data, pos)); pos += nb*2
bsize = list(struct.unpack_from(f'<{nb}I', data, pos)); pos += nb*4
nstr  = struct.unpack_from('<I', data, pos)[0]; pos += 4
pos += 4  # max string len
strings = []
for i in range(nstr):
    slen = struct.unpack_from('<I', data, pos)[0]; pos += 4
    strings.append(data[pos:pos+slen].decode()); pos += slen
ngrp = struct.unpack_from('<I', data, pos)[0]; pos += 4+ngrp*4

print(f"{path}: ver={ver:#x} bsver={bsver} blocks={nb}")
off = pos
for i in range(nb):
    tname = btypes[tidx[i]]
    if tname in ('bhkPhysicsSystem', 'bhkRagdollSystem'):
        blen, = struct.unpack_from('<I', data, off)
        blob = data[off+4:off+4+blen]
        print(f"block {i}: {tname}, block size {bsize[i]}, blob len {blen}")
        # dump first 256 bytes
        for r in range(0, min(256, len(blob)), 16):
            hexs = ' '.join(f'{b:02x}' for b in blob[r:r+16])
            asc = ''.join(chr(b) if 32 <= b < 127 else '.' for b in blob[r:r+16])
            print(f"  {r:6x}  {hexs:<48}  {asc}")
        # search for class name strings anywhere in the blob
        names = []
        i2 = 0
        while i2 < len(blob):
            if blob[i2:i2+3] == b'hkp' or blob[i2:i2+2] == b'hk':
                end = blob.find(b'\x00', i2)
                if end > i2 and end - i2 < 64:
                    s = blob[i2:end]
                    if all(32 <= c < 127 for c in s) and len(s) > 3:
                        names.append((i2, s.decode()))
                        i2 = end
            i2 += 1
        seen = set()
        for o, n in names:
            if n not in seen:
                print(f"  class @{o:#x}: {n}")
                seen.add(n)
        out = path.rsplit('\\', 1)[-1].rsplit('/', 1)[-1] + f'.block{i}.hk.bin'
        open(sys.argv[2] + '/' + out, 'wb').write(blob)
        print(f"  saved -> {out}")
    off += bsize[i]
