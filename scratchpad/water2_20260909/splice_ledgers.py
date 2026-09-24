#!/usr/bin/env python3
"""splice_ledgers.py -- put lane WATER2's entries into MISTAKES.md and
WW_CHANGES.md, newest first, without touching a byte of either neighbour.

Both files take the new block immediately after their header paragraph, which
is where "newest at the top" means. Line endings are measured with PYTHON BYTE
COUNTS before and after (CONSTITUTION rule 8): `MISTAKES.md` is LF-only and
must stay 0 CR; `WW_CHANGES.md` is MIXED and stays so, so the delta in CR must
be exactly 0 -- the inserted block is LF-only, matching the LF-only entries at
the top of that file.
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))


def splice(path, anchor, block, expect_cr_delta=0):
    raw = open(path, 'rb').read()
    cr0 = raw.count(b'\r')
    s = raw.decode('utf-8')
    assert s.count(anchor) == 1, 'anchor not unique in %s' % path
    assert block not in s, 'already spliced into %s' % path
    s = s.replace(anchor, anchor + block, 1)
    out = s.encode('utf-8')
    cr1 = out.count(b'\r')
    assert cr1 - cr0 == expect_cr_delta, 'CR moved by %d in %s' % (cr1 - cr0, path)
    open(path, 'wb').write(out)
    print('  %-16s CR %d -> %d, %d -> %d bytes' % (os.path.basename(path), cr0, cr1,
                                                   len(raw), len(out)))


def main():
    mistakes = open(os.path.join(HERE, 'MISTAKES_ENTRIES.md'), encoding='utf-8').read()
    # drop this file's own title line; keep the four entries
    body = mistakes.split('\n', 2)[2].lstrip('\n')
    splice('MISTAKES.md', 'Newest at the top.\n', '\n' + body.rstrip('\n') + '\n')

    changes = open(os.path.join(HERE, 'WW_CHANGES_ENTRY.md'), encoding='utf-8').read()
    splice('WW_CHANGES.md',
           '# NifSkope — Wild Wasteland Edition: Change Log\n',
           '\n' + changes.rstrip('\n') + '\n')
    return 0


if __name__ == '__main__':
    sys.exit(main())
