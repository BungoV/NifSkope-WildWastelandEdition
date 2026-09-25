"""BAKE2, ruling (a): the instruments' self-test for the .lodi v10 wide-scale bit, with NO build (skill
ww-lodo-version-bump s3). Hand-builds a version-10 file from a shipped v7 one and asks the decoders about it.

  S1  synth: set bit 7 (0x80) on instance K of a v7 file, write version 10, repair the chunk CRC, indexCrc32 and
      headerCrc32 (in that order). The NEW decoder must accept it and read instance K as old scale + 8, every other
      instance's decoded fields unchanged.
  S2  strip: synth vs source differs only at 0x04 (version), the flag byte, and the three CRC words.
  S3  the OLD decoder (git HEAD's lodgen_native_decode.py) must REFUSE the synth: 'version 10' is outside 3..9.
      This is the decoder half of "fails on the current code".
  S4  mutations, each refused by ITS OWN rule (CRCs repaired after mutating, so no stale CRC does the refusing):
        m1  bit 7 in a VERSION 9 file        -> 'reserved flags' (v9 knows 0x7f)
        m2  bit 7 in a VERSION 7 file        -> 'reserved flags' (v7 knows 0x3f)
        m3  bit 8 (0x100) in a version 10    -> 'reserved flags' (v10 knows 0xff)
        m4  scale word 0 WITHOUT bit 7, v10  -> 'scale is 0'
      and the control:
        c1  scale word 0 WITH bit 7, v10     -> accepted, reads 8.0
        c2  recrc() of the unmutated source  -> accepted, byte-identical to the source
usage: python widescale_synth.py <v7 .lodi> <scratch dir>
exit 0 = every line PASS."""
import sys, os, struct, zlib, subprocess, importlib.util

REPO = 'E:/Projects/NifskopeWWE-bake2'
sys.path.insert(0, REPO + '/tests/spells')
import lodgen_native_decode as NEW


def load_old(tmp):
    src = subprocess.run(['git', '-C', REPO, 'show', 'HEAD:tests/spells/lodgen_native_decode.py'],
                         capture_output=True, check=True).stdout
    p = os.path.join(tmp, 'old_decode.py'); open(p, 'wb').write(src)
    spec = importlib.util.spec_from_file_location('old_decode', p)
    m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m); return m


def recrc(b, T):
    """Repair every chunk CRC, then indexCrc32 over the decoder's own ranges, then headerCrc32."""
    h = T['header']
    for k, c in enumerate(T['chunks']):
        s, e = c['instanceFirst'], c['instanceFirst'] + c['instanceCount']
        rec = bytes(b[h['offInstances'] + s * 24:h['offInstances'] + e * 24])
        cold = bytes(b[h['offCold'] + s * 8:h['offCold'] + e * 8])
        co = h['offChunks'] + k * 32 + 0x18
        b[co:co + 4] = (zlib.crc32(cold, zlib.crc32(rec)) & 0xFFFFFFFF).to_bytes(4, 'little')
    blob = b''.join(bytes(b[o:o + n]) for o, n in T['indexRanges'])
    b[0x64:0x68] = (zlib.crc32(blob) & 0xFFFFFFFF).to_bytes(4, 'little')
    b[0x0C:0x10] = (zlib.crc32(bytes(b[0x10:T['headerBytes']])) & 0xFFFFFFFF).to_bytes(4, 'little')
    return b


def variant(src_bytes, T, path, version=None, k=0, orflags=0, scale=None):
    b = bytearray(src_bytes)
    if version is not None:
        b[4:8] = struct.pack('<I', version)
    off = T['header']['offInstances'] + k * 24
    if orflags:
        f = struct.unpack_from('<H', b, off + 0x14)[0] | orflags
        struct.pack_into('<H', b, off + 0x14, f)
    if scale is not None:
        struct.pack_into('<H', b, off + 0x0C, scale)
    recrc(b, T)
    open(path, 'wb').write(bytes(b))
    return bytes(b)


def outcome(mod, path):
    try:
        mod.read_lodi(path); return 'ACCEPT', None
    except mod.Refusal as e:
        return 'REFUSE', str(e)


