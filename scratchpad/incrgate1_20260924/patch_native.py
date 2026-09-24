import os
P = r'E:\Projects\NifskopeWWE-incrgate1\tests\spells\lodgen_native.sh'
S = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'snip_row6_leg.txt')
src = open(P, 'rb').read()
cr0 = src.count(b'\r')
snip = open(S, 'rb').read()
assert snip.count(b'\r') == 0
a = b'grep -E "^  (ok|FAIL)   D|FLOOR a grown box" "$W/cut_occ.log"\n'
assert src.count(a) == 1, src.count(a)
src = src.replace(a, a + snip)
assert src.count(b'\r') == cr0
open(P + '.tmp', 'wb').write(src)
os.replace(P + '.tmp', P)
print('patched, CR', cr0)
