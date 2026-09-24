"""DEFAULTS1: lodgen_native_baseline.sh's checked-in baseline was made with the
identity channels ON, and its profile header SAYS identity=1. Identity is off by
default since 2026-09-12, so the bakes have to ask for it or the header lies and
every hash moves. The profile string itself is not touched.
"""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/tests/spells/lodgen_native_baseline.sh'
EDITS = [
    ('\t"$NS" -no-gui lodgen "$ESM" --worldspace 3C --objects "$1" "$2" --dim "$3" $AOFLAG $4 \\',
     '\t# --identity is SPELLED: the profile header below says identity=1 and the\n'
     '\t# checked-in baseline was made that way, but identity became opt-in on\n'
     '\t# 2026-09-12 (bungo\'s ruling).\n'
     '\t"$NS" -no-gui lodgen "$ESM" --worldspace 3C --objects "$1" "$2" --dim "$3" $AOFLAG --identity $4 \\'),
    ('"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -19 25 --dim 4 $AOFLAG \\',
     '"$NS" -no-gui lodgen "$ESM" --worldspace 3C --terrain-region -20 24 -19 25 --dim 4 $AOFLAG --identity \\'),
]

raw = open(P, 'rb').read()
print('before %d B  LF %d  CR %d' % (len(raw), raw.count(b'\n'), raw.count(b'\r')))
text = raw.decode('utf-8')
for anchor, repl in EDITS:
    n = text.count(anchor)
    if n != 1:
        print('REFUSED: anchor %r found %d times' % (anchor[:60], n))
        sys.exit(2)
    text = text.replace(anchor, repl)
out = text.encode('utf-8')
assert out.count(b'\r') == 0
open(P, 'wb').write(out)
back = open(P, 'rb').read()
print('after  %d B  LF %d  CR %d' % (len(back), back.count(b'\n'), back.count(b'\r')))
print('ok')
