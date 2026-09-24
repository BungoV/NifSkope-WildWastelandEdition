# Lane BTOFREE1, 2026-09-16 -- patch 11.
#
# Check 4 of lodgen_native.sh now spells `--keep-bto`, because without it the
# FO4CS bake leaves no chunk and check 5 -- "the stock bake is byte-identical
# with and without --native" -- would have had no chunk to compare and would
# have gone quietly vacuous. Adding the switch made check 5 fail on ONE file:
#
#     DIFFER Commonwealth.lodb
#     26 stock files compared, 1 differ
#
# and that is the ledger telling the truth. `switches` is a sha1 over the
# argument vector with gLgSwitchSkip's tokens dropped; `--native` is on that
# skip list (it cannot make a tracked chunk output stale) and `--keep-bto`
# deliberately is NOT, because it decides what is on disk. So the two command
# lines check 5 compares now hash differently, by design.
#
# The fix is not to loosen the check. `.lodb` is not a stock output at all -- it
# is our ledger, and it records the command line that produced the bake. It
# comes out of the byte sweep and gets a leg that says exactly how the two
# ledgers differ: identical field for field once `switches` is removed, and
# `switches` moved. That is a stronger statement than `cmp` was making, and it
# would fail if the native bake had touched a single recorded chunk digest.
ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'


def patch(path, pairs):
    b = open(ROOT + path, 'rb').read()
    cr0, lf0, n0 = b.count(b'\r'), b.count(b'\n'), len(b)
    s = b.decode('utf-8')
    for old, new in pairs:
        n = s.count(old)
        assert n == 1, 'anchor count %d (want 1) in %s for: %r' % (n, path, old[:80])
        s = s.replace(old, new)
    nb = s.encode('utf-8')
    assert nb.count(b'\r') == cr0, 'CR count moved in %s' % path
    open(ROOT + path, 'wb').write(nb)
    print('%-40s %d -> %d bytes, CR %d -> %d, LF %d -> %d'
          % (path, n0, len(nb), cr0, nb.count(b'\r'), lf0, nb.count(b'\n')))


N = []
N.append((
'''d=0; n=0
for f in "$W"/stock/*; do
\tb="$(basename "$f")"; n=$((n+1))
\tcmp -s "$f" "$W/native/$b" || { echo "    DIFFER $b"; d=$((d+1)); }
done
echo "    $n stock files compared, $d differ"
[ "$d" -eq 0 ] && [ "$n" -gt 0 ] && note "the stock bake is byte-identical with and without --native" \\
\t|| bad "the stock bake is byte-identical with and without --native"
''',
'''d=0; n=0
for f in "$W"/stock/*; do
\tb="$(basename "$f")"
\t# THE LEDGER IS NOT A STOCK OUTPUT and it is compared field by field below
\t# instead. `.lodb` records the COMMAND LINE that made the bake: `switches`
\t# is a sha1 over the argument vector with gLgSwitchSkip's tokens dropped,
\t# `--native` is on that skip list and `--keep-bto` (which check 4 now
\t# spells, so that there ARE chunks here to compare) deliberately is not.
\t# The two lines therefore hash differently by design, and `cmp` can only
\t# say that they do.
\tcase "$b" in *.lodb) continue ;; esac
\tn=$((n+1))
\tcmp -s "$f" "$W/native/$b" || { echo "    DIFFER $b"; d=$((d+1)); }
done
echo "    $n stock files compared, $d differ (the .lodb ledger is compared below)"
[ "$d" -eq 0 ] && [ "$n" -gt 0 ] && note "the stock bake is byte-identical with and without --native" \\
\t|| bad "the stock bake is byte-identical with and without --native"
if [ -f "$W/stock/Commonwealth.lodb" ] && [ -f "$W/native/Commonwealth.lodb" ]; then
\tif "$PY" "$ROOT/tests/spells/lodgen_btofree_ledger.py" keep \\
\t\t"$W/stock/Commonwealth.lodb" "$W/native/Commonwealth.lodb"; then
\t\tnote "and the two ledgers differ ONLY in the command-line digest, every recorded chunk digest alike"
\telse bad "and the two ledgers differ ONLY in the command-line digest, every recorded chunk digest alike"; fi
else
\tbad "and the two ledgers differ ONLY in the command-line digest (a .lodb is missing)"
fi
''',
))
patch('tests/spells/lodgen_native.sh', N)

# The comparator's two `keep`-mode sentences named --keep-bto, which was true of
# its first caller and is not of its second. The mode still asks exactly the same
# two questions; only the wording moves.
C = []
C.append((
"""        check('the --keep-bto ledger differs from the rung ONLY in the command-line digest '""",
"""        check('the second ledger differs from the first ONLY in the command-line digest '""",
))
C.append((
'''  usage: lodgen_btofree_ledger.py keep <rung.lodb> <keep.lodb>
         lodgen_btofree_ledger.py drop <rung.lodb> <drop.lodb>''',
'''  usage: lodgen_btofree_ledger.py keep <a.lodb> <b.lodb>
         lodgen_btofree_ledger.py drop <rung.lodb> <drop.lodb>

`keep` is the general question "these two bakes recorded the same files, and
only the command line moved", asked by lodgen_btofree.sh leg (b) (rung vs
--keep-bto) and by lodgen_native.sh check 5 (stock vs --native --keep-bto).''',
))
patch('tests/spells/lodgen_btofree_ledger.py', C)
print('patch11 ok')