def main():
    src, tmp = sys.argv[1], sys.argv[2]
    os.makedirs(tmp, exist_ok=True)
    OLD = load_old(tmp)
    a = open(src, 'rb').read()
    T = NEW.read_lodi(src)
    ver = T['header']['version']
    ok = True

    def line(name, good, detail):
        nonlocal ok
        ok &= bool(good)
        print('%s %s: %s' % ('PASS' if good else 'FAIL', name, detail))

    line('S0 source', ver == 7 and not any(r['flags'] & 0x80 for r in T['instances']),
         'version %d, %d instances, bit 7 set on %d' % (ver, len(T['instances']),
                                                         sum(1 for r in T['instances'] if r['flags'] & 0x80)))
    K = 0
    p1 = os.path.join(tmp, 'synth_v10.lodi')
    s = variant(a, T, p1, version=10, k=K, orflags=0x80)
    try:
        T1 = NEW.read_lodi(p1)
        same = all((x['scaleF'] == y['scaleF'] and x['flags'] == y['flags']) if i != K else True
                   for i, (x, y) in enumerate(zip(T['instances'], T1['instances'])))
        line('S1 new decoder reads the synth', T1['header']['version'] == 10 and
             abs(T1['instances'][K]['scaleF'] - (T['instances'][K]['scaleF'] + 8.0)) < 1e-9 and same,
             'instance %d scale %.5f -> %.5f; the other %d unchanged: %s'
             % (K, T['instances'][K]['scaleF'], T1['instances'][K]['scaleF'], len(T['instances']) - 1, same))
    except NEW.Refusal as e:
        line('S1 new decoder reads the synth', False, 'refused: %s' % e)
    moved = [i for i in range(len(a)) if a[i] != s[i]]
    h = T['header']; fo = h['offInstances'] + K * 24 + 0x14
    ci = next(k for k, c in enumerate(T['chunks']) if c['instanceCount'] and
              c['instanceFirst'] <= K < c['instanceFirst'] + c['instanceCount'])
    allowed = {4, 5, 6, 7, fo, fo + 1} | set(range(0x0C, 0x10)) | set(range(0x64, 0x68)) | \
        set(range(h['offChunks'] + ci * 32 + 0x18, h['offChunks'] + ci * 32 + 0x1C))
    line('S2 strip', len(a) == len(s) and set(moved) <= allowed and 4 in moved,
         '%d bytes moved, all in {version, flag byte, chunk/index/header CRC}: %s'
         % (len(moved), set(moved) <= allowed))
    o = outcome(OLD, p1)
    line('S3 old decoder refuses the synth', o[0] == 'REFUSE' and 'version 10' in (o[1] or ''), o)

    cases = [('m1 bit7 in v9', dict(version=9, orflags=0x80), 'REFUSE', 'reserved flags'),
             ('m2 bit7 in v7', dict(version=None, orflags=0x80), 'REFUSE', 'reserved flags'),
             ('m3 bit8 in v10', dict(version=10, orflags=0x100), 'REFUSE', 'reserved flags'),
             ('m4 scale 0 no bit7, v10', dict(version=10, scale=0), 'REFUSE', 'scale is 0'),
             ('c1 scale 0 with bit7, v10', dict(version=10, orflags=0x80, scale=0), 'ACCEPT', None)]
    for name, kw, want, why in cases:
        p = os.path.join(tmp, name.split()[0] + '.lodi')
        variant(a, T, p, k=K, **kw)
        o = outcome(NEW, p)
        good = o[0] == want and (why is None or why in (o[1] or ''))
        extra = ''
        if name.startswith('c1') and o[0] == 'ACCEPT':
            v = NEW.read_lodi(p)['instances'][K]['scaleF']; good &= v == 8.0; extra = ' scale %.5f' % v
        line('S4 ' + name, good, '%s %s%s' % (o[0], o[1] or '', extra))
    p = os.path.join(tmp, 'c2.lodi')
    c2 = variant(a, T, p)
    line('S4 c2 recrc control', c2 == a and outcome(NEW, p)[0] == 'ACCEPT', 'identical to source: %s' % (c2 == a))
    print('WIDESCALE INSTRUMENTS %s' % ('PASS' if ok else 'FAIL'))
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
