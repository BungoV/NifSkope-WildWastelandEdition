"""AUDIT1: can a VERSION 5 .lodi be given an aggregate that breaks the rules,
and does the product's own reader accept it?

`--aggregate` refuses without a real card library (src/nifcli.cpp:3857), so the
v5 aggregate path cannot be reached by a bake in this lane. It can be reached
by construction: a v5 file's header already carries the aggregate words, and
src/lodifile.cpp:941 already builds the blob table for `v5 && aggregateCount`.
So one 48-byte record inserted where the writer would have put it -- between
the occluder range blob and the placement-AO blob -- is a legal-looking v5 file
with one aggregate, and every aggregate RULE in the reader is v4-only.

    python try_aggv5.py <real v5 .lodi> <out .lodi> <views>

views 1 breaks a HEADER rule (a card needs two azimuths); views 2 leaves the
header clean and the all-zero record breaks a PAYLOAD rule (HEIGHT is clear).
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, 'E:/Projects/NifskopeWildWastelandEdition/tests/spells')
import lodgen_native_mutate as M          # noqa: E402

H_OFF_AGG, H_OFF_COVERED = 0xB0, 0xB8
H_AGGCOUNT, H_COVCOUNT = 0xC0, 0xC4
H_AGGSTRIDE, H_AGGVIEWS, H_AGGSWITCH, H_AGGBAND = 0xC8, 0xCA, 0xCC, 0xD0
H_OFF_PAO, H_PAOCOUNT = 0xE4, 0xEC
H_FILEBYTES = 0x88
STRIDE = 48
SLOT = 4096      # every payload offset is 4,096-aligned (src/lodifile.cpp)


def rd(b, off, fmt):
    return struct.unpack_from('<' + fmt, b, off)[0]


def wr(b, off, fmt, v):
    struct.pack_into('<' + fmt, b, off, v)


def main():
    src, dst, views = sys.argv[1], sys.argv[2], int(sys.argv[3])
    b = bytearray(open(src, 'rb').read())
    ver = rd(b, 4, 'I')
    aggWas = rd(b, H_AGGCOUNT, 'I')
    offPao = rd(b, H_OFF_PAO, 'Q')
    print('source: version %d, aggregateCount %d, offPlacementAo %d, %d bytes'
          % (ver, aggWas, offPao, len(b)))
    if ver != 5 or aggWas != 0 or offPao == 0:
        print('ABORT: this case wants a version-5 file with no aggregate and an AO blob')
        return 2
    out = bytearray(b[:offPao]) + bytearray(SLOT) + bytearray(b[offPao:])
    wr(out, H_OFF_AGG, 'Q', offPao)
    wr(out, H_OFF_COVERED, 'Q', offPao + SLOT)
    wr(out, H_AGGCOUNT, 'I', 1)
    wr(out, H_COVCOUNT, 'I', 0)
    wr(out, H_AGGSTRIDE, 'H', STRIDE)
    wr(out, H_AGGVIEWS, 'H', views)
    wr(out, H_AGGSWITCH, 'f', 8.0)
    wr(out, H_AGGBAND, 'f', 2.0)
    wr(out, H_OFF_PAO, 'Q', offPao + SLOT)
    wr(out, H_FILEBYTES, 'Q', len(out))
    M.resign_lodi(out)
    open(dst, 'wb').write(bytes(out))
    print('wrote %s: one 48-byte aggregate record at %d, aggregateViews %d, %d bytes'
          % (dst, offPao, views, len(out)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
