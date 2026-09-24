# -*- coding: utf-8 -*-
"""splice.py -- anchored text splices that refuse unless every anchor matches
exactly once, and assert the CR count did not move (line endings by Python
byte count, never grep)."""
import sys


def splice(path, edits):
    b = open(path, 'rb').read()
    cr = b.count(b'\r')
    s = b.decode('utf-8')
    for anchor, new, where in edits:
        n = s.count(anchor)
        if n != 1:
            print('REFUSED: anchor matches %d times in %s: %r' % (n, path, anchor[:70]))
            sys.exit(1)
        if where == 'after':
            s = s.replace(anchor, anchor + new)
        elif where == 'replace':
            s = s.replace(anchor, new)
        else:
            s = s.replace(anchor, new + anchor)
    out = s.encode('utf-8')
    assert out.count(b'\r') == cr, 'CR count moved'
    open(path, 'wb').write(out)
    print('%s: %d -> %d bytes, CR %d' % (path, len(b), len(out), cr))
