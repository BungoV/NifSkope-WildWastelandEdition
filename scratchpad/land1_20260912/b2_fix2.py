"""Gate B2 must name the SAME plugin path gate B3's base ledger was written with.

The switch digest covers the argument vector, and the plugin path is one of its
arguments -- documented, deliberate, and the reason B3 bakes every variant over
ONE live plugin path. B2 named the vanilla Fallout4.esm instead, so every arm
tripped `the switches differ` before reaching the refusal it was testing.

Two of the five arms were failing on that. The `switches` arm was PASSING on it,
which is worse: it would have passed on a build where the switch digest was not
computed at all. An arm that passes for a reason it did not ask for is not
evidence, and this one only stood out once the per-arm phrase assertion made the
four messages readable side by side.

The live plugin is restored to a byte copy of the vanilla ESM first, because the
refs re-run left the edited variant in place.
"""
import sys

P = 'E:/Projects/NifskopeWildWastelandEdition/scratchpad/land1_20260912/b2_refusals.sh'

OLD = 'ESM="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"'
NEW = ('VANILLA="X:/Programs/Steam/steamapps/common/Fallout 4/Data/Fallout4.esm"\n'
       '# THE SAME PLUGIN PATH gate B3\'s base ledger was written with. The switch\n'
       '# digest covers the argument vector and the plugin path is one of its arguments,\n'
       '# so naming the vanilla path here made every arm refuse with "the switches\n'
       '# differ" -- two arms failed on it and, worse, the switches arm PASSED on it.\n'
       'ESM="$R/scratchpad/land1_20260912/out/b3/esm/A.esm"')

OLD2 = '[ -f "$GOOD/Commonwealth.lodb" ] || { echo "gate B2 needs gate B3\'s base bake first"; exit 2; }'
NEW2 = ('[ -f "$GOOD/Commonwealth.lodb" ] || { echo "gate B2 needs gate B3\'s base bake first"; exit 2; }\n'
        '# Restore the live plugin to the bytes the base ledger was written from: B3\'s\n'
        '# later arms copy edited variants over this path, and an edited plugin would\n'
        '# make the merge-ok control report dirty chunks for a real reason.\n'
        'cp -f "$VANILLA" "$ESM" || { echo "cannot restore $ESM"; exit 2; }')


def main():
    b = open(P, 'rb').read()
    s = b.decode('utf-8')
    for old, new, label in ((OLD, NEW, 'ESM'), (OLD2, NEW2, 'restore')):
        n = s.count(old)
        assert n == 1, '%s matched %d times' % (label, n)
        s = s.replace(old, new)
    out = s.encode('utf-8')
    open(P, 'wb').write(out)
    print('b2_refusals.sh %d -> %d bytes, CR %d' % (len(b), len(out), out.count(b'\x0d')))
    return 0


if __name__ == '__main__':
    sys.exit(main())
