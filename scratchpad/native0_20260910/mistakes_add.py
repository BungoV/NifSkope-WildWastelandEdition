"""Prepend lane NATIVE0b's MISTAKES.md entries under the file's header block (newest at the
top). LF-only file; CR count asserted 0 before and after."""
import sys
P = 'MISTAKES.md'
b = open(P, 'rb').read()
assert b.count(b'\r') == 0
s = b.decode('utf-8')
anchor = 'Newest at the top.\n\n'
assert s.count(anchor) == 1
entry = open('scratchpad/native0_20260910/MISTAKES_ENTRIES.md', 'rb').read().decode('utf-8')
if entry.split('\n', 1)[0] in s:
    print('already applied'); sys.exit(0)
s = s.replace(anchor, anchor + entry.rstrip('\n') + '\n\n', 1)
out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(P, 'wb').write(out)
print('MISTAKES.md: +%d lines' % (out.count(b'\n') - b.count(b'\n')))
