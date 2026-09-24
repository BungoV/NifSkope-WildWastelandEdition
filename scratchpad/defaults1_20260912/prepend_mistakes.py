"""DEFAULTS1: put this lane's MISTAKES entries at the TOP of the root ledger.

Refusing: the header must be found exactly once, the entry must not already be
present, and the CR count must stay 0.
"""
import sys

ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'
DEST = ROOT + 'MISTAKES.md'
SRC = ROOT + 'scratchpad/defaults1_20260912/MISTAKES_ENTRIES.md'
MARK = '## 2026-09-12 -- lane NATIVEVIEW1'

entry = open(SRC, 'rb').read().decode('utf-8')
raw = open(DEST, 'rb').read()
print('before  %d bytes  LF %d  CR %d' % (len(raw), raw.count(b'\n'), raw.count(b'\r')))
text = raw.decode('utf-8')

if '## 2026-09-12 -- lane DEFAULTS1' in text:
    print('REFUSED: the DEFAULTS1 entry is already in the ledger')
    sys.exit(2)
c = text.count(MARK)
if c != 1:
    print('REFUSED: the anchor %r was found %d times' % (MARK, c))
    sys.exit(2)

text = text.replace(MARK, entry.rstrip('\n') + '\n\n' + MARK)
out = text.encode('utf-8')
open(DEST, 'wb').write(out)
back = open(DEST, 'rb').read()
print('after   %d bytes  LF %d  CR %d' % (len(back), back.count(b'\n'), back.count(b'\r')))
assert back.count(b'\r') == 0, 'CR bytes appeared'
assert back == out, 'read-back differs'
print('ok')
