"""survey_wrld.py - must the plugin override the WRLD record, and at what cost?

DLCRobot.esm carries the Commonwealth WRLD record (0000003C, 330,604 bytes)
as an override alongside its 2 LAND overrides. Copying that pattern has a
consequence the DLCs do not have: OUR plugin loads AFTER the DLCs, so a WRLD
record copied from Fallout4.esm would REVERT whatever the DLCs changed in it.

So, measured here:
  * which shipped plugins carry a Commonwealth WRLD override at all
  * exactly which subrecords each of them changes relative to the master, so
    the cost of reverting them is a fact and not a worry
  * whether the WRLD record carries a LOCALIZABLE subrecord (FULL), which
    would make a verbatim copy into a non-localized plugin wrong the same way
    a CELL FULL would have been

Read-only.
"""

import os
import struct

import fo4esm as E

DATA = r'X:\Programs\Steam\steamapps\common\Fallout 4\Data'

LOCALIZED_TAGS = {b'FULL', b'DESC', b'SHRT', b'RNAM', b'ITXT'}


def get_wrld(path, formid=E.COMMONWEALTH):
    buf = E.load(path)
    hit = []
    for node in E.top_level_groups(buf):
        if node.label == b'WRLD' and node.gtype == E.GT_TOP:
            off = node.offset + 24
            end = node.offset + node.gsize
            # WRLD records sit at the top of this group; children groups follow.
            while off + 24 <= end:
                if buf[off:off + 4] == b'GRUP':
                    _, gsize, _, _, _ = E.read_group_header(buf, off)
                    off += gsize
                    continue
                sig, dsize, flags, fid, tail = E.read_record_header(buf, off)
                if sig == b'WRLD' and fid == formid:
                    hit.append((off, dsize, flags, tail,
                                E.record_payload(buf, off, dsize, flags)))
                off += 24 + dsize
    return hit[0] if hit else None


def subs(payload):
    return list(E.subrecords(payload))


def main():
    master = get_wrld(DATA + r'\Fallout4.esm')
    print('=== Fallout4.esm, Commonwealth WRLD 0000003C ===')
    print('dsize=%d compressed=%s payload=%d tail=%s'
          % (master[1], bool(master[2] & E.COMPRESSED_FLAG), len(master[4]),
             master[3].hex()))
    msubs = subs(master[4])
    print('subrecords (%d):' % len(msubs))
    for tag, content in msubs:
        note = '  <-- LOCALIZABLE' if tag in LOCALIZED_TAGS else ''
        head = content[:24].hex()
        print('  %s len=%-8d %s%s' % (tag.decode('latin1'), len(content), head, note))

    names = sorted(n for n in os.listdir(DATA)
                   if n.lower().endswith(('.esm', '.esp', '.esl'))
                   and n != 'Fallout4.esm')
    print()
    print('=== plugins that override the Commonwealth WRLD record ===')
    for n in names:
        p = os.path.join(DATA, n)
        if os.path.getsize(p) < 200:
            continue
        try:
            r = get_wrld(p)
        except Exception as exc:
            print('  %-34s FAILED %s' % (n, exc))
            continue
        if r is None:
            print('  %-34s no Commonwealth WRLD override' % n)
            continue
        rsubs = subs(r[4])
        print('  %-34s payload=%d (master %d) tail=%s'
              % (n, len(r[4]), len(master[4]), r[3].hex()))
        mtags = [t for t, _ in msubs]
        rtags = [t for t, _ in rsubs]
        if mtags != rtags:
            print('      subrecord TAG SEQUENCE differs')
            print('      master: %s' % [t.decode('latin1') for t in mtags])
            print('      this  : %s' % [t.decode('latin1') for t in rtags])
        same = 0
        for i in range(min(len(msubs), len(rsubs))):
            if msubs[i] == rsubs[i]:
                same += 1
                continue
            a, b = msubs[i], rsubs[i]
            if a[0] == b[0]:
                nd = (sum(1 for p_, q in zip(a[1], b[1]) if p_ != q)
                      if len(a[1]) == len(b[1]) else None)
                print('      %s differs: master len %d, this len %d%s'
                      % (a[0].decode('latin1'), len(a[1]), len(b[1]),
                         '' if nd is None else ', %d differing bytes' % nd))
                if len(a[1]) <= 64:
                    print('         master %s' % a[1].hex())
                    print('         this   %s' % b[1].hex())
            else:
                print('      position %d: master %s vs this %s'
                      % (i, a[0].decode('latin1'), b[0].decode('latin1')))
        print('      %d of %d subrecords byte-identical to the master'
              % (same, len(msubs)))


if __name__ == '__main__':
    main()
