"""Check the CMSD material-run table in a NIF's compiled collision, independently.

    python tools/hkmatrun.py <file.nif> [--expect-materials=N] [--runs] [--quiet]
    python tools/hkmatrun.py <file.nif> --damage=<out.nif>

This parses the Havok packfile itself rather than asking NifSkope, so it fails
on a writer bug that NifSkope's own decoder would agree with. What it measures,
on every hknpCompressedMeshShapeData in the file:

  * every section's runs start at primitive 0 and sum to its primitive count
  * section +0x54 reads (firstRunIndex << 8) | runCount, and the block it names
    lies inside the run array
  * every run's material index is inside hknpBSMaterialProperties
  * byte 1 of a run record is zero

All four are measured invariants of the vanilla corpus (2,490 meshes, 3,898
sections, 9,536 run records; see WW_CHANGES 2026-08-20). Exit 0 if the file
holds up, 1 if it does not, 2 if it holds no compiled mesh at all.

`--runs` adds one line per run, in file order, so a caller can see WHICH
primitives got which material rather than only how many did.

`--damage` writes a copy with every section's +0x54 run count forced to 1, which
is exactly what the writer did before 2026-08-20. A harness uses it to show this
check is not vacuous: the damaged copy must fail.
"""
import struct, sys


def nif_blobs(path):
    """Every bhkPhysicsSystem / bhkRagdollSystem blob in a Fallout 4 NIF."""
    data = open(path, 'rb').read()
    pos = data.index(b'\x0a') + 1
    pos += 4 + 1 + 4                          # version, endian, user version
    nb, = struct.unpack_from('<I', data, pos); pos += 4
    bsver, = struct.unpack_from('<I', data, pos); pos += 4
    for _ in range(4 if bsver == 130 else 3):
        pos += 1 + data[pos]                  # export strings
    ntypes = struct.unpack_from('<H', data, pos)[0]; pos += 2
    types = []
    for _ in range(ntypes):
        n = struct.unpack_from('<I', data, pos)[0]; pos += 4
        types.append(data[pos:pos + n].decode('latin-1')); pos += n
    tidx = struct.unpack_from('<%dH' % nb, data, pos); pos += nb * 2
    bsize = struct.unpack_from('<%dI' % nb, data, pos); pos += nb * 4
    nstr = struct.unpack_from('<I', data, pos)[0]; pos += 8
    for _ in range(nstr):
        n = struct.unpack_from('<I', data, pos)[0]; pos += 4 + n
    ngrp = struct.unpack_from('<I', data, pos)[0]; pos += 4 + ngrp * 4
    out, off = [], pos
    for i in range(nb):
        if types[tidx[i]] in ('bhkPhysicsSystem', 'bhkRagdollSystem'):
            n, = struct.unpack_from('<I', data, off)
            # the blob's own file offset, so a caller can write back into it
            out.append((i, off + 4, data[off + 4:off + 4 + n]))
        off += bsize[i]
    return out


class Pack(object):
    """The __data__ section of an hk_2014 packfile, with its fixups resolved."""

    def __init__(self, blob):
        if struct.unpack_from('<II', blob, 0) != (0x57E0E057, 0x10C0C010):
            raise ValueError('not a Havok packfile')
        nsec, = struct.unpack_from('<i', blob, 20)
        secs = []
        for s in range(nsec):
            o = 0x40 + s * 0x40
            secs.append((blob[o:o + 19].split(b'\x00')[0].decode('latin-1'),)
                        + struct.unpack_from('<7i', blob, o + 20))
        names, cn = {}, secs[0]
        p, end = cn[1], cn[1] + cn[2]
        while p < end - 5 and blob[p + 4] == 0x09:
            e = blob.index(b'\x00', p + 5)
            names[p + 5 - cn[1]] = blob[p + 5:e].decode('latin-1')
            p = e + 1
        dt = [s for s in secs if s[0] == '__data__'][0]
        self.base = dt[1]                     # __data__ start, inside the blob
        self.data = blob[dt[1]:dt[1] + dt[2]]
        self.objects = []
        p = dt[1] + dt[4]
        while p + 12 <= dt[1] + dt[5]:
            src, sec, cno = struct.unpack_from('<iii', blob, p); p += 12
            if src != -1:
                self.objects.append((src, names.get(cno, '?')))
        self.objects.sort()
        self.local = {}
        p = dt[1] + dt[2]
        while p + 8 <= dt[1] + dt[3]:
            src, dst = struct.unpack_from('<ii', blob, p); p += 8
            if src != -1:
                self.local[src] = dst

    def u32(self, off):
        return struct.unpack_from('<I', self.data, off)[0]

    def of_class(self, name):
        return [o for o, c in self.objects if c == name]


