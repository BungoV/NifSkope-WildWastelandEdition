#!/usr/bin/env python
"""Doctor ONE thing in a `.lodo`/`.lodi` pair and re-sign every checksum the
change would otherwise trip, so that what answers is the PRODUCT'S OWN READER
(`lodgen --native-verify`, src/lodofile.cpp + src/lodifile.cpp) and not a CRC.

This is the C++ half of what `lodgen_native_mutate.py` does for the independent
Python decoder: same idea, same re-signing, but every case here is aimed at a
rule that the C++ reader is the only thing that enforces.

    python tests/spells/lodgen_native_doctor.py <case> <lodo> <lodi>

The files are edited IN PLACE -- copy them first. One line is printed saying
what moved and, after `expect:`, the substring the refusal must contain.

  lodi-wrap    the instance blob's offset set 4,096 short of 2^64, so that
               `off + bytes` WRAPS below fileBytes. A reader that adds before
               it compares accepts the file and then walks the pad bytes from
               the previous payload all the way to that offset.
  lodo-wrap    the same, on the .lodo's vertex blob.
  agg-views    a VERSION 5 file given one aggregate whose aggregateViews is 1.
               A card needs two azimuths to blend between; the header rule that
               says so was written for version 4 only, and placement AO is on
               by default, so every bake since writes version 5.
  agg-record   the same version-5 file given one aggregate whose 48-byte record
               is all zeros: HEIGHT clear, no identity bit, no half extent, no
               depthSpan, no boundRadius, standing for no instance. The whole
               payload gate is version-4 only for the same reason.

The aggregate is INSERTED rather than baked, because `--aggregate` refuses
without a real card library (src/nifcli.cpp:3857). One 4,096-aligned slot goes
where the writer would have put it, between the occluder range blob and the
placement-AO blob, and every checksum is re-signed.
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import lodgen_native_mutate as M          # noqa: E402  (resign_lodi / resign_lodo)

WRAP = 0xFFFFFFFFFFFFF000                 # 4,096-aligned, 4,096 short of 2^64

# .lodi header words (src/lodifile.cpp:32..54)
I_HCRC, I_OFF_INST, I_FILEBYTES = 0x0C, 0x78, 0x88
I_INSTCOUNT, I_AGGCOUNT, I_COVCOUNT, I_AGGVIEWS = 0x58, 0xC0, 0xC4, 0xCA
I_OFF_COVERED, I_OFF_AGG = 0xB8, 0xB0
I_AGGSTRIDE, I_AGGSWITCH, I_AGGBAND = 0xC8, 0xCC, 0xD0
I_OFF_PAO = 0xE4
AGG_STRIDE, AGG_SLOT = 48, 4096   # LODI_AGGREGATE_STRIDE; every blob is 4,096-aligned
# .lodo header words (src/lodofile.cpp:30..38)
O_HCRC, O_VERTICES, O_OFF_VERTS, O_VSTRIDE = 0x0C, 0x60, 0x98, 0x6A


def rd(b, off, fmt):
    return struct.unpack_from('<' + fmt, b, off)[0]


def wr(b, off, fmt, v):
    struct.pack_into('<' + fmt, b, off, v)


def main():
    if len(sys.argv) != 4:
        print(__doc__)
        return 2
    case, lodo, lodi = sys.argv[1], sys.argv[2], sys.argv[3]
    o = bytearray(open(lodo, 'rb').read())
    i = bytearray(open(lodi, 'rb').read())

    if case == 'lodi-wrap':
        was = rd(i, I_OFF_INST, 'Q')
        bytes_ = rd(i, I_INSTCOUNT, 'I') * 24
        wr(i, I_OFF_INST, 'Q', WRAP)
        M.resign_header_lodi(i)
        open(lodi, 'wb').write(bytes(i))
        print('lodi: instance blob offset %d -> 0x%X (%d bytes, sum wraps to %d, '
              'fileBytes %d); expect: instance blob runs past the file'
              % (was, WRAP, bytes_, (WRAP + bytes_) % (1 << 64), rd(i, I_FILEBYTES, 'Q')))
        return 0

    if case == 'lodo-wrap':
        was = rd(o, O_OFF_VERTS, 'Q')
        bytes_ = rd(o, O_VERTICES, 'I') * rd(o, O_VSTRIDE, 'H')
        wr(o, O_OFF_VERTS, 'Q', WRAP)
        M.resign_header_lodo(o)
        open(lodo, 'wb').write(bytes(o))
        print('lodo: vertex blob offset %d -> 0x%X (%d bytes, sum wraps to %d); '
              'expect: vertex blob runs past the file'
              % (was, WRAP, bytes_, (WRAP + bytes_) % (1 << 64)))
        return 0

    if case in ('agg-views', 'agg-record'):
        ver = rd(i, 4, 'I')
        if ver != 5 or rd(i, I_AGGCOUNT, 'I') != 0 or rd(i, I_OFF_PAO, 'Q') == 0:
            print('SKIP: this case wants a version-5 .lodi with no aggregate and an AO blob '
                  '(version %d, aggregateCount %d)' % (ver, rd(i, I_AGGCOUNT, 'I')))
            return 3
        views = 1 if case == 'agg-views' else 2
        offPao = rd(i, I_OFF_PAO, 'Q')
        out = bytearray(i[:offPao]) + bytearray(AGG_SLOT) + bytearray(i[offPao:])
        wr(out, I_OFF_AGG, 'Q', offPao)
        wr(out, I_OFF_COVERED, 'Q', offPao + AGG_SLOT)
        wr(out, I_AGGCOUNT, 'I', 1)
        wr(out, I_COVCOUNT, 'I', 0)
        wr(out, I_AGGSTRIDE, 'H', AGG_STRIDE)
        wr(out, I_AGGVIEWS, 'H', views)
        wr(out, I_AGGSWITCH, 'f', 8.0)
        wr(out, I_AGGBAND, 'f', 2.0)
        wr(out, I_OFF_PAO, 'Q', offPao + AGG_SLOT)
        wr(out, I_FILEBYTES, 'Q', len(out))
        M.resign_lodi(out)
        open(lodi, 'wb').write(bytes(out))
        print('lodi: version 5 given one aggregate at %d, aggregateViews %d, the record %s; '
              'expect: %s'
              % (offPao, views,
                 'all zeros' if views == 2 else 'all zeros behind a broken header',
                 'aggregateViews' if views == 1 else 'HEIGHT is clear'))
        return 0

    print('unknown case %r' % case)
    return 2


if __name__ == '__main__':
    sys.exit(main())
