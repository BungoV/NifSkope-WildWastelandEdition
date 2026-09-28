"""redread.py -- GROUND1 reader red controls: doctored copies of a good v12 .lodi, each with its header and
index CRCs RECOMPUTED so the refusal that fires is the stream check, not a CRC.

  python redread.py <good v12 .lodi> <out dir>     -> writes <out dir>/<case>.lodi, prints the decoder verdict
"""
import os, sys, struct, zlib
sys.path.insert(0, 'E:/Projects/NifskopeWWE-ground1/tests/spells')
import lodgen_native_decode as ND

SRC, OUT = sys.argv[1], sys.argv[2]
os.makedirs(OUT, exist_ok=True)
good = open(SRC, 'rb').read()
T = ND.read_lodi(SRC)
h = T['header']
hdr = T['headerBytes']
n = h['instanceCount']
ranges = T['indexRanges']
# where indexCrc32 sits: the one header word in 0x48..0x6F equal to it
icrcAt = [o for o in range(0x48, 0x70, 4) if struct.unpack_from('<I', good, o)[0] == h['indexCrc32']]
assert len(icrcAt) == 1, icrcAt
icrcAt = icrcAt[0]
offG, bytesG = h['offVertexGround'], h['vertexGroundBytes']
assert (offG, bytesG) in ranges


def seal(b, rng=None):
    b = bytearray(b)
    rng = rng or ranges
    ic = zlib.crc32(b''.join(bytes(b[o:o + s]) for o, s in rng)) & 0xFFFFFFFF
    struct.pack_into('<I', b, icrcAt, ic)
    struct.pack_into('<I', b, 12, zlib.crc32(bytes(b[0x10:hdr])) & 0xFFFFFFFF)
    return bytes(b)


cases = {}
cases['good'] = good
b = bytearray(good); struct.pack_into('<Q', b, 0x130, 0)
cases['v12_offset_zero'] = seal(b, [r for r in ranges if r != (offG, bytesG)])
b = bytearray(good); struct.pack_into('<Q', b, 0x130, 0); struct.pack_into('<I', b, 0x138, 0)
cases['v12_no_stream'] = seal(b, [r for r in ranges if r != (offG, bytesG)])
b = bytearray(good); struct.pack_into('<I', b, 4, 11)
cases['v11_carrying_0x130'] = seal(b)
b = bytearray(good); b[0x13C] = 1
cases['pad_0x13C_nonzero'] = seal(b)
# one slice boundary moved by one: ground slice k-1 and k disagree with the AO slices
first = list(struct.unpack_from('<%dI' % (n + 1), good, offG))
k = next(i for i in range(1, n) if first[i + 1] - first[i] >= 2 and first[i] - first[i - 1] >= 2)
b = bytearray(good); struct.pack_into('<I', b, offG + 4 * k, first[k] + 1)
cases['slice_disagrees_with_ao'] = seal(b)
# the last offset no longer ends the stream
b = bytearray(good); struct.pack_into('<I', b, offG + 4 * n, first[n] - 1)
cases['end_offset_short'] = seal(b)
# offsets not monotone
b = bytearray(good); struct.pack_into('<I', b, offG + 4 * k, first[k + 1] + 1)
cases['offsets_not_monotone'] = seal(b)

for name, data in cases.items():
    p = os.path.join(OUT, name + '.lodi')
    open(p, 'wb').write(data)
    try:
        ND.read_lodi(p)
        v = 'ACCEPTED'
    except ND.Refusal as e:
        v = 'REFUSED: %s' % str(e)[:200]
    print('%-26s decoder %s' % (name, v))
