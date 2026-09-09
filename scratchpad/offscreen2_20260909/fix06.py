import sys

p = 'WW_CHANGES.md'
b = open(p, 'rb').read()
cr_before = b.count(b'\r')
lf_before = b.count(b'\n')

entry = open('scratchpad/offscreen2_20260909/entry.md', 'rb').read()
# The top of the file is LF-only (recent entries); match the neighbours and add
# no CR at all, so the file's mixed-by-design CR count is unchanged.
assert b'\r' not in entry, 'the entry must be LF-only to match the top of the file'
if not entry.endswith(b'\n'):
    entry += b'\n'

anchor = b'# NifSkope \xe2\x80\x94 Wild Wasteland Edition: Change Log\n\n'
n = b.count(anchor)
if n != 1:
    sys.stderr.write('anchor %d matches\n' % n)
    sys.exit(1)

out = b.replace(anchor, anchor + entry + b'\n', 1)
assert out.count(b'\r') == cr_before, 'CR count moved'
open(p, 'wb').write(out)
print('CR', out.count(b'\r'), '(was', cr_before, ') LF', out.count(b'\n'),
      '(was', lf_before, ') bytes', len(out))
