"""FARRING1 step 9: docs/MISTAKES.md gains the two this lane recognised,
newest first, above the current top entry."""

P = 'docs/MISTAKES.md'
b = open(P, 'rb').read()
assert b.count(b'\r') == 0
s = b.decode('utf-8')

A = "## 2026-09-06 — A hand-written BGSM reader that answered instead of failing\n"
n = s.count(A)
assert n == 1, 'anchor matched %d times' % n
snip = open('scratchpad/snip_mistakes.md', encoding='utf-8').read()
assert snip.count('\r') == 0
s = s.replace(A, snip + A)

out = s.encode('utf-8')
assert out.count(b'\r') == 0
open(P, 'wb').write(out)
print('docs/MISTAKES.md: %d -> %d bytes, CR %d' % (len(b), len(out), out.count(b'\r')))
