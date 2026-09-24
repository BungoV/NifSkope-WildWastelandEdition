# Lane BTOFREE1, 2026-09-16 -- the third piece of red, added to section 4 the
# moment it was attributed rather than at the end.
P = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/btofree1_20260916/lane_btofree1_report.md'

ADD = '''* `tests/spells/lodgen_byte_gate.sh` **phase (c)**: the panel\u2019s `Commonwealth.4.-20.24.DDS` and
  `Commonwealth.lodi` are not the command line\u2019s, at identical sizes. I proved it is not mine by
  running the rung exe through the same gate \u2014 it reads the same two files \u2014 and by comparing the
  two panel trees, which are byte-identical on all fourteen surviving files. \u00a72 has the numbers.
  Owner: whoever the director assigns. It is a **front-end divergence**, so it is the kind of thing
  that quietly becomes "the panel bakes a different game" if it is left.
'''

b = open(P, 'rb').read()
cr0 = b.count(b'\r')
s = b.decode('utf-8')
old = ('''* the stock `.BTO` silent ~6 percent drop on dense chunks (measured by GENSMALL1). **bungo's call**,''')
assert s.count(old) == 1, 'anchor count %d' % s.count(old)
s = s.replace(old, ADD + old)
nb = s.encode('utf-8')
assert nb.count(b'\r') == cr0
open(P, 'wb').write(nb)
print('report 4b written: %d -> %d B, CR %d, LF %d' % (len(b), len(nb), nb.count(b'\r'), nb.count(b'\n')))
