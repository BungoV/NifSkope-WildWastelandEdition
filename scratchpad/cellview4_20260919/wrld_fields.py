#!/usr/bin/env python3
"""CELLVIEW4 item 2 -- does the record name a DEFAULT land texture anywhere?

The 6 quads of Sanctuary -20,7 that stay bare even under the four-corner rule
have no BTXT and zero opacity on every layer at every corner.  Before
proposing what to draw there, check whether the game is told: list every
subrecord the Commonwealth WRLD carries, and every LTEX the plugin defines
whose EDID looks like a default.  A field that is not there cannot be the
answer, and saying so is the point.
"""
import struct
import sys

import splat_sim as ss
from cell_census import fields


def main():
    esm = ss.Esm(sys.argv[1])
    buf = esm.buf
    seen = {}
    ltex_names = []

    def cb(rec, path):
        if rec is None:
            return
        if rec.type == b'WRLD':
            fl = list(ss.it_of(esm, rec))
            edid = b''
            for t, p in fl:
                if t == b'EDID':
                    edid = p.split(b'\0')[0]
            if edid == b'Commonwealth':
                for t, p in fl:
                    seen.setdefault(t.decode('latin1'), []).append(len(p))
        elif rec.type == b'LTEX':
            for t, p in ss.it_of(esm, rec):
                if t == b'EDID':
                    n = p.split(b'\0')[0].decode('latin1')
                    if 'efault' in n or 'ase' in n.lower()[:4]:
                        ltex_names.append((rec.form, n))

    esm.walk(24 + struct.unpack_from('<I', buf, 4)[0], len(buf), cb)
    print('Commonwealth WRLD subrecords (type: sizes):')
    for k in sorted(seen):
        print('   %-6s %s' % (k, seen[k]))
    print('\nLTEX records whose EDID mentions "default": %d' % len(ltex_names))
    for f, n in ltex_names[:20]:
        print('   %08X  %s' % (f, n))
    return 0


if __name__ == '__main__':
    sys.exit(main())