def check(path, expect=None, quiet=False, runs_detail=False):
    def say(msg):
        if not quiet:
            print(msg)

    meshes, bad = 0, []
    for bi, at, blob in nif_blobs(path):
        try:
            pk = Pack(blob)
        except ValueError:
            continue
        tables = []
        for off in pk.of_class('hknpBSMaterialProperties'):
            n = pk.u32(off + 0x18)
            tables.append([pk.u32(off + 0x20 + i * 0x18 + 0x14) for i in range(n)])
        table = tables[0] if len(tables) == 1 else None
        for cm in pk.of_class('hknpCompressedMeshShapeData'):
            meshes += 1
            nsec, nruns = pk.u32(cm + 0x58), pk.u32(cm + 0xa8)
            secAt, runAt = pk.local.get(cm + 0x50), pk.local.get(cm + 0xa0)
            if secAt is None or runAt is None:
                bad.append('block %d: the mesh has no section or run pointer' % bi)
                continue
            runs = [tuple(pk.data[runAt + r * 4:runAt + r * 4 + 4]) for r in range(nruns)]
            say('block %d: %d sections, %d runs, materials %s'
                % (bi, nsec, nruns, ['0x%08X' % c for c in (table or [])]))
            # primitives per material, walking each section's OWN run block --
            # a run shared by two sections counts once for each of them
            tally = {}
            for s in range(nsec):
                f54 = pk.u32(secAt + s * 0x60 + 0x54)
                for mat, b1, start, n in runs[f54 >> 8:(f54 >> 8) + (f54 & 0xff)]:
                    tally[mat] = tally.get(mat, 0) + n
            if runs_detail:
                for s in range(nsec):
                    f54 = pk.u32(secAt + s * 0x60 + 0x54)
                    for r in range(f54 >> 8, (f54 >> 8) + (f54 & 0xff)):
                        mat, b1, start, n = runs[r]
                        crc = table[mat] if table is not None and mat < len(table) else None
                        print('run %d section %d %s primitives %d..%d'
                              % (r, s, '0x%08X' % crc if crc is not None else '?',
                                 start, start + n - 1))
            for mi in sorted(tally):
                crc = table[mi] if table is not None and mi < len(table) else None
                say('  material %d = %s: %d primitives'
                    % (mi, '0x%08X' % crc if crc is not None else '?', tally[mi]))
            for s in range(nsec):
                so = secAt + s * 0x60
                prims = pk.u32(so + 0x50) & 0xff
                f54 = pk.u32(so + 0x54)
                first, count = f54 >> 8, f54 & 0xff
                where = 'block %d section %d' % (bi, s)
                if count == 0 or first + count > nruns:
                    bad.append('%s: run block %d+%d is outside the %d-run array'
                               % (where, first, count, nruns))
                    continue
                at = 0
                for mat, b1, start, n in runs[first:first + count]:
                    if b1 != 0:
                        bad.append('%s: run byte 1 is %d, zero on all 9,536 vanilla records' % (where, b1))
                    if start != at:
                        bad.append('%s: run starts at primitive %d, expected %d' % (where, start, at))
                    if table is not None and mat >= len(table):
                        bad.append('%s: material index %d is past the %d-entry table'
                                   % (where, mat, len(table)))
                    at += n
                if at != prims:
                    bad.append('%s: runs cover %d primitives of %d' % (where, at, prims))
            if expect is not None and table is not None and len(table) != expect:
                bad.append('block %d: material table holds %d entries, expected %d'
                           % (bi, len(table), expect))
    if not meshes:
        say('%s holds no compiled mesh' % path)
        return 2
    for b in bad:
        print('FAIL %s' % b)
    say('%d mesh(es) checked, %d problems' % (meshes, len(bad)))
    return 1 if bad else 0


def damage(path, out):
    """Write a copy with every section's run count forced to 1 -- the pre-2026-08-20 bug."""
    data = bytearray(open(path, 'rb').read())
    hits = 0
    for bi, at, blob in nif_blobs(path):
        try:
            pk = Pack(blob)
        except ValueError:
            continue
        for cm in pk.of_class('hknpCompressedMeshShapeData'):
            secAt = pk.local.get(cm + 0x50)
            if secAt is None:
                continue
            for s in range(pk.u32(cm + 0x58)):
                o = at + pk.base + secAt + s * 0x60 + 0x54
                struct.pack_into('<I', data, o, 1)
                hits += 1
    open(out, 'wb').write(bytes(data))
    print('damaged %d section run-count fields -> %s' % (hits, out))
    return 0 if hits else 1


if __name__ == '__main__':
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    expect, out = None, None
    for a in sys.argv[1:]:
        if a.startswith('--expect-materials='):
            expect = int(a.split('=', 1)[1])
        if a.startswith('--damage='):
            out = a.split('=', 1)[1]
    if out:
        sys.exit(damage(args[0], out))
    sys.exit(check(args[0], expect, '--quiet' in sys.argv, '--runs' in sys.argv))
