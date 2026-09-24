ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'
dst = ROOT + 'MISTAKES.md'
src = ROOT + 'scratchpad/btofree1_20260916/MISTAKES_ENTRIES.md'
b = open(dst, 'rb').read()
cr0, lf0, n0 = b.count(b'\r'), b.count(b'\n'), len(b)
assert cr0 == 0, 'MISTAKES.md is no longer LF-only'
entry = open(src, 'rb').read()
assert entry.count(b'\r') == 0, 'the entry file is not LF-only'
anchor = b'## 2026-09-16 16:2x -- lane NATIVE1c'
n = b.count(anchor)
assert n == 1, 'anchor count %d' % n
nb = b.replace(anchor, entry + anchor)
assert nb.count(b'\r') == 0
open(dst, 'wb').write(nb)
print('MISTAKES.md %d -> %d bytes, CR %d -> %d, LF %d -> %d'
      % (n0, len(nb), cr0, nb.count(b'\r'), lf0, nb.count(b'\n')))
