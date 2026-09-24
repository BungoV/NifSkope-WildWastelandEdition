import os
P = r'E:\Projects\NifskopeWWE-incrgate1\tests\spells\lodgen_native_baseline.sh'
S = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'snip_drop_mode.txt')
src = open(P, 'rb').read()
cr0 = src.count(b'\r')
snip = open(S, 'rb').read()
assert snip.count(b'\r') == 0


def rep(s, a, b):
    assert s.count(a) == 1, (a, s.count(a))
    return s.replace(a, b)


src = rep(src, b'#               baseline with one hex digit flipped must name exactly that file\n',
          b'#               baseline with one hex digit flipped must name exactly that file\n'
          b'#   --drop-proof  plan 5 row 13: bake (-32,0) dim 32 ONCE with --slot-fallback --native\n'
          b'#               and count the placements the stock chunk drops against the ones the\n'
          b'#               native pair keeps (the block above the region set says how)\n')
src = rep(src, b'--write|--check|--selftest\n', b'--write|--check|--selftest|--drop-proof\n')
src = rep(src, b'AOFLAG=""; [ "$AO" = "1" ] || AOFLAG="--no-ao"\n',
          b'AOFLAG=""; [ "$AO" = "1" ] || AOFLAG="--no-ao"\n' + snip)
assert src.count(b'\r') == cr0
open(P + '.tmp', 'wb').write(src)
os.replace(P + '.tmp', P)
print('patched, CR', cr0)
