"""w4_repin_selftest helper: mutate the library .lodo of w4/<dir> in place. usage: w4_mutate.py <dir> other|third"""
import sys, struct, zlib
d, m = sys.argv[1], sys.argv[2]
for r in ('boston', 'sanc'):
    p = 'w4/%s/%s/Native/FO4CSLOD/Commonwealth/Commonwealth.lodo' % (d, r)
    b = bytearray(open(p, 'rb').read()); ov = struct.unpack_from('<Q', b, 0x98)[0]
    o = ov + (270125 if m == 'other' else 270124) * 16 + 15
    b[o] = ((b[o] + 1) & 255) if m == 'other' else 150
    # a VALID file with that byte (the decoder refuses a stale CRC before the gate ever compares): both CRCs redone
    offs = struct.unpack_from('<QQQQQQQ', b, 0x70); bc, mc, cc, matc, vc = struct.unpack_from('<IIIII', b, 0x50)
    sb = struct.unpack_from('<I', b, 0x6C)[0]; ol = struct.unpack_from('<Q', b, 0xC0)[0]
    ccnt, coff = struct.unpack_from('<IQ', b, 0xD4)
    crc = 0
    for off, size in ((offs[0], bc * 32), (offs[1], mc * 56), (offs[2], cc * 16), (ol, cc * 48), (offs[3], matc * 16),
                      (offs[4], cc * 48), (offs[5], vc * 16), (offs[6], sb)) + (((coff, ccnt * 4),) if ccnt else ()):
        crc = zlib.crc32(bytes(b[off:off + size]), crc)
    struct.pack_into('<I', b, 0xA8, crc & 0xFFFFFFFF)
    struct.pack_into('<I', b, 0x0C, zlib.crc32(bytes(b[0x10:0x100])) & 0xFFFFFFFF)
    open(p, 'wb').write(b)
    print(p, hex(o), 'now', b[o])
