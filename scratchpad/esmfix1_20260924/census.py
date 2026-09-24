"""Read only: for every plugin in a --print-source listing, the records whose raw top byte is
>= the plugin's master count (the game resolves those to the plugin itself), split into
byte == masterCnt (the normal own-record byte) and byte > masterCnt (what this lane changes),
and which of those the rung reader refuses. One line per plugin with anything > masterCnt."""
import struct, sys, re, collections
paths = [m.group(1) for m in (re.match(r'plugin \d+: (.*)$', l.rstrip('\n')) for l in open(sys.argv[1], encoding='utf-8')) if m]
tot = 0
for p in paths:
    b = open(p, 'rb').read()
    tes4 = struct.unpack_from('<I', b, 4)[0]
    mc = 0; pos, end = 24, 24 + tes4
    while pos + 6 <= end:
        tag, ln = b[pos:pos + 4], struct.unpack_from('<H', b, pos + 4)[0]
        if tag == b'MAST': mc += 1
        pos += 6 + ln
    over = collections.Counter(); refused = 0; n = 0
    pos = 24 + tes4
    while pos + 24 <= len(b):
        typ = b[pos:pos + 4]; size, fl, fid = struct.unpack_from('<III', b, pos + 4)
        if typ == b'GRUP': pos += 24; continue
        n += 1; t = fid >> 24
        if t > mc: over[t] += 1
        if fid > 0x0FFFFFFF and ((fid + 0x03000000) & 0xFE000000): refused += 1
        pos += 24 + size
    tot += sum(over.values())
    if over or refused:
        print('%s: %d masters, %d records, top byte > masters %s, rung-refused %d' % (p.split('/')[-1], mc, n, {'%02X' % k: v for k, v in sorted(over.items())}, refused))
print('plugins %d, records with top byte > master count: %d' % (len(paths), tot))
