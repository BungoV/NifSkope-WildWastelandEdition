"""AUDIT1 step 6: re-aim the doctor's two aggregate cases.

The first draft asked for a bake with `--aggregate`, and that bake cannot be
made here: `--aggregate` REFUSES without `--impostors <card bake tree>`
(src/nifcli.cpp:3857 -- "an aggregate sheet is composited from the cell's own
trees' card sheets"), and a real card library is a bake of its own.

It does not need one. A version-5 `.lodi` header ALREADY carries the aggregate
words (src/lodifile.cpp:866..873), and src/lodifile.cpp:941 already builds the
blob table for `v5 && aggregateCount`. So one 4,096-aligned slot holding a
single 48-byte record, inserted where the writer would have put it -- between
the occluder range blob and the placement-AO blob -- is a version-5 file that
says it carries one aggregate. Measured on the audited exe before any fix: BOTH
files below are ACCEPTED by `--native-verify`, which is the whole of C2.

  agg-views   aggregateViews 1. A card needs two azimuths to blend between and
              the v4 reader refuses it by name (src/lodifile.cpp:904).
  agg-record  aggregateViews 2 -- a clean header -- and the record itself all
              zeros, so HEIGHT is clear, the identity bit is absent, the half
              extent, depthSpan and boundRadius are not positive and it stands
              for no instance. The v4 reader refuses the first of those by name
              (src/lodifile.cpp:1167).
"""
import io
import os
import sys
import tempfile

P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/lodgen_native_doctor.py'

OLD_DOC = """  agg-views    aggregateViews 1 in a file that carries aggregates: a card needs
               two azimuths to blend between, and the rule was written for v4
               only, so a v5 file (placement AO is on by default) was never
               asked.
  agg-cover    the covered-instance blob made to name its first instance twice.
               The runtime SUPPRESSES a covered instance, so a double claim is
               a tree that nothing draws in its place.
"""
NEW_DOC = """  agg-views    a VERSION 5 file given one aggregate whose aggregateViews is 1.
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

OLD_CASES_HEAD = "    if case == 'agg-views':"
NEW_CASES = '''    if case in ('agg-views', 'agg-record'):
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

'''


def main():
    s = io.open(P, encoding='utf-8', newline='').read()
    if 'agg-record' in s:
        print('ABORT: the doctor already carries the new cases')
        return 1
    for a in (OLD_DOC, OLD_CASES_HEAD, "    print('unknown case %r' % case)"):
        if s.count(a) != 1:
            print('ABORT: an anchor appears %d times' % s.count(a))
            return 1
    s = s.replace(OLD_DOC, NEW_DOC, 1)
    tail = "    print('unknown case %r' % case)"
    a, b = s.index(OLD_CASES_HEAD), s.index(tail)
    s = s[:a] + NEW_CASES + s[b:]
    # the header words the new cases write, beside the ones already named
    old = "I_OFF_COVERED = 0xB8\n"
    if s.count(old) != 1:
        print('ABORT: the offset block anchor appears %d times' % s.count(old))
        return 1
    s = s.replace(old, "I_OFF_COVERED, I_OFF_AGG = 0xB8, 0xB0\n"
                       "I_AGGSTRIDE, I_AGGSWITCH, I_AGGBAND = 0xC8, 0xCC, 0xD0\n"
                       "I_OFF_PAO = 0xE4\n"
                       "AGG_STRIDE, AGG_SLOT = 48, 4096   # LODI_AGGREGATE_STRIDE; every blob is 4,096-aligned\n", 1)
    if s.count('\r'):
        print('ABORT: CR in the result')
        return 1
    d = os.path.dirname(P)
    f = tempfile.NamedTemporaryFile('w', encoding='utf-8', newline='', dir=d,
                                    delete=False, suffix='.tmp')
    f.write(s)
    f.close()
    os.replace(f.name, P)
    print('lodgen_native_doctor.py: %d bytes, CR %d' % (len(s), s.count('\r')))
    return 0


if __name__ == '__main__':
    sys.exit(main())
