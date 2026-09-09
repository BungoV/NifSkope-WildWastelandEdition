"""survey_hedr.py - what does TES4.HEDR "number of records" actually count?

Records only? Records + GRUPs? Including the TES4 itself? Guessing wrong here
gives a plugin that loads but that tools distrust. Settle it by counting both
quantities in every shipped plugin and seeing which one HEDR equals.

Read-only.
"""

import os
import struct

import fo4esm as E

DATA = r'X:\Programs\Steam\steamapps\common\Fallout 4\Data'


def counts(path):
    buf = E.load(path)
    sig, dsize, flags, formid, tail = E.read_record_header(buf, 0)
    hedr = None
    for tag, content in E.subrecords(E.record_payload(buf, 0, dsize, flags)):
        if tag == b'HEDR':
            hedr = struct.unpack('<fiI', content[:12])
            break
    nrec = [0]
    ngrp = [0]

    def on_rec(*a):
        nrec[0] += 1

    def on_grp(*a):
        ngrp[0] += 1

    for node in E.top_level_groups(buf):
        ngrp[0] += 1
        E.walk(buf, node.offset + 24, node.offset + node.gsize, [node], on_rec, on_grp)
    return hedr, nrec[0], ngrp[0]


def main():
    print('%-34s %10s %10s %10s   %s' % ('plugin', 'HEDR', 'records', 'GRUPs', 'HEDR =='))
    for n in sorted(os.listdir(DATA)):
        if not n.lower().endswith(('.esm', '.esp', '.esl')):
            continue
        p = os.path.join(DATA, n)
        if os.path.getsize(p) < 200:
            continue
        try:
            hedr, nrec, ngrp = counts(p)
        except Exception as exc:
            print('%-34s FAILED %s' % (n, exc))
            continue
        h = hedr[1]
        which = []
        if h == nrec:
            which.append('records')
        if h == nrec + 1:
            which.append('records+TES4')
        if h == nrec + ngrp:
            which.append('records+GRUPs')
        if h == nrec + ngrp + 1:
            which.append('records+GRUPs+TES4')
        print('%-34s %10d %10d %10d   %s'
              % (n, h, nrec, ngrp, ','.join(which) or 'NONE OF THESE'))


if __name__ == '__main__':
    main()
