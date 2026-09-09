"""FARRING1 step 7: WW_CHANGES.md gains 2026-09-06n above the newest entry.
The file is MIXED line endings (19020 CR / 21950 LF) and stays so: the new
text is LF-only, like the entries around it, and the CR count must not move."""

P = 'WW_CHANGES.md'
b = open(P, 'rb').read()
crBefore = b.count(b'\r')
s = b.decode('utf-8')

A = "## 2026-09-06l — The emissive multiple rides in the `.lodm`\n"
n = s.count(A)
assert n == 1, 'anchor matched %d times' % n
snip = open('scratchpad/snip_wwchanges.md', encoding='utf-8').read()
assert snip.count('\r') == 0, 'the new entry must be LF-only'
s = s.replace(A, snip + A)

out = s.encode('utf-8')
assert out.count(b'\r') == crBefore, 'CR count moved: %d -> %d' % (crBefore, out.count(b'\r'))
open(P, 'wb').write(out)
print('WW_CHANGES.md: %d -> %d bytes, CR %d (was %d), LF %d'
      % (len(b), len(out), out.count(b'\r'), crBefore, out.count(b'\n')))
