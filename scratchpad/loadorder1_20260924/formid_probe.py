"""Read only: which records in each plugin carry a raw form ID libfo76utils refuses
(formID > 0x0FFFFFFF and not in FD/FE), and how many masters the plugin lists."""
import struct, sys, collections
for p in sys.argv[1:]:
    b = open(p, 'rb').read()
    tes4 = struct.unpack_from('<I', b, 4)[0]
    masters = []
    pos, end = 24, 24 + tes4
    while pos + 6 <= end:
        tag, ln = b[pos:pos + 4], struct.unpack_from('<H', b, pos + 4)[0]
        if tag == b'MAST':
            masters.append(b[pos + 6:pos + 6 + ln].rstrip(b'\0').decode('latin1'))
        pos += 6 + ln
    flags = struct.unpack_from('<I', b, 8)[0]
    bad = collections.Counter()
    first = []
    pos = 0
    n = 0
    while pos + 24 <= len(b):
        typ = b[pos:pos + 4]
        size, fl, fid = struct.unpack_from('<III', b, pos + 4)
        if typ == b'GRUP':
            pos += 24
            continue
        n += 1
        if fid > 0x0FFFFFFF and ((fid + 0x03000000) & 0xFE000000):
            bad[fid >> 24] += 1
            if len(first) < 3:
                first.append('%s %08X @%d' % (typ.decode('latin1'), fid, pos))
        pos += 24 + size
    print('%s: flags %08X, %d masters, %d records, refused %d by top byte %s; first %s'
          % (p.split('/')[-1], flags, len(masters), n, sum(bad.values()),
             {'%02X' % k: v for k, v in sorted(bad.items())}, first))
