"""IDENT2 (2026-09-29): refusal checks for the .lodi v13 file-wide group table, on a real v13 file.
Each case edits the file, re-signs indexCrc32 and headerCrc32 over the file's own ranges (so the RULE
answers, not a checksum), writes it to <out>/<case>.lodi and asks the independent Python decoder.
The C++ reader is asked the same files by the caller (--native-verify).
usage: python v13mut.py <v13 .lodi> <out dir>"""
import os, struct, sys, zlib
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'tests', 'spells'))
import lodgen_native_decode as D

src, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
raw = open(src, 'rb').read()
T = D.read_lodi(src)
h = T['header']
assert h['version'] == 13 and h['groupStride'] == 4, 'not a v13 file'
off, n, gc = h['offGroup'], h['instanceCount'], h['groupCount']


def resign(b, ranges):
    idx = b''.join(bytes(b[o:o + s]) for o, s in ranges)
    struct.pack_into('<I', b, 0x64, zlib.crc32(idx) & 0xFFFFFFFF)
    struct.pack_into('<I', b, 0x0C, zlib.crc32(bytes(b[0x10:T['headerBytes']])) & 0xFFFFFFFF)


def g(b, i):
    return struct.unpack_from('<I', b, off + 4 * i)[0]


def put(b, i, v):
    struct.pack_into('<I', b, off + 4 * i, v)


# a placement whose group has other members (so moving it does not empty its id)
cnt = {}
for i in range(n):
    cnt[g(raw, i)] = cnt.get(g(raw, i), 0) + 1
shared = next(i for i in range(n) if cnt[g(raw, i)] > 1)
lone = next(i for i in range(n) if cnt[g(raw, i)] == 1)
cases = [
    ('control_resigned', lambda b: None, None),
    ('id_at_groupCount', lambda b: put(b, shared, gc), 'group'),
    ('id_unused', lambda b: put(b, lone, g(b, shared)), 'group'),
    ('groupCount_plus1', lambda b: struct.pack_into('<I', b, 0x108, gc + 1), 'group'),
    ('stride_2', lambda b: struct.pack_into('<H', b, 0x10C, 2), 'stride'),
    ('no_group_table', lambda b: (struct.pack_into('<Q', b, 0x100, 0), struct.pack_into('<I', b, 0x108, 0)), 'group'),
]
bad = 0
for name, fn, want in cases:
    b = bytearray(raw)
    fn(b)
    ranges = [r for r in T['indexRanges'] if not (name == 'no_group_table' and r[0] == off)]
    resign(b, ranges)
    p = os.path.join(out, name + '.lodi')
    open(p, 'wb').write(b)
    try:
        D.read_lodi(p)
        got = None
    except D.Refusal as e:
        got = str(e)
    ok = (got is None) if want is None else (got is not None and want in got.lower())
    bad += not ok
    print('%-4s %-18s %s' % ('ok' if ok else 'FAIL', name, got if got else 'loads'))
print('RESULT', 'PASS' if not bad else 'FAIL %d' % bad)
sys.exit(1 if bad else 0)
