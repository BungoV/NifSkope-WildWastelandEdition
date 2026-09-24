"""Read only: TES4 header of each plugin (flags, HEDR, form version, masters) and the
records' raw top bytes vs the master count. Prints one verdict block per plugin."""
import struct, sys, collections, zlib
for p in sys.argv[1:]:
    b = open(p, 'rb').read()
    tes4 = struct.unpack_from('<I', b, 4)[0]
    flags = struct.unpack_from('<I', b, 8)[0]
    formver = struct.unpack_from('<H', b, 20)[0]
    masters, hedr = [], None
    pos, end = 24, 24 + tes4
    while pos + 6 <= end:
        tag, ln = b[pos:pos + 4], struct.unpack_from('<H', b, pos + 4)[0]
        d = b[pos + 6:pos + 6 + ln]
        if tag == b'MAST': masters.append(d.rstrip(b'\0').decode('latin1'))
        if tag == b'HEDR': hedr = struct.unpack_from('<fII', d)
        pos += 6 + ln
    tops = collections.Counter(); types = collections.Counter(); ex = {}
    pos = 24 + tes4; n = 0
    while pos + 24 <= len(b):
        typ = b[pos:pos + 4]
        size, fl, fid = struct.unpack_from('<III', b, pos + 4)
        if typ == b'GRUP':
            pos += 24; continue
        n += 1; t = fid >> 24; tops[t] += 1; types[(t, typ)] += 1
        ex.setdefault(t, '%s %08X' % (typ.decode(), fid))
        pos += 24 + size
    print('%s\n  flags %08X (ESM %d, LOCALIZED %d, ESL %d), form version %d, HEDR %s\n  masters %s\n  records %d, top bytes %s\n  first per byte %s'
          % (p.split('/')[-1], flags, flags & 1, (flags >> 7) & 1, (flags >> 9) & 1, formver, hedr, masters, n,
             {'%02X' % k: v for k, v in sorted(tops.items())}, ex))
    print('  types by byte', {'%02X %s' % (k[0], k[1].decode()): v for k, v in sorted(types.items())})
