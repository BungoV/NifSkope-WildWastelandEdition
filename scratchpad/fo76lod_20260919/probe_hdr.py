"""Probe a NIF/BTO header: version, BS version, export strings, block type census."""
import struct
import sys
import collections


def probe(path):
    d = open(path, 'rb').read()
    o = d.index(b'\n') + 1
    ver, = struct.unpack_from('<I', d, o); o += 4
    endian = d[o]; o += 1
    uver, = struct.unpack_from('<I', d, o); o += 4
    nblocks, = struct.unpack_from('<I', d, o); o += 4
    bsver, = struct.unpack_from('<I', d, o); o += 4
    exports = []
    for _ in range(4):
        n = d[o]; o += 1
        exports.append(d[o:o + n].decode('latin-1')); o += n
    tail = ''
    if bsver >= 155:
        # FO76 header adds two more sized strings after Max Filepath in some builds
        pass
    ntypes, = struct.unpack_from('<H', d, o); o += 2
    types = []
    for _ in range(ntypes):
        n, = struct.unpack_from('<I', d, o); o += 4
        types.append(d[o:o + n].decode('latin-1')); o += n
    tidx = struct.unpack_from('<%dH' % nblocks, d, o); o += 2 * nblocks
    bsize = struct.unpack_from('<%dI' % nblocks, d, o); o += 4 * nblocks
    nstr, = struct.unpack_from('<I', d, o); o += 4
    o += 4
    strings = []
    for _ in range(nstr):
        n, = struct.unpack_from('<I', d, o); o += 4
        strings.append(d[o:o + n].decode('latin-1')); o += n
    ngroups, = struct.unpack_from('<I', d, o); o += 4 + 4 * ngroups
    datastart = o
    total = sum(bsize)
    nroots, = struct.unpack_from('<I', d, datastart + total)
    end = datastart + total + 4 + 4 * nroots
    print('%s' % path)
    print('  header line: %r' % d[:d.index(b'\n')].decode('latin-1'))
    print('  ver=%08x endian=%d userver=%d bsver=%d blocks=%d types=%d strings=%d'
          % (ver, endian, uver, bsver, nblocks, ntypes, nstr))
    print('  exports=%r' % (exports,))
    print('  block data %d..%d, roots=%d, end=%d, filesize=%d  %s'
          % (datastart, datastart + total, nroots, end, len(d),
             'OK' if end == len(d) else 'MISMATCH'))
    cen = collections.Counter(types[i] for i in tidx)
    for k, v in cen.most_common():
        print('   %6d  %s' % (v, k))
    return dict(types=types, tidx=tidx, bsize=bsize, strings=strings,
                datastart=datastart, data=d, bsver=bsver)


if __name__ == '__main__':
    for p in sys.argv[1:]:
        probe(p)
        print()
