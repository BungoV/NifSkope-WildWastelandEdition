"""IMPOSTORFIX5 -- splice this lane's entries into the top of root MISTAKES.md.

The file is CRLF (10452 CR, 10452 LF: every line). The entries are authored LF
in `mistakes_entries.txt`, so they are converted on the way in, and the CR count
is asserted to move by exactly the number of lines added. Byte splice, one
anchor, exact-once.
"""
import os, sys

ROOT = r'E:\Projects\NifskopeWildWastelandEdition'
PATH = os.path.join(ROOT, 'MISTAKES.md')
SRC = os.path.join(ROOT, 'scratchpad', 'impostorfix5_20260919', 'mistakes_entries.txt')

ANCHOR = b'Newest at the top.\r\n\r\n'


def main(apply_it):
    b = open(PATH, 'rb').read()
    n = b.count(ANCHOR)
    print('MISTAKES.md  anchor matches %d  %s' % (n, 'OK' if n == 1 else 'REFUSE'))
    print('before: bytes %d  CR %d  LF %d' % (len(b), b.count(b'\r'), b.count(b'\n')))
    if n != 1:
        return 1
    add = open(SRC, 'rb').read().replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
    if not add.endswith(b'\r\n\r\n'):
        add = add.rstrip(b'\r\n') + b'\r\n\r\n'
    i = b.index(ANCHOR) + len(ANCHOR)
    out = b[:i] + add + b[i:]
    lines = add.count(b'\r\n')
    assert out.count(b'\r') == b.count(b'\r') + lines, 'CR count did not move by the lines added'
    assert out.count(b'\n') == b.count(b'\n') + lines, 'LF count did not move by the lines added'
    assert b'\n\n' not in out.replace(b'\r\n', b'X'), 'a bare LF got in'
    if not apply_it:
        print('--check only: nothing written. %d lines would be added.' % lines)
        return 0
    open(PATH, 'wb').write(out)
    print('after:  bytes %d  CR %d  LF %d  (+%d lines)'
          % (len(out), out.count(b'\r'), out.count(b'\n'), lines))
    print('WROTE', PATH)
    return 0


if __name__ == '__main__':
    sys.exit(main('--apply' in sys.argv))
