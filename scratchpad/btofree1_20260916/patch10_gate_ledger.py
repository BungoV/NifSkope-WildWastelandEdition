# Lane BTOFREE1, 2026-09-16 -- patch 10: the new gate's first run found the two
# places where "the rung's bytes" is not the right question, and both are the
# LEDGER. `cmp` on a `.lodb` cannot say WHY it moved; the ledger comparator can,
# so the file comes out of the tree sweep and gets legs of its own.
#
# Measured on the 17:11 exe, region -20 24 -19 25 dim 4:
#   (a) rung .lodb 716 B, default 647 B -- the .BTO row is gone and nothing else
#   (b) rung .lodb 716 B, --keep-bto 716 B, differing from char 632: the
#       `switches` digest, 6f9a266c179d -> deaac40aee43, because --keep-bto is
#       one more token on the command line. Every output file's own sha1 in both
#       ledgers is identical, including the manifest's.
ROOT = 'E:/Projects/NifskopeWildWastelandEdition/'


def patch(path, pairs):
    b = open(ROOT + path, 'rb').read()
    cr0, lf0, n0 = b.count(b'\r'), b.count(b'\n'), len(b)
    s = b.decode('utf-8')
    for old, new in pairs:
        n = s.count(old)
        assert n == 1, 'anchor count %d (want 1) in %s for: %r' % (n, path, old[:90])
        s = s.replace(old, new)
    nb = s.encode('utf-8')
    assert nb.count(b'\r') == cr0, 'CR count moved in %s: %d -> %d' % (path, cr0, nb.count(b'\r'))
    open(ROOT + path, 'wb').write(nb)
    print('%-38s %d -> %d bytes, CR %d -> %d, LF %d -> %d'
          % (path, n0, len(nb), cr0, nb.count(b'\r'), lf0, nb.count(b'\n')))


G = []

# the interpreter, beside the other paths
G.append((
'''DIM="${DIM:-4}"''',
'''DIM="${DIM:-4}"
PY="${PY:-$(command -v python || echo /c/Windows/py)}"
LEDGER="$ROOT/tests/spells/lodgen_btofree_ledger.py"''',
))

# (a): the ledger leaves the sweep and gets its own legs
G.append((
'''\techo "    everything except the chunks, against the rung:"
\ttreecmp "$W/rung_native" "$W/drop" '[.]BTO$\'''',
'''\t# THE LEDGER IS NOT SWEPT HERE, AND NOT BECAUSE IT IS INCONVENIENT.
\t# `.lodb` records what a bake LEFT ON DISK, so a bake that leaves no chunk
\t# must not record one -- digesting a file about to be deleted is the defect
\t# that once forced full rebakes. `cmp` can only say that it moved; the leg
\t# below says which rows moved and asserts that NOTHING else did, which is a
\t# stronger statement than the sweep was making.
\techo "    everything except the chunks and the ledger, against the rung:"
\ttreecmp "$W/rung_native" "$W/drop" '[.]BTO$|[.]lodb$\'''',
))

G.append((
'''\tfor f in nat/Commonwealth.lodo nat/Commonwealth.lodi; do
\t\tif [ -f "$W/rung_native/$f" ] && cmp -s "$W/rung_native/$f" "$W/drop/$f"; then
\t\t\tnote "(a) $(basename "$f") is byte-identical to the rung's ($(stat -c%s "$W/drop/$f") bytes)"
\t\telse bad "(a) $(basename "$f") is byte-identical to the rung's"; fi
\tdone''',
'''\tfor f in nat/Commonwealth.lodo nat/Commonwealth.lodi; do
\t\tif [ -f "$W/rung_native/$f" ] && cmp -s "$W/rung_native/$f" "$W/drop/$f"; then
\t\t\tnote "(a) $(basename "$f") is byte-identical to the rung's ($(stat -c%s "$W/drop/$f") bytes)"
\t\telse bad "(a) $(basename "$f") is byte-identical to the rung's"; fi
\tdone
\techo "    the ledger, row by row:"
\tif "$PY" "$LEDGER" drop "$W/rung_native/Commonwealth.lodb" "$W/drop/Commonwealth.lodb"; then
\t\tnote "(a) the ledger drops the .BTO row and keeps every other digest, the manifest's included"
\telse bad "(a) the ledger drops the .BTO row and keeps every other digest, the manifest's included"; fi''',
))

# (b): same treatment
G.append((
'''\ttreecmp "$W/rung_native" "$W/keep" -
\tif [ "$TC_DIFF" -eq 0 ] && [ "$TC_ONLYA" -eq 0 ] && [ "$TC_ONLYB" -eq 0 ] && [ "$TC_SAME" -gt 0 ]; then
\t\tnote "(b) the whole --keep-bto tree is byte-identical to the rung's ($TC_SAME files)"
\telse
\t\tbad "(b) the whole --keep-bto tree is byte-identical to the rung's ($TC_DIFF differ, $TC_ONLYA/$TC_ONLYB only on one side)"
\tfi''',
'''\t# the ledger again, and this time it MUST move: `--keep-bto` is one more
\t# token on the command line, the ledger digests that line, and an
\t# --incremental run has to refuse to reuse chunks a different line produced.
\ttreecmp "$W/rung_native" "$W/keep" '[.]lodb$'
\tif [ "$TC_DIFF" -eq 0 ] && [ "$TC_ONLYA" -eq 0 ] && [ "$TC_ONLYB" -eq 0 ] && [ "$TC_SAME" -gt 0 ]; then
\t\tnote "(b) every output file is byte-identical to the rung's ($TC_SAME files, the chunks included)"
\telse
\t\tbad "(b) every output file is byte-identical to the rung's ($TC_DIFF differ, $TC_ONLYA/$TC_ONLYB only on one side)"
\tfi
\techo "    the ledger, row by row:"
\tif "$PY" "$LEDGER" keep "$W/rung_native/Commonwealth.lodb" "$W/keep/Commonwealth.lodb"; then
\t\tnote "(b) the ledger differs only in the command-line digest, and that digest moved"
\telse bad "(b) the ledger differs only in the command-line digest, and that digest moved"; fi''',
))

patch('tests/spells/lodgen_btofree.sh', G)
print('patch10 ok')
