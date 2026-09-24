"""What do the base model strings actually look like, and what does the
`architecture\\` prefix catch? Reads the .lodo string blob directly, so this is
the writer's own bytes and not a guess."""
import sys
from collections import Counter

b = open(sys.argv[1], 'rb').read()
strs = [x for x in b.split(b'\x00') if x.lower().endswith(b'.nif')]
print('NUL-terminated .nif strings in the .lodo: %d' % len(strs))
pref = Counter()
for x in strs:
    t = x.decode('latin-1').replace('/', '\\')
    pref[t.split('\\')[0].lower()] += 1
print('first path component, top 15:')
for k, v in pref.most_common(15):
    print('  %-30s %d' % (k if k else '(no separator: a bare file name)', v))
print()
print('first 8 strings verbatim:')
for x in strs[:8]:
    print('   ' + x.decode('latin-1'))
arch = [x for x in strs if x.decode('latin-1').lower().replace('/', '\\').startswith('architecture\\')]
print()
print('strings under architecture\\ : %d of %d' % (len(arch), len(strs)))
