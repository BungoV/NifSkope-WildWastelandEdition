"""Byte-splice IMPOSTORFIX4's entries into the TOP of the root MISTAKES.md.

The file is CRLF throughout (10360 CR, 10360 LF). The entry file is written LF
and converted here, so the CR count must rise by exactly the number of lines
added and by nothing else. Both counts are printed before and after.
"""
import os

ROOT = r'E:\Projects\NifskopeWildWastelandEdition'
DST = os.path.join(ROOT, 'MISTAKES.md')
SRC = os.path.join(ROOT, 'scratchpad', 'impostorfix4_20260919', 'mistakes_entry.txt')
MARK = b'Newest at the top.\r\n\r\n'

b = open(DST, 'rb').read()
e = open(SRC, 'rb').read().replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
print('BEFORE  bytes %d  CR %d  LF %d' % (len(b), b.count(b'\r'), b.count(b'\n')))
print('ENTRY   bytes %d  CR %d  LF %d' % (len(e), e.count(b'\r'), e.count(b'\n')))
assert b.count(MARK) == 1, 'header marker must appear exactly once'
assert e.count(b'\r') == e.count(b'\n'), 'entry must be pure CRLF'
i = b.index(MARK) + len(MARK)
out = b[:i] + e + b[i:]
print('AFTER   bytes %d  CR %d  LF %d' % (len(out), out.count(b'\r'), out.count(b'\n')))
assert out.count(b'\r') == b.count(b'\r') + e.count(b'\r')
assert out.count(b'\n') == b.count(b'\n') + e.count(b'\n')
assert len(out) == len(b) + len(e)
open(DST, 'wb').write(out)
print('WROTE', DST)
