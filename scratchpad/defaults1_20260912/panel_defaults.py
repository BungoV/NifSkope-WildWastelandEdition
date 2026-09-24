"""DEFAULTS1 -- the panel rows follow the four ruled defaults.

Refusing patch: every anchor must be found exactly once or nothing is written.
--check writes nothing. Line endings are measured with Python byte counts.
"""
import sys

P = r'E:/Projects/NifskopeWildWastelandEdition/src/lodgenmanager.cpp'
NL = chr(92) + 'n'          # the two characters backslash-n, as they sit in the C++ source

SUBS = [
    ('tr( "Hex tile size" ), 0.0, 0.0, 4096.0, 1, 16.0,',
     'tr( "Hex tile size" ), 256.0, 0.0, 4096.0, 1, 16.0,'),
    ('"0 is off and is the bake without it, byte for byte. One texture repeat is' + NL + '"',
     '"0 turns it off; 256 is the default (bungo 2026-09-12). One repeat is' + NL + '"'),
    ('tr( "Warp amplitude" ), 0.0, 0.0, 4096.0, 1, 16.0,',
     'tr( "Warp amplitude" ), 341.0, 0.0, 4096.0, 1, 16.0,'),
    ('"world units. 0 is off and is the bake without it, byte for byte.' + NL + '"',
     '"world units. 0 turns it off; 341 is the default (bungo 2026-09-12).' + NL + '"'),
    ('tr( "Mip bias" ), 0.0, -4.0, 4.0, 2, 0.10,',
     'tr( "Mip bias" ), -0.22, -4.0, 4.0, 2, 0.10,'),
    ('"is sharper. 0 is off and is the bake without it, byte for byte.' + NL + '"',
     '"is sharper. 0 turns it off; -0.22 is the default (bungo 2026-09-12).' + NL + '"'),
    ('tr( "Guide rule" ), 0,',
     'tr( "Guide rule" ), 5,'),
    ('"faces, the two warps push the sample along it. Off is the default and is' + NL + '"',
     '"faces, the two warps push the sample along it. Flat warp at strength 1 is' + NL + '"'),
    ('"the bake without it, byte for byte.' + NL + 'Command line: --land-guide" ) );',
     '"the default (bungo 2026-09-12); Off turns it off.' + NL + 'Command line: --land-guide" ) );'),
    ('tr( "Verge painted as road" ), 1.0, 0.0, 1.0, 2, 0.05,',
     'tr( "Verge painted as road" ), 0.0, 0.0, 1.0, 2, 0.05,'),
    ('"road; 0 leaves the landscape\'s own colour on the verge.' + NL + '"',
     '"road; 0 -- the default since 2026-09-12 -- leaves the landscape\'s own' + NL + '"\n'
     '\t\t\t\t\t"colour on the verge.' + NL + '"'),
]

raw = open(P, 'rb').read()
print('before  %d bytes  LF %d  CR %d' % (len(raw), raw.count(b'\n'), raw.count(b'\r')))
text = raw.decode('utf-8')
found = 0
for old, new in SUBS:
    c = text.count(old)
    if c != 1:
        print('REFUSED: anchor found %d times: %r' % (c, old[:70]))
        sys.exit(2)
    text = text.replace(old, new)
    found += 1
print('%d of %d anchors, each exactly once' % (found, len(SUBS)))
if '--check' in sys.argv:
    print('--check: nothing written')
    sys.exit(0)
out = text.encode('utf-8')
open(P, 'wb').write(out)
back = open(P, 'rb').read()
print('after   %d bytes  LF %d  CR %d' % (len(back), back.count(b'\n'), back.count(b'\r')))
assert back.count(b'\r') == 0, 'CR bytes appeared'
assert back == out, 'read-back differs'
print('ok')
